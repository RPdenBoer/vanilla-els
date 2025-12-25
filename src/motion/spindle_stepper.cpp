#include "spindle_stepper.h"
#include "config_motion.h"
#include "mpg_encoder.h"
#include <Arduino.h>
#include "esp32-hal-rmt.h"

// ============================================================================
// Static member initialization
// ============================================================================
volatile int32_t SpindleStepper::position = 0;
int16_t SpindleStepper::rpm_signed = 0;
int16_t SpindleStepper::rpm_abs = 0;
int16_t SpindleStepper::target_rpm = 0;
int16_t SpindleStepper::current_rpm = 0;
int8_t SpindleStepper::direction = 0;
bool SpindleStepper::running = false;

uint32_t SpindleStepper::last_update_us = 0;
uint32_t SpindleStepper::last_dt_us = 0;
uint32_t SpindleStepper::step_period_us = 0;
int32_t SpindleStepper::steps_per_sec = 0;
int64_t SpindleStepper::step_accumulator_fp = 0;
bool SpindleStepper::rmt_ready = false;

static constexpr int64_t FP_SCALE = 65536;

// ============================================================================
// Initialization
// ============================================================================
bool SpindleStepper::init() {
    // Configure GPIO pins
    pinMode(SPINDLE_STEP_PIN, OUTPUT);
    pinMode(SPINDLE_DIR_PIN, OUTPUT);
    digitalWrite(SPINDLE_STEP_PIN, LOW);
    digitalWrite(SPINDLE_DIR_PIN, LOW);
    
    if (SPINDLE_EN_PIN >= 0) {
        pinMode(SPINDLE_EN_PIN, OUTPUT);
        digitalWrite(SPINDLE_EN_PIN, HIGH);  // Disabled initially (active low assumed)
    }
    
    // Configure direction switch inputs
    // Note: GPIO 36/39 are input-only and don't have internal pullups.
    pinMode(SPINDLE_FWD_PIN, INPUT);
    pinMode(SPINDLE_REV_PIN, INPUT);
    
    // Note: MPG encoder is initialized separately in main.cpp
    
    // Initialize RMT for step generation
    rmt_ready = rmtInit(SPINDLE_STEP_PIN, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, SPINDLE_RMT_RES_HZ);
    if (!rmt_ready) {
        Serial.println("[SpindleStepper] RMT init failed");
        return false;
    }
    rmtSetEOT(SPINDLE_STEP_PIN, 0);

    last_update_us = micros();
    
    Serial.printf("[SpindleStepper] Initialized: %ld steps/rev, max %ld RPM\n",
        SPINDLE_STEPS_PER_REV, SPINDLE_MAX_RPM);
    
    return true;
}

// ============================================================================
// Read control inputs (MPG encoder and direction switch)
// ============================================================================
void SpindleStepper::readControls() {
    // Read direction switch (active LOW)
    bool fwd = (digitalRead(SPINDLE_FWD_PIN) == LOW);
    bool rev = (digitalRead(SPINDLE_REV_PIN) == LOW);
    
    // Determine direction (both on = invalid, treat as stop)
    if (fwd && !rev) {
        direction = 1;
    } else if (rev && !fwd) {
        direction = -1;
    } else {
        direction = 0;
    }
    
    // Get RPM from MPG encoder (only when in RPM control mode)
    if (MpgEncoder::getMode() == MpgMode::RPM_CONTROL) {
        target_rpm = MpgEncoder::getRpmSetting();
    }
    // When in jog mode, target_rpm stays at last value (spindle keeps running)
    
    // If direction is off, target is 0 (for acceleration ramping)
    if (direction == 0) {
        // Don't reset target_rpm - it represents the "set" RPM
        // Just let the spindle stop via acceleration control
    }
}

// ============================================================================
// Update speed with acceleration limiting
// ============================================================================
void SpindleStepper::updateSpeed() {
    uint32_t now_us = micros();
    uint32_t dt_us = now_us - last_update_us;
    last_update_us = now_us;
	last_dt_us = dt_us;
    
    // Calculate max RPM change for this update interval
    int32_t max_delta = (SPINDLE_ACCEL_RPM_PER_SEC * (int32_t)dt_us) / 1000000;
    if (max_delta < 1) max_delta = 1;
    
    // Apply acceleration limiting
    int16_t rpm_target = (direction == 0) ? 0 : target_rpm;
    
    if (current_rpm < rpm_target) {
        current_rpm += max_delta;
        if (current_rpm > rpm_target) current_rpm = rpm_target;
    } else if (current_rpm > rpm_target) {
        current_rpm -= max_delta;
        if (current_rpm < rpm_target) current_rpm = rpm_target;
    }
    
    // Calculate step period from RPM
    // steps_per_sec = (RPM / 60) * STEPS_PER_REV
    // period_us = 1000000 / steps_per_sec
    
    if (current_rpm < SPINDLE_MIN_RPM) {
        // Stopped or too slow
        step_period_us = 0;
        steps_per_sec = 0;
        running = false;
        step_accumulator_fp = 0;
    } else {
        steps_per_sec = ((int32_t)current_rpm * SPINDLE_STEPS_PER_REV) / 60;
        if (steps_per_sec <= 0) {
            step_period_us = 0;
            running = false;
        } else {
            uint32_t unclamped_period_us = 1000000UL / (uint32_t)steps_per_sec;
            step_period_us = unclamped_period_us;
            if (step_period_us < SPINDLE_MIN_STEP_PERIOD_US)
                step_period_us = SPINDLE_MIN_STEP_PERIOD_US;
            if (step_period_us > SPINDLE_MAX_STEP_PERIOD_US)
                step_period_us = SPINDLE_MAX_STEP_PERIOD_US;
            if (step_period_us != unclamped_period_us) {
                steps_per_sec = (int32_t)(1000000UL / step_period_us);
            }
            running = true;
        }
    }

    // Update public RPM based on actual step rate
    if (steps_per_sec > 0) {
        int16_t actual_rpm = (int16_t)((steps_per_sec * 60) / SPINDLE_STEPS_PER_REV);
        rpm_abs = actual_rpm;
        rpm_signed = actual_rpm * direction;
    } else {
        rpm_abs = 0;
        rpm_signed = 0;
    }
}

void SpindleStepper::updateRmtLoop() {
	static uint32_t prev_period_us = 0;
	static bool loop_active = false;

	const bool want_run = rmt_ready && running && direction != 0 && step_period_us > 0;

	if (want_run) {
		if (step_period_us != prev_period_us || !loop_active) {
			const uint32_t tick_us = 1000000UL / SPINDLE_RMT_RES_HZ;
			uint32_t high_us = SPINDLE_PULSE_US;
			if (high_us < tick_us) high_us = tick_us;
			if (high_us >= step_period_us) high_us = step_period_us - tick_us;
			uint32_t low_us = step_period_us - high_us;

			uint32_t high_ticks = (high_us + tick_us - 1) / tick_us;
			uint32_t low_ticks = (low_us + tick_us - 1) / tick_us;
			if (high_ticks < 1) high_ticks = 1;
			if (low_ticks < 1) low_ticks = 1;
			if (high_ticks > 32767) high_ticks = 32767;
			if (low_ticks > 32767) low_ticks = 32767;

			rmt_data_t symbol = {};
			symbol.level0 = 1;
			symbol.duration0 = (uint16_t)high_ticks;
			symbol.level1 = 0;
			symbol.duration1 = (uint16_t)low_ticks;

			rmtWriteLooping(SPINDLE_STEP_PIN, &symbol, 1);
			loop_active = true;
			prev_period_us = step_period_us;
		}

		if (SPINDLE_EN_PIN >= 0) {
			digitalWrite(SPINDLE_EN_PIN, LOW);  // Active low enable
		}
	} else {
		if (loop_active) {
			rmtWriteLooping(SPINDLE_STEP_PIN, nullptr, 0);
			loop_active = false;
			prev_period_us = 0;
		}

		if (SPINDLE_EN_PIN >= 0) {
			digitalWrite(SPINDLE_EN_PIN, HIGH);  // Disabled
		}
	}
}

void SpindleStepper::accumulatePosition() {
	if (!rmt_ready || direction == 0 || steps_per_sec <= 0 || last_dt_us == 0) {
		step_accumulator_fp = 0;
		return;
	}

	int64_t delta_fp = (int64_t)steps_per_sec * (int64_t)last_dt_us * FP_SCALE / 1000000;
	step_accumulator_fp += delta_fp;

	int32_t steps = (int32_t)(step_accumulator_fp / FP_SCALE);
	if (steps != 0) {
		step_accumulator_fp -= (int64_t)steps * FP_SCALE;
		position += (direction > 0) ? steps : -steps;
	}
}

// ============================================================================
// Main update - call from loop
// ============================================================================
void SpindleStepper::update() {
    // Read control inputs
    readControls();
    
    // Update speed with acceleration
    updateSpeed();
    
    // Set direction pin
    bool dir_level = (direction >= 0);
    if (SPINDLE_INVERT_DIR) dir_level = !dir_level;
    digitalWrite(SPINDLE_DIR_PIN, dir_level ? HIGH : LOW);

    updateRmtLoop();
    accumulatePosition();
}

// ============================================================================
// Emergency stop
// ============================================================================
void SpindleStepper::stop() {
    running = false;
    current_rpm = 0;
    rpm_signed = 0;
    rpm_abs = 0;
    target_rpm = 0;
    direction = 0;
	step_period_us = 0;
	steps_per_sec = 0;
	step_accumulator_fp = 0;
	updateRmtLoop();
    
    if (SPINDLE_EN_PIN >= 0) {
        digitalWrite(SPINDLE_EN_PIN, HIGH);  // Disabled
    }
    
    Serial.println("[SpindleStepper] Emergency stop");
}
