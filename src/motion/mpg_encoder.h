#pragma once

#include <Arduino.h>
#include <stdint.h>

// ============================================================================
// MPG Encoder: Manual Pulse Generator for speed control and axis jogging
// Uses GPIO ISR for quadrature decoding (input-only pins don't support PCNT)
// ============================================================================

// MPG operating modes
enum class MpgMode : uint8_t {
    RPM_CONTROL = 0,  // Default: controls spindle RPM (0-200 counts = 0-3000 RPM)
    JOG_Z,            // Jog Z axis (each pulse = step)
    JOG_C,            // Jog C axis / spindle position
};

class MpgEncoder {
public:
    static bool init();
    
    // Call from main loop to process accumulated counts
    static void update();
    
    // Get accumulated counts since last read (resets accumulator)
    static int32_t getDelta();
    
    // Get current RPM setting (0-3000 based on position)
    static int16_t getRpmSetting() { return rpm_setting; }
    
    // Get total accumulated position (not reset)
    static int32_t getPosition() { return position; }
    
    // Mode control
    static MpgMode getMode() { return mode; }
    static void setMode(MpgMode m);
    
    // Reset position to zero (e.g., when changing modes)
    static void resetPosition() { position = 0; delta_accum = 0; }
    
private:
    static volatile int32_t position;     // Total accumulated position
    static volatile int32_t delta_accum;  // Accumulated delta since last getDelta()
    static volatile uint8_t last_state;
    static int16_t rpm_setting;           // Current RPM derived from position
    static MpgMode mode;
    
    static void IRAM_ATTR isrA();
    static void IRAM_ATTR isrB();
    static void IRAM_ATTR processQuadrature();
};
