#pragma once

#include <stdint.h>

// ============================================================================
// Stepper motor driver for ELS Z-axis
// Uses RMT peripheral for precise pulse timing
// ============================================================================

class Stepper {
public:
    static bool init();
    
    // Output N steps in the given direction
    // Positive = forward, negative = reverse
    static void step(int32_t count);
    
    // Get current position in steps
    static int32_t getPosition() { return position; }
    
    // Reset position counter (doesn't move motor)
    static void resetPosition() { position = 0; }
    
    // Set direction for next steps
    static void setDirection(bool forward);
    
private:
    static int32_t position;
    static bool rmt_ready;
    static void* rmt_channel;
};
