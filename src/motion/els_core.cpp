#include "els_core.h"
#include "config_motion.h"
#include "encoder_motion.h"
#include "stepper.h"
#include "spindle_stepper.h"
#include <Arduino.h>

// ============================================================================
// Spindle position abstraction
// ============================================================================
static inline int32_t getSpindlePosition()
{
	return SpindleStepper::getPosition();
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
int32_t ElsCore::sync_tolerance_in_ticks = 8;  // ~1.8 degrees tolerance for sync acquisition
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

// Check if two phases are within tolerance (handles wrap-around)
static inline bool phase_within_tolerance(int32_t phase_a, int32_t phase_b, int32_t tolerance) {
	int32_t diff = phase_a - phase_b;
	if (diff < 0) diff = -diff;
	// Handle wrap-around: if diff > half revolution, measure the other way
	if (diff > C_COUNTS_PER_REV / 2) diff = C_COUNTS_PER_REV - diff;
	return diff <= tolerance;
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
    const bool was_enabled = enabled;
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
	} else if (sync_enabled && !was_enabled) {
		// Only reset sync when transitioning from disabled to enabled
		sync_waiting = true;
		sync_in = false;
	}
}

void ElsCore::setPitchUm(int32_t pitch) {
    if (pitch == pitch_um) return;
    pitch_um = pitch;
    // Pitch changed - need to re-sync
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
	sync_enabled = enabled;
	
	if (!sync_enabled) {
		sync_waiting = false;
		sync_in = false;
		sync_z_um = z_um;
		sync_phase_ticks = c_ticks;
		return;
	}
	
	// Only reset sync if parameters changed significantly or sync was just enabled
	// Small variations in parameters (due to rounding etc) shouldn't reset sync
	const int32_t z_diff = (z_um > sync_z_um) ? (z_um - sync_z_um) : (sync_z_um - z_um);
	int32_t phase_diff = (c_ticks > sync_phase_ticks) ? (c_ticks - sync_phase_ticks) : (sync_phase_ticks - c_ticks);
	if (phase_diff > C_COUNTS_PER_REV / 2) phase_diff = C_COUNTS_PER_REV - phase_diff;
	
	const bool params_changed = (z_diff > 100) || (phase_diff > 8);  // 100um or ~2 degrees
	
	if (!was_enabled || params_changed) {
#if DEBUG_SPI_LOGGING
		if (sync_in) {
			Serial.printf("[SYNC] Reset! was_en=%d, z_diff=%ld, phase_diff=%ld\n",
				was_enabled, z_diff, phase_diff);
		}
#endif
		sync_in = false;
		sync_waiting = ElsCore::enabled;
	}
	
	sync_z_um = z_um;
	sync_phase_ticks = c_ticks;
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
				
				// Debug: log acquisition attempt every 500ms
				static uint32_t last_acq_debug_ms = 0;
				if (millis() - last_acq_debug_ms > 500) {
					Serial.printf("[ACQ] z=%ld, z_ref=%ld, phase_d=%ld, tgt=%ld, cur=%ld, diff=%ld\n",
						z_um, sync_z_um, phase_delta, target_phase, current_phase,
						(current_phase - target_phase));
					last_acq_debug_ms = millis();
				}
				
				// Check if we're within tolerance OR crossed through the target
				const bool at_target = phase_within_tolerance(current_phase, target_phase, sync_tolerance_in_ticks);
				const bool crossed_target = crossed_phase(last_spindle_count, spindle_count, target_phase);
				
				if (!at_target && !crossed_target) {
					last_spindle_count = spindle_count;
					last_z_um = z_um;
					return;
				}
				
				// Sync acquired!
				sync_waiting = false;
				sync_in = true;
				sync_ref_z_um = z_um;
				sync_ref_spindle = spindle_count - (current_phase - target_phase);
				last_spindle_count = spindle_count;
				step_accumulator = 0;
				
				// Log acquisition details
				Serial.printf("[ACQ] LOCKED! z=%ld, tgt_phase=%ld, cur_phase=%ld, snap_spindle=%ld\n",
					z_um, target_phase, current_phase, sync_ref_spindle);
			}
		} else if (!sync_in) {
			sync_waiting = true;
			last_spindle_count = spindle_count;
			last_z_um = z_um;
			step_accumulator = 0;
			return;
		}

		// If sync_in, continue to step output below
	}

	// ========================================================================
	// STEP OUTPUT - with sync correction if enabled
	// ========================================================================
	
	int32_t spindle_delta = spindle_count - last_spindle_count;
    last_spindle_count = spindle_count;
	last_z_um = z_um;
    
    if (spindle_delta == 0) return;
    
    // Check endstops before moving
    if (!checkEndstops(z_um)) {
        enabled = false;
        fault = true;
        endstop_triggered = true;
        return;
    }
    
    // Calculate base step output from spindle delta
    int64_t numerator = (int64_t)spindle_delta * (int64_t)pitch_um * 
                        (int64_t)ELS_STEPS_PER_REV * (int64_t)direction_mul * FP_SCALE;
    int64_t denominator = (int64_t)C_COUNTS_PER_REV * (int64_t)ELS_LEADSCREW_PITCH_UM;
    
    int64_t step_delta_fp = numerator / denominator;

	// ========================================================================
	// SYNC CORRECTION - using reference point captured at acquisition
	// ========================================================================
	if (sync_enabled && sync_in) {
        constexpr int32_t COARSE_TOLERANCE_UM = 500;  // 0.5mm - well under 1 thread pitch (2mm)

		// Calculate expected Z based on spindle delta FROM THE REFERENCE POINT
		// At acquisition: sync_ref_z_um and sync_ref_spindle were captured
		// Note: Sign is inverted because Z encoder direction is opposite to step output
		const int64_t spindle_delta_from_ref = spindle_count - sync_ref_spindle;
		const int64_t expected_z_num = spindle_delta_from_ref * (int64_t)pitch_um * (int64_t)direction_mul;
		const int32_t expected_z = sync_ref_z_um - (int32_t)(expected_z_num / (int64_t)C_COUNTS_PER_REV);

		// Error: positive = Z is ahead (too far), negative = Z is behind
        const int32_t z_error = z_um - expected_z;
        const int32_t abs_z_error = (z_error < 0) ? -z_error : z_error;
        
        static uint32_t last_sync_debug_ms = 0;
        if (millis() - last_sync_debug_ms > 500) {
            Serial.printf("[SYNC] err=%ld um%s\n", z_error, 
                (abs_z_error > COARSE_TOLERANCE_UM) ? " LOST!" : "");
            last_sync_debug_ms = millis();
        }
        
        if (abs_z_error > COARSE_TOLERANCE_UM) {
            // Major disturbance (half-nut disconnected?) - go RED, wait for re-sync
            sync_in = false;
            sync_waiting = true;
            step_accumulator = 0;
            return;  // Don't output steps until re-synced
        }
        
        // TODO: Add fine tolerance correction to creep back to perfect alignment
    }
    
    // Accumulate fractional steps
    step_accumulator += step_delta_fp;
    
    // Extract whole steps
    int32_t steps_to_output = (int32_t)(step_accumulator / FP_SCALE);
    
    if (steps_to_output != 0) {
        // Remove outputted steps from accumulator
        step_accumulator -= (int64_t)steps_to_output * FP_SCALE;
        
#if DEBUG_SPI_LOGGING
        static uint32_t last_debug_ms = 0;
        static int32_t total_steps_output = 0;
        total_steps_output += abs(steps_to_output);
        if (millis() - last_debug_ms > 1000) {
            Serial.printf("[ELS] steps_out=%ld\n", total_steps_output);
            total_steps_output = 0;
            last_debug_ms = millis();
        }
#endif
        
        // Output steps
        Stepper::step(steps_to_output);
    }
}
