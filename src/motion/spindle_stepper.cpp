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
volatile int8_t SpindleStepper::pending_soft_toggle_dir = 0;
bool SpindleStepper::prev_fwd_pressed = false;
bool SpindleStepper::prev_rev_pressed = false;
bool SpindleStepper::fwd_long_handled = false;
bool SpindleStepper::rev_long_handled = false;
uint32_t SpindleStepper::fwd_down_ms = 0;
uint32_t SpindleStepper::rev_down_ms = 0;
uint32_t SpindleStepper::last_toggle_ms = 0;
bool SpindleStepper::jog_active = false;
int8_t SpindleStepper::jog_dir = 0;

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

void SpindleStepper::queueSoftToggle(int8_t dir) {
    if (dir > 0) {
        pending_soft_toggle_dir = 1;
    } else if (dir < 0) {
        pending_soft_toggle_dir = -1;
    }
}

// ============================================================================
// Read control inputs (MPG encoder and direction switch)
// ============================================================================
void SpindleStepper::readControls() {
    const uint32_t now_ms = millis();

    // Read momentary direction buttons (active LOW)
    const bool fwd_pressed = (digitalRead(SPINDLE_FWD_PIN) == LOW);
    const bool rev_pressed = (digitalRead(SPINDLE_REV_PIN) == LOW);

    const bool fwd_edge = fwd_pressed && !prev_fwd_pressed;
    const bool rev_edge = rev_pressed && !prev_rev_pressed;
    const bool fwd_released = !fwd_pressed && prev_fwd_pressed;
    const bool rev_released = !rev_pressed && prev_rev_pressed;

    auto handle_short_press = [&](int8_t dir) {
        if (jog_active)
            return;
        if ((now_ms - last_toggle_ms) < 50)
            return;
        last_toggle_ms = now_ms;
        if (direction != 0) {
            direction = 0;
        } else {
            direction = (dir > 0) ? 1 : -1;
        }
    };

    auto start_jog = [&](int8_t dir) {
        jog_active = true;
        jog_dir = (dir > 0) ? 1 : -1;
        direction = jog_dir;
        current_rpm = (SPINDLE_JOG_RPM > SPINDLE_MAX_RPM) ? SPINDLE_MAX_RPM : (int16_t)SPINDLE_JOG_RPM;
    };

    auto stop_jog = [&](int8_t dir) {
        if (!jog_active || jog_dir != dir)
            return;
        jog_active = false;
        jog_dir = 0;
        direction = 0;
        current_rpm = 0;
    };

    int8_t soft_dir = pending_soft_toggle_dir;
    if (soft_dir != 0) {
        pending_soft_toggle_dir = 0;
        handle_short_press(soft_dir);
    }

    if (fwd_edge) {
        fwd_down_ms = now_ms;
        fwd_long_handled = false;
    }
    if (rev_edge) {
        rev_down_ms = now_ms;
        rev_long_handled = false;
    }

    if (!jog_active && fwd_pressed && !rev_pressed && !fwd_long_handled &&
        (now_ms - fwd_down_ms >= SPINDLE_JOG_PRESS_MS)) {
        start_jog(1);
        fwd_long_handled = true;
    }
    if (!jog_active && rev_pressed && !fwd_pressed && !rev_long_handled &&
        (now_ms - rev_down_ms >= SPINDLE_JOG_PRESS_MS)) {
        start_jog(-1);
        rev_long_handled = true;
    }

    if (fwd_released) {
        if (fwd_long_handled) {
            stop_jog(1);
        } else {
            handle_short_press(1);
        }
    }
    if (rev_released) {
        if (rev_long_handled) {
            stop_jog(-1);
        } else {
            handle_short_press(-1);
        }
    }

    prev_fwd_pressed = fwd_pressed;
    prev_rev_pressed = rev_pressed;
    
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
    int16_t rpm_target = 0;
    if (jog_active) {
        rpm_target = (SPINDLE_JOG_RPM > SPINDLE_MAX_RPM) ? SPINDLE_MAX_RPM : (int16_t)SPINDLE_JOG_RPM;
        if (rpm_target < 0) rpm_target = 0;
        current_rpm = rpm_target;
    } else {
        rpm_target = (direction == 0) ? 0 : target_rpm;
        if (current_rpm < rpm_target) {
            current_rpm += max_delta;
            if (current_rpm > rpm_target) current_rpm = rpm_target;
        } else if (current_rpm > rpm_target) {
            current_rpm -= max_delta;
            if (current_rpm < rpm_target) current_rpm = rpm_target;
        }
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
// Immediate stepping for MPG jog mode
// ============================================================================
void SpindleStepper::stepImmediate(int32_t count) {
    if (count == 0 || !rmt_ready) return;
    
    // Stop continuous loop if running
    if (running) {
        rmtWriteLooping(SPINDLE_STEP_PIN, nullptr, 0);
    }
    
    // Set direction
    bool forward = (count > 0);
    bool dir_level = forward;
    if (SPINDLE_INVERT_DIR) dir_level = !dir_level;
    digitalWrite(SPINDLE_DIR_PIN, dir_level ? HIGH : LOW);
    delayMicroseconds(2);  // Direction settle time
    
    int32_t steps = abs(count);
    
    // Limit steps per call
    if (steps > 100) steps = 100;
    
    // Build RMT pulse data
    static rmt_data_t rmt_buf[100];
    static bool buf_init = false;
    if (!buf_init) {
        const uint32_t tick_us = 1000000UL / SPINDLE_RMT_RES_HZ;
        uint32_t pulse_ticks = (SPINDLE_PULSE_US + tick_us - 1) / tick_us;
        if (pulse_ticks < 1) pulse_ticks = 1;
        rmt_data_t pulse = {};
        pulse.level0 = 1;
        pulse.duration0 = (uint16_t)pulse_ticks;
        pulse.level1 = 0;
        pulse.duration1 = (uint16_t)pulse_ticks;
        for (int i = 0; i < 100; i++) {
            rmt_buf[i] = pulse;
        }
        buf_init = true;
    }
    
    // Output steps
    rmtWrite(SPINDLE_STEP_PIN, rmt_buf, (size_t)steps, RMT_WAIT_FOR_EVER);
    
    // Update position
    position += forward ? steps : -steps;
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
	jog_active = false;
	jog_dir = 0;
	pending_soft_toggle_dir = 0;
	step_period_us = 0;
	steps_per_sec = 0;
	step_accumulator_fp = 0;
	updateRmtLoop();
    
    if (SPINDLE_EN_PIN >= 0) {
        digitalWrite(SPINDLE_EN_PIN, HIGH);  // Disabled
    }
    
    Serial.println("[SpindleStepper] Emergency stop");
}
