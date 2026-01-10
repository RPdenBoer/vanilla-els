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
int32_t ElsCore::sync_speed_scale_fp = 65536;
uint32_t ElsCore::sync_last_adjust_ms = 0;
uint16_t ElsCore::sync_abs_error_um = 0;
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

static inline int32_t abs_i32(int32_t v) { return (v < 0) ? -v : v; }

uint16_t ElsCore::getSyncSpeedScalePermille()
{
	// sync_speed_scale_fp is fixed-point with FP_SCALE (= 65536) meaning 1.0x.
	int64_t permille = ((int64_t)sync_speed_scale_fp * 1000LL) / FP_SCALE;
	if (permille < 0) permille = 0;
	if (permille > 65535) permille = 65535;
	return (uint16_t)permille;
}

uint16_t ElsCore::getSyncAbsErrorUm()
{
	return sync_abs_error_um;
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
	sync_speed_scale_fp = (int32_t)FP_SCALE;
	sync_last_adjust_ms = millis();
	sync_abs_error_um = 0;
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
		sync_speed_scale_fp = (int32_t)FP_SCALE;
		sync_abs_error_um = 0;
	} else if (sync_enabled && !was_enabled) {
		// Continuous sync: keep moving; reset controller state on enable
		sync_waiting = false;
		sync_in = false;
		sync_speed_scale_fp = (int32_t)FP_SCALE;
		sync_last_adjust_ms = millis();
		sync_abs_error_um = 0;
	}
}

void ElsCore::setPitchUm(int32_t pitch) {
    if (pitch == pitch_um) return;
    pitch_um = pitch;
	if (sync_enabled && enabled) {
		sync_waiting = false;
		sync_in = false;
		sync_speed_scale_fp = (int32_t)FP_SCALE;
		sync_last_adjust_ms = millis();
	}
}

void ElsCore::setDirectionMul(int8_t mul) {
    int8_t new_mul = (mul < 0) ? -1 : 1;
	if (new_mul == direction_mul) return;
    direction_mul = new_mul;
	if (sync_enabled && enabled) {
		sync_waiting = false;
		sync_in = false;
		sync_speed_scale_fp = (int32_t)FP_SCALE;
		sync_last_adjust_ms = millis();
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
	const bool enabled_changed = (enabled != sync_enabled);
	const bool ref_changed = (z_um != sync_z_um) || (c_ticks != sync_phase_ticks);

	// NOTE: setSync() is invoked frequently via SPI commands.
	// Do NOT reset controller state unless something actually changed, otherwise
	// the trim will be stuck at 1.000x and sync will appear to do nothing.
	sync_enabled = enabled;
	sync_z_um = z_um;
	sync_phase_ticks = c_ticks;

	if (enabled_changed || ref_changed)
	{
		// Continuous sync: no waiting/acquire state. We always run ELS normally and
		// apply a bounded speed trim to converge toward the theoretical 0/0 crossing.
		sync_waiting = false;
		sync_in = false;
		sync_speed_scale_fp = (int32_t)FP_SCALE;
		sync_last_adjust_ms = millis();
		sync_abs_error_um = 0;
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
				sync_waiting = false;
				sync_in = false;
				sync_speed_scale_fp = (int32_t)FP_SCALE;
				sync_last_adjust_ms = millis();
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
			sync_waiting = false;
			sync_in = false;
			sync_speed_scale_fp = (int32_t)FP_SCALE;
			sync_last_adjust_ms = millis();
		}
	}

    if (!enabled) return;

	// Get current spindle position (from encoder or stepper, depending on mode)
	int32_t spindle_count = getSpindlePosition();
	const int32_t z_um = EncoderMotion::getZCount() * Z_UM_PER_COUNT;

	// Continuous sync: no waiting/acquire path. Always run step output below.
	sync_waiting = false;

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
	// SYNC SPEED TRIM (continuous)
	// - Always keep outputting ELS steps from spindle motion.
	// - If sync is enabled, compute expected Z from the theoretical 0/0 crossing
	//   and apply a bounded speed trim (±50%) to converge.
	// ========================================================================
	if (sync_enabled && pitch_um != 0)
	{
		const int32_t sync_scale_min_fp = (int32_t)((FP_SCALE * (int64_t)SYNC_SPEED_MIN_PCT) / 100);
		const int32_t sync_scale_max_fp = (int32_t)((FP_SCALE * (int64_t)SYNC_SPEED_MAX_PCT) / 100);

		// Expected Z based on spindle position relative to the 0/0 reference.
		// Note: Sign is inverted because Z encoder direction is opposite to step output.
		const int64_t spindle_from_c0 = (int64_t)spindle_count - (int64_t)sync_phase_ticks;
		const int64_t expected_z_num = spindle_from_c0 * (int64_t)pitch_um * (int64_t)direction_mul;
		const int32_t expected_z = sync_z_um - (int32_t)(expected_z_num / (int64_t)C_COUNTS_PER_REV);
		const int32_t z_error = z_um - expected_z;
		const int32_t abs_z_error = abs_i32(z_error);
		sync_abs_error_um = (abs_z_error > 65535) ? 65535 : (uint16_t)abs_z_error;

#if DEBUG_SPI_LOGGING
		static uint32_t last_sync_dbg_ms = 0;
		if ((uint32_t)(millis() - last_sync_dbg_ms) >= 1000) {
			last_sync_dbg_ms = millis();
			Serial.printf("[SYNC] err_um=%ld scale=%u/1000 Z=%ld expZ=%ld C=%ld c0=%ld\n",
				(long)z_error,
				(unsigned)ElsCore::getSyncSpeedScalePermille(),
				(long)z_um,
				(long)expected_z,
				(long)spindle_count,
				(long)sync_phase_ticks);
		}
#endif

		// Hysteresis for UI state: in-sync means "close enough".
		if (sync_in) {
			if (abs_z_error > SYNC_OUT_TOL_UM) sync_in = false;
		} else {
			if (abs_z_error <= SYNC_IN_TOL_UM) sync_in = true;
		}

		const uint32_t now_ms = millis();
		if ((uint32_t)(now_ms - sync_last_adjust_ms) >= SYNC_ADJUST_INTERVAL_MS)
		{
			sync_last_adjust_ms = now_ms;

			int32_t target_scale_fp = (int32_t)FP_SCALE;
			if (abs_z_error > SYNC_DEADBAND_UM)
			{
				// error_frac_fp ~= (z_error / pitch) in fixed-point
				const int64_t error_frac_fp = ((int64_t)z_error * (int64_t)FP_SCALE) / (int64_t)pitch_um;
				const int64_t correction_fp = -((error_frac_fp * (int64_t)SYNC_K_NUM) / (int64_t)SYNC_K_DEN);
				int64_t scale_fp_64 = (int64_t)FP_SCALE + correction_fp;
				if (scale_fp_64 < (int64_t)sync_scale_min_fp) scale_fp_64 = (int64_t)sync_scale_min_fp;
				if (scale_fp_64 > (int64_t)sync_scale_max_fp) scale_fp_64 = (int64_t)sync_scale_max_fp;
				target_scale_fp = (int32_t)scale_fp_64;
			}

			// Smooth the scale to avoid oscillations.
			sync_speed_scale_fp += (target_scale_fp - sync_speed_scale_fp) / SYNC_SMOOTH_DEN;
		}

		// Apply current scale to the step delta (fixed-point).
		step_delta_fp = ((int64_t)step_delta_fp * (int64_t)sync_speed_scale_fp) / (int64_t)FP_SCALE;
	}
	else
	{
		// Sync disabled or invalid pitch: run nominal.
		sync_in = false;
		sync_speed_scale_fp = (int32_t)FP_SCALE;
		sync_abs_error_um = 0;
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
