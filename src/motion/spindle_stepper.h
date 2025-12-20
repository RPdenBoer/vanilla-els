#pragma once

#include <stdint.h>
#include <Arduino.h>  // For IRAM_ATTR

// ============================================================================
// Spindle Stepper Driver
// Generates step pulses to drive spindle motor, with position/RPM tracking
// Used when SPINDLE_MODE == SPINDLE_MODE_STEPPER
// ============================================================================

class SpindleStepper {
public:
    // Initialize timer and GPIO
    static bool init();
    
    // Call from main loop to read controls and update speed
    static void update();
    
    // Get current spindle position in steps (used by ELS as source of truth)
    static int32_t getPosition() { return position; }
    
    // Get current actual RPM (may differ from target during accel)
    static int16_t getRpmSigned() { return rpm_signed; }
    static int16_t getRpmAbs() { return rpm_abs; }
    
    // Get target RPM from potentiometer
    static int16_t getTargetRpm() { return target_rpm; }
    
    // Get direction: +1 forward, -1 reverse, 0 stopped
    static int8_t getDirection() { return direction; }
    
    // Reset position counter (e.g., for zeroing)
    static void resetPosition() { position = 0; }
    
    // Emergency stop - immediately stops output
    static void stop();
    
    // Check if spindle is running
    static bool isRunning() { return running; }
    
    // Position is public so ISR can access it
    static volatile int32_t position;
    
private:
    static int16_t rpm_signed;          // Current RPM with sign
    static int16_t rpm_abs;             // Current RPM absolute
    static int16_t target_rpm;          // Target RPM from pot
    static int16_t current_rpm;         // Actual RPM (after accel limiting)
    static int8_t direction;            // +1, -1, or 0
    static bool running;
    
    static uint32_t last_update_us;
    static uint32_t step_interval_us;   // Microseconds between steps
    static uint32_t last_step_us;
    
    // Read analog potentiometer and direction switch
    static void readControls();
    
    // Apply acceleration limiting
    static void updateSpeed();
    
    // Hardware timer handle
    static void* timer_handle;
};
