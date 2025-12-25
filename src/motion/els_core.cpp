#include "els_core.h"
#include "config_motion.h"
#include "encoder_motion.h"
#include "stepper.h"
#include <Arduino.h>

#if SPINDLE_MODE == SPINDLE_MODE_STEPPER
#include "spindle_stepper.h"
#endif

// ============================================================================
// Spindle position abstraction - works for both encoder and stepper modes
// ============================================================================
static inline int32_t getSpindlePosition()
{
#if SPINDLE_MODE == SPINDLE_MODE_ENCODER
	return EncoderMotion::getSpindleCount();
#else
	return SpindleStepper::getPosition();
#endif
}

// Static member initialization
bool ElsCore::enabled = false;
bool ElsCore::fault = false;
bool ElsCore::endstop_triggered = false;

int32_t ElsCore::pitch_um = 1000;  // Default 1mm pitch
int8_t ElsCore::direction_mul = 1;

bool ElsCore::sync_enabled = false;
bool ElsCore::sync_waiting = false;
bool ElsCore::sync_in = false;
int32_t ElsCore::sync_z_um = 0;
int32_t ElsCore::sync_phase_ticks = 0;
int32_t ElsCore::sync_tolerance_out_um = 25;
int32_t ElsCore::sync_ref_z_um = 0;
int32_t ElsCore::sync_ref_spindle = 0;
int32_t ElsCore::last_z_um = 0;
volatile bool ElsCore::jog_active = false;
volatile int8_t ElsCore::jog_dir = 0;
bool ElsCore::jog_prev_active = false;
uint32_t ElsCore::jog_last_us = 0;
int64_t ElsCore::jog_step_accumulator = 0;

int32_t ElsCore::endstop_min_um = INT32_MIN;
int32_t ElsCore::endstop_max_um = INT32_MAX;
bool ElsCore::endstop_min_enabled = false;
bool ElsCore::endstop_max_enabled = false;

int32_t ElsCore::last_spindle_count = 0;
int64_t ElsCore::step_accumulator = 0;

// Fixed-point scale for sub-step precision (16 fractional bits)
static constexpr int64_t FP_SCALE = 65536;
static constexpr int64_t JOG_STEPS_PER_US_FP =
	(int64_t)ELS_JOG_MM_PER_MIN * 1000LL * (int64_t)ELS_STEPS_PER_REV * FP_SCALE /
	(60LL * 1000000LL * (int64_t)ELS_LEADSCREW_PITCH_UM);

static inline int32_t wrap_phase(int32_t count) {
	int32_t r = count % C_COUNTS_PER_REV;
	if (r < 0) r += C_COUNTS_PER_REV;
	return r;
}

static inline bool crossed_phase(int32_t prev, int32_t curr, int32_t target_phase) {
	const int32_t delta = curr - prev;
	if (delta == 0) return false;
	if (delta >= C_COUNTS_PER_REV || delta <= -C_COUNTS_PER_REV) return true;

	const int32_t prev_phase = wrap_phase(prev);
	const int32_t curr_phase = wrap_phase(curr);
	const int32_t target = wrap_phase(target_phase);

	if (prev_phase == target || curr_phase == target) return true;

	if (delta > 0) {
		if (prev_phase < curr_phase) return (target > prev_phase && target < curr_phase);
		return (target > prev_phase || target < curr_phase);
	}

	if (prev_phase > curr_phase) return (target < prev_phase && target > curr_phase);
	return (target < prev_phase || target > curr_phase);
}

void ElsCore::init() {
    enabled = false;
    fault = false;
    endstop_triggered = false;
	last_spindle_count = getSpindlePosition();
	step_accumulator = 0;
	sync_enabled = false;
	sync_waiting = false;
	sync_in = false;
	sync_ref_z_um = 0;
	sync_ref_spindle = 0;
	last_z_um = EncoderMotion::getZCount() * Z_UM_PER_COUNT;
	jog_active = false;
	jog_dir = 0;
	jog_prev_active = false;
	jog_last_us = 0;
	jog_step_accumulator = 0;
}

void ElsCore::setEnabled(bool on) {
    if (on && !enabled) {
        // Enabling: sync to current spindle position
		last_spindle_count = getSpindlePosition();
		step_accumulator = 0;
        fault = false;
        endstop_triggered = false;
		last_z_um = EncoderMotion::getZCount() * Z_UM_PER_COUNT;
    }
    enabled = on;
	if (!enabled) {
		sync_waiting = false;
		sync_in = false;
	} else if (sync_enabled) {
		sync_waiting = true;
		sync_in = false;
	}
}

void ElsCore::setPitchUm(int32_t pitch) {
    if (pitch == pitch_um) return;
    pitch_um = pitch;
	if (sync_enabled && enabled) {
		sync_waiting = true;
		sync_in = false;
	}
}

void ElsCore::setDirectionMul(int8_t mul) {
    int8_t new_mul = (mul < 0) ? -1 : 1;
	if (new_mul == direction_mul) return;
    direction_mul = new_mul;
	if (sync_enabled && enabled) {
		sync_waiting = true;
		sync_in = false;
	}
}

void ElsCore::setJog(int8_t dir, bool active)
{
	int8_t new_dir = 0;
	if (active)
	{
		if (dir > 0) new_dir = 1;
		else if (dir < 0) new_dir = -1;
	}
	jog_dir = new_dir;
	jog_active = active && (new_dir != 0);
}

void ElsCore::setSync(bool enabled, int32_t z_um, int32_t c_ticks) {
	const bool was_enabled = sync_enabled;
	const int32_t prev_z = sync_z_um;
	const int32_t prev_phase = sync_phase_ticks;
	sync_enabled = enabled;
	sync_z_um = z_um;
	sync_phase_ticks = c_ticks;
	if (!sync_enabled) {
		sync_waiting = false;
		sync_in = false;
		return;
	}
	if (!was_enabled || prev_z != z_um || prev_phase != c_ticks) {
		sync_in = false;
		sync_waiting = ElsCore::enabled;
	}
}

void ElsCore::setEndstops(int32_t min_um, int32_t max_um, bool min_en, bool max_en) {
    endstop_min_um = min_um;
    endstop_max_um = max_um;
    endstop_min_enabled = min_en;
    endstop_max_enabled = max_en;
}

bool ElsCore::checkEndstops(int32_t z_um) {
    if (endstop_min_enabled && z_um < endstop_min_um) {
        return false;  // Out of bounds
    }
    if (endstop_max_enabled && z_um > endstop_max_um) {
        return false;  // Out of bounds
    }
    return true;  // In bounds
}

void ElsCore::update() {
	const bool jog_active_now = jog_active;
	const int8_t jog_dir_now = jog_dir;
	if (jog_active_now && jog_dir_now != 0)
	{
		if (!jog_prev_active)
		{
			jog_prev_active = true;
			jog_last_us = micros();
			jog_step_accumulator = 0;
			last_spindle_count = getSpindlePosition();
			last_z_um = EncoderMotion::getZCount() * Z_UM_PER_COUNT;
			step_accumulator = 0;
			if (sync_enabled)
			{
				sync_waiting = true;
				sync_in = false;
			}
		}

		const uint32_t now_us = micros();
		const uint32_t dt_us = now_us - jog_last_us;
		jog_last_us = now_us;

		if (dt_us > 0 && JOG_STEPS_PER_US_FP > 0)
		{
			const int64_t step_delta_fp = (int64_t)dt_us * JOG_STEPS_PER_US_FP;
			jog_step_accumulator += step_delta_fp;

			int32_t steps_to_output = (int32_t)(jog_step_accumulator / FP_SCALE);
			if (steps_to_output != 0)
			{
				if (steps_to_output > ELS_MAX_STEPS_PER_CYCLE)
					steps_to_output = ELS_MAX_STEPS_PER_CYCLE;
				jog_step_accumulator -= (int64_t)steps_to_output * FP_SCALE;
				Stepper::step(steps_to_output * (int32_t)jog_dir_now);
			}
		}
		return;
	}

	if (jog_prev_active)
	{
		jog_prev_active = false;
		jog_step_accumulator = 0;
		last_spindle_count = getSpindlePosition();
		last_z_um = EncoderMotion::getZCount() * Z_UM_PER_COUNT;
		step_accumulator = 0;
		if (sync_enabled && enabled)
		{
			sync_waiting = true;
			sync_in = false;
		}
	}

    if (!enabled) return;

	// Get current spindle position (from encoder or stepper, depending on mode)
	int32_t spindle_count = getSpindlePosition();
	const int32_t z_um = EncoderMotion::getZCount() * Z_UM_PER_COUNT;

	if (sync_enabled) {
		if (sync_waiting) {
			if (pitch_um == 0) {
				sync_waiting = false;
				sync_in = false;
			} else {
				const int64_t phase_num = (int64_t)(z_um - sync_z_um) *
										  (int64_t)C_COUNTS_PER_REV *
										  (int64_t)direction_mul;
				const int32_t phase_delta = (int32_t)(phase_num / (int64_t)pitch_um);
				const int32_t target_phase = wrap_phase(sync_phase_ticks + phase_delta);
				const int32_t current_phase = wrap_phase(spindle_count);
				if (current_phase != target_phase &&
					!crossed_phase(last_spindle_count, spindle_count, target_phase)) {
					last_spindle_count = spindle_count;
					last_z_um = z_um;
					return;
				}
				sync_waiting = false;
				sync_in = true;
				sync_ref_z_um = z_um;
				sync_ref_spindle = spindle_count;
				last_spindle_count = spindle_count;
				step_accumulator = 0;
			}
		} else if (!sync_in) {
			sync_waiting = true;
			last_spindle_count = spindle_count;
			last_z_um = z_um;
			step_accumulator = 0;
			return;
		}

		if (sync_in) {
			const int32_t spindle_delta = spindle_count - sync_ref_spindle;
			const int64_t numerator = (int64_t)spindle_delta * (int64_t)pitch_um * (int64_t)direction_mul;
			const int32_t expected_z = sync_ref_z_um + (int32_t)(numerator / (int64_t)C_COUNTS_PER_REV);
			const int32_t err = z_um - expected_z;
			const int32_t abs_err = (err < 0) ? -err : err;
			if (abs_err > sync_tolerance_out_um) {
				sync_in = false;
				sync_waiting = true;
				last_spindle_count = spindle_count;
				last_z_um = z_um;
				step_accumulator = 0;
				return;
			}
		}
	}

	int32_t spindle_delta = spindle_count - last_spindle_count;
    last_spindle_count = spindle_count;
	last_z_um = z_um;
    
    if (spindle_delta == 0) return;
    
#if DEBUG_SPI_LOGGING
    // Debug: log spindle movement
    static uint32_t last_debug_ms = 0;
    static int32_t total_spindle_delta = 0;
    static int32_t total_steps_output = 0;
    total_spindle_delta += spindle_delta;
    
    if (millis() - last_debug_ms > 1000) {
        Serial.printf("[ELS] pitch=%ld um, spindle_delta=%ld, steps_out=%ld\n",
            pitch_um, total_spindle_delta, total_steps_output);
        total_spindle_delta = 0;
        total_steps_output = 0;
        last_debug_ms = millis();
    }
#endif
    
    // Check endstops before moving
    if (!checkEndstops(z_um)) {
        enabled = false;
        fault = true;
        endstop_triggered = true;
        return;
    }
    
    // Calculate required Z movement in microns
    // spindle_delta counts / C_COUNTS_PER_REV = revolutions
    // revolutions * pitch_um = microns to travel
    // microns / ELS_LEADSCREW_PITCH_UM * ELS_STEPS_PER_REV = steps
    
    // Use fixed-point math for precision:
    // steps = (spindle_delta * pitch_um * ELS_STEPS_PER_REV * direction_mul) 
    //         / (C_COUNTS_PER_REV * ELS_LEADSCREW_PITCH_UM)
    
    int64_t numerator = (int64_t)spindle_delta * (int64_t)pitch_um * 
                        (int64_t)ELS_STEPS_PER_REV * (int64_t)direction_mul * FP_SCALE;
    int64_t denominator = (int64_t)C_COUNTS_PER_REV * (int64_t)ELS_LEADSCREW_PITCH_UM;
    
    int64_t step_delta_fp = numerator / denominator;
    
    // Accumulate fractional steps
    step_accumulator += step_delta_fp;
    
    // Extract whole steps
    int32_t steps_to_output = (int32_t)(step_accumulator / FP_SCALE);
    
    if (steps_to_output != 0) {
        // Remove outputted steps from accumulator
        step_accumulator -= (int64_t)steps_to_output * FP_SCALE;
        
#if DEBUG_SPI_LOGGING
        total_steps_output += abs(steps_to_output);
#endif
        
        // Output steps
        Stepper::step(steps_to_output);
    }
}
