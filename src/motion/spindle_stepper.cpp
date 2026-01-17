#include "spindle_stepper.h"
#include "config_motion.h"
#include "dual_button_logic.h"
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
bool SpindleStepper::jog_active = false;
int8_t SpindleStepper::jog_dir = 0;

uint32_t SpindleStepper::last_update_us = 0;
uint32_t SpindleStepper::last_dt_us = 0;
uint32_t SpindleStepper::step_period_ticks = 0;
int32_t SpindleStepper::steps_per_sec = 0;
int64_t SpindleStepper::step_accumulator_fp = 0;
bool SpindleStepper::rmt_ready = false;

static constexpr int64_t FP_SCALE = 65536;

static DualButtonState g_spindle_btn_state;
static constexpr DualButtonConfig SPINDLE_BTN_CFG = {
    .long_press_ms = SPINDLE_JOG_PRESS_MS,
    .short_lockout_ms = 50,
    .overlap_debounce_ms = 20,
    .require_other_released_for_short = true,
};

// Min/max step period in RMT ticks (computed from config)
// At 10MHz: 1 tick = 0.1us. 3000 RPM with 1600 steps = 12.5us = 125 ticks
static constexpr uint32_t MIN_STEP_PERIOD_TICKS = (uint32_t)((uint64_t)SPINDLE_MIN_STEP_PERIOD_US * SPINDLE_RMT_RES_HZ / 1000000UL);
static constexpr uint32_t MAX_STEP_PERIOD_TICKS = (uint32_t)((uint64_t)SPINDLE_MAX_STEP_PERIOD_US * SPINDLE_RMT_RES_HZ / 1000000UL);

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
	pinMode(SPINDLE_FWD_PIN, INPUT_PULLUP);
	pinMode(SPINDLE_REV_PIN, INPUT_PULLUP);

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

    auto handle_short_press = [&](int8_t dir) {
        if (jog_active)
            return;
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
		int16_t jog_rpm = (SPINDLE_JOG_RPM > SPINDLE_MAX_RPM) ? (int16_t)SPINDLE_MAX_RPM : (int16_t)SPINDLE_JOG_RPM;
		current_rpm = (int16_t)(jog_rpm * 10);
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

	dualButtonUpdate(
		g_spindle_btn_state,
		SPINDLE_BTN_CFG,
		fwd_pressed,
		rev_pressed,
		now_ms,
		[]() { return false; },
		handle_short_press,
		[&](int8_t /*dir*/) { return !jog_active; },
		start_jog,
		stop_jog);
    
    // Get RPM x10 from MPG encoder (only when in RPM control mode)
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
    
    // Calculate max RPM×10 change for this update interval
	int32_t max_delta = ((SPINDLE_ACCEL_RPM_PER_SEC * 10) * (int32_t)dt_us) / 1000000;
    if (max_delta < 1) max_delta = 1;
    
    // Apply acceleration limiting
    int16_t rpm_target = 0;
    if (jog_active) {
		int16_t jog_rpm = (SPINDLE_JOG_RPM > SPINDLE_MAX_RPM) ? (int16_t)SPINDLE_MAX_RPM : (int16_t)SPINDLE_JOG_RPM;
		rpm_target = (int16_t)(jog_rpm * 10);
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
    
    // Calculate step period from RPM×10 directly in RMT ticks for full resolution
    // period_ticks = 60 * RMT_HZ / (RPM * STEPS_PER_REV)
    //              = 600 * RMT_HZ / (RPM×10 * STEPS_PER_REV)

    const int16_t min_rpm_x10 = (int16_t)(SPINDLE_MIN_RPM * 10);
    if (current_rpm < min_rpm_x10) {
        // Stopped or too slow
        step_period_ticks = 0;
        steps_per_sec = 0;
        running = false;
        step_accumulator_fp = 0;
    } else {
        const int64_t denom = (int64_t)current_rpm * (int64_t)SPINDLE_STEPS_PER_REV;
        if (denom <= 0) {
            step_period_ticks = 0;
            running = false;
            steps_per_sec = 0;
        } else {
            // Compute period directly in RMT ticks (full resolution, no µs quantization)
            // period_ticks = 600 * RMT_HZ / (RPM×10 * steps_per_rev)
            uint32_t unclamped_ticks = (uint32_t)(((int64_t)600 * (int64_t)SPINDLE_RMT_RES_HZ + (denom / 2)) / denom);
            step_period_ticks = unclamped_ticks;
            if (step_period_ticks < MIN_STEP_PERIOD_TICKS)
                step_period_ticks = MIN_STEP_PERIOD_TICKS;
            if (step_period_ticks > MAX_STEP_PERIOD_TICKS)
                step_period_ticks = MAX_STEP_PERIOD_TICKS;
            // Derive steps_per_sec from ticks
            steps_per_sec = (step_period_ticks > 0) ? (int32_t)(SPINDLE_RMT_RES_HZ / step_period_ticks) : 0;
            running = (steps_per_sec > 0);
        }
    }

    // Update public RPM based on the actual generated step period (in ticks).
    // rpm_x10 = 600 * RMT_HZ / (period_ticks * steps_per_rev)
    if (step_period_ticks > 0) {
        const int64_t denom_rpm = (int64_t)step_period_ticks * (int64_t)SPINDLE_STEPS_PER_REV;
        int16_t actual_rpm_x10 = 0;
        if (denom_rpm > 0) {
            actual_rpm_x10 = (int16_t)(((int64_t)600 * (int64_t)SPINDLE_RMT_RES_HZ + (denom_rpm / 2)) / denom_rpm);
        }
		rpm_abs = (actual_rpm_x10 < 0) ? (int16_t)-actual_rpm_x10 : actual_rpm_x10;
		rpm_signed = (int16_t)(actual_rpm_x10 * direction);
    } else {
        rpm_abs = 0;
        rpm_signed = 0;
    }
}

void SpindleStepper::updateRmtLoop() {
	static uint32_t prev_period_ticks = 0;
	static bool loop_active = false;
    static uint8_t prev_symbol_count = 0;

	const bool want_run = rmt_ready && running && direction != 0 && step_period_ticks > 0;

	if (want_run) {
		if (step_period_ticks != prev_period_ticks || !loop_active) {
			// Period is already in RMT ticks - use directly
			const uint32_t total_ticks = step_period_ticks;
			const uint32_t ticks_per_us = SPINDLE_RMT_RES_HZ / 1000000UL;
			uint32_t high_ticks = SPINDLE_PULSE_US * ticks_per_us;
			if (high_ticks < 1) high_ticks = 1;
			if (high_ticks >= total_ticks) high_ticks = total_ticks - 1;
			uint32_t low_ticks_total = total_ticks - high_ticks;
			if (low_ticks_total < 1) low_ticks_total = 1;
			if (high_ticks > 32767) high_ticks = 32767;

			// With 0.1us (10MHz) ticks, low duration can exceed 32767 ticks even at moderate RPM.
			// Split the low time across additional symbols.
			// 1 RPM = 37.5ms period = ~375,000 ticks. 32k per symbol -> ~12 symbols. Keep safe margin.
			static rmt_data_t symbols[24];
			uint8_t symbol_count = 0;

			uint32_t low_ticks0 = low_ticks_total;
			if (low_ticks0 > 32767) low_ticks0 = 32767;
			symbols[0] = {};
			symbols[0].level0 = 1;
			symbols[0].duration0 = (uint16_t)high_ticks;
			symbols[0].level1 = 0;
			symbols[0].duration1 = (uint16_t)low_ticks0;
			symbol_count = 1;

			uint32_t remaining = low_ticks_total - low_ticks0;
			while (remaining > 0 && symbol_count < (uint8_t)(sizeof(symbols) / sizeof(symbols[0]))) {
				uint32_t slice = remaining;
				if (slice > 32767) slice = 32767;
				symbols[symbol_count] = {};
				// Keep the line low for the entire symbol.
				symbols[symbol_count].level0 = 0;
				symbols[symbol_count].duration0 = (uint16_t)slice;
				symbols[symbol_count].level1 = 0;
				symbols[symbol_count].duration1 = 0;
				symbol_count++;
				remaining -= slice;
			}

			if (symbol_count != prev_symbol_count || step_period_ticks != prev_period_ticks || !loop_active) {
				rmtWriteLooping(SPINDLE_STEP_PIN, symbols, symbol_count);
				prev_symbol_count = symbol_count;
			}
			loop_active = true;
			prev_period_ticks = step_period_ticks;
		}

		if (SPINDLE_EN_PIN >= 0) {
			digitalWrite(SPINDLE_EN_PIN, LOW);  // Active low enable
		}
	} else {
		if (loop_active) {
			rmtWriteLooping(SPINDLE_STEP_PIN, nullptr, 0);
			loop_active = false;
			prev_period_ticks = 0;
            prev_symbol_count = 0;
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
        // Convert pulse width from microseconds to RMT ticks
        const uint32_t ticks_per_us = SPINDLE_RMT_RES_HZ / 1000000UL;
        uint32_t pulse_ticks = SPINDLE_PULSE_US * ticks_per_us;
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
	step_period_ticks = 0;
	steps_per_sec = 0;
	step_accumulator_fp = 0;
	updateRmtLoop();
    
    if (SPINDLE_EN_PIN >= 0) {
        digitalWrite(SPINDLE_EN_PIN, HIGH);  // Disabled
    }
    
    Serial.println("[SpindleStepper] Emergency stop");
}
