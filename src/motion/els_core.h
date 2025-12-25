#pragma once

#include <stdint.h>

// ============================================================================
// Electronic Leadscrew Core Logic
// Synchronizes stepper position with spindle encoder
// ============================================================================

class ElsCore {
public:
    static void init();
    static void update();  // Called from high-priority motion task
    
    // Enable/disable ELS
    static bool isEnabled() { return enabled; }
    static void setEnabled(bool on);
    
    // Thread pitch in microns per spindle revolution
    static void setPitchUm(int32_t pitch);
    static int32_t getPitchUm() { return pitch_um; }
    
    // Direction multiplier (+1 normal, -1 reversed for jog)
    static void setDirectionMul(int8_t mul);

    // Sync helper (phase-lock to spindle based on Z=0/C=0 reference)
    static void setSync(bool enabled, int32_t z_um, int32_t c_ticks);
    static bool isSyncWaiting() { return sync_waiting; }
    static bool isSyncEnabled() { return sync_enabled; }
    static bool isSyncIn() { return sync_in; }

	// Jog control (constant feed, ignores spindle)
	static void setJog(int8_t dir, bool active);
	static bool isJogActive() { return jog_active; }
    
    // Software endstops (in raw Z encoder microns)
    static void setEndstops(int32_t min_um, int32_t max_um, bool min_en, bool max_en);
    
    // Fault/status
    static bool hasFault() { return fault; }
    static bool endstopTriggered() { return endstop_triggered; }
    static void clearFault() { fault = false; endstop_triggered = false; }
    
private:
    static bool enabled;
    static bool fault;
    static bool endstop_triggered;
    
    static int32_t pitch_um;
    static int8_t direction_mul;
    
    static int32_t endstop_min_um;
    static int32_t endstop_max_um;
    static bool endstop_min_enabled;
    static bool endstop_max_enabled;
    
    static int32_t last_spindle_count;
    static int64_t step_accumulator;  // Fixed-point accumulator for sub-step precision

    static bool sync_enabled;
    static bool sync_waiting;
    static bool sync_in;
    static int32_t sync_z_um;         // Reference Z0 (machine coordinates)
    static int32_t sync_phase_ticks;  // Reference C0 (raw ticks)
    static int32_t sync_tolerance_out_um;
    static int32_t sync_ref_z_um;
    static int32_t sync_ref_spindle;
	static int32_t last_z_um;
	static volatile bool jog_active;
	static volatile int8_t jog_dir;
	static bool jog_prev_active;
	static uint32_t jog_last_us;
	static int64_t jog_step_accumulator;

    static bool checkEndstops(int32_t z_um);
};
