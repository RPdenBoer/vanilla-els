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

int32_t ElsCore::endstop_min_um = INT32_MIN;
int32_t ElsCore::endstop_max_um = INT32_MAX;
bool ElsCore::endstop_min_enabled = false;
bool ElsCore::endstop_max_enabled = false;

int32_t ElsCore::last_spindle_count = 0;
int64_t ElsCore::step_accumulator = 0;

// Fixed-point scale for sub-step precision (16 fractional bits)
static constexpr int64_t FP_SCALE = 65536;

void ElsCore::init() {
    enabled = false;
    fault = false;
    endstop_triggered = false;
	last_spindle_count = getSpindlePosition();
	step_accumulator = 0;
}

void ElsCore::setEnabled(bool on) {
    if (on && !enabled) {
        // Enabling: sync to current spindle position
		last_spindle_count = getSpindlePosition();
		step_accumulator = 0;
        fault = false;
        endstop_triggered = false;
    }
    enabled = on;
}

void ElsCore::setPitchUm(int32_t pitch) {
    pitch_um = pitch;
}

void ElsCore::setDirectionMul(int8_t mul) {
    direction_mul = (mul < 0) ? -1 : 1;
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
    if (!enabled) return;

	// Get current spindle position (from encoder or stepper, depending on mode)
	int32_t spindle_count = getSpindlePosition();
	int32_t spindle_delta = spindle_count - last_spindle_count;
    last_spindle_count = spindle_count;
    
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
    int32_t z_um = EncoderMotion::getZCount() * Z_UM_PER_COUNT;
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
