#pragma once

#include <stdint.h>
#include <stddef.h>

// ============================================================================
// LeadscrewProxy: Sends ELS commands to motion board, tracks local UI state
// ============================================================================

class LeadscrewProxy {
public:
    static void init();
    
    // State that UI controls (sent to motion board)
    static bool isEnabled() { return enabled; }
    static void setEnabled(bool on);
    static void toggleEnabled() { setEnabled(!enabled); }
    
    // Pitch input/display mode is independent of DRO mm/in
    static bool isPitchTpiMode() { return pitch_tpi_mode; }
    static void setPitchTpiMode(bool on) { pitch_tpi_mode = on; }
    static void togglePitchTpiMode() { pitch_tpi_mode = !pitch_tpi_mode; }
    
    // Pitch in microns per spindle revolution
    static int32_t getPitchUm() { return pitch_um; }
    static void setPitchUm(int32_t pitch_um_per_rev);
    
    // Direction multiplier: +1 normal, -1 reversed (for jog)
    static int8_t getDirectionMul() { return direction_mul; }
    static void setDirectionMul(int8_t mul) { direction_mul = (mul < 0) ? -1 : 1; }
    
    // UI helpers
    static void formatPitchLabel(char *out, size_t n);
    
    // Flag set when bounds are exceeded (reported from motion board)
    static bool wasBoundsExceeded() { return bounds_exceeded; }
    static void clearBoundsExceeded() { bounds_exceeded = false; }
    static void setBoundsExceeded(bool exceeded) { bounds_exceeded = exceeded; }
    
    // Track ELS fault from motion board
    static bool hasFault() { return els_fault; }
    static void setFault(bool fault) { els_fault = fault; }
    
private:
    static bool enabled;
    static bool pitch_tpi_mode;
    static int32_t pitch_um;
    static int8_t direction_mul;
    static bool bounds_exceeded;
    static bool els_fault;
};
