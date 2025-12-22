#include "leadscrew_proxy.h"
#include "coordinates_ui.h"

#include <Arduino.h>
#include <cmath>

// Static member definitions
bool LeadscrewProxy::enabled = false;
bool LeadscrewProxy::pitch_tpi_mode = false;
int32_t LeadscrewProxy::pitch_um = 1000; // default: 1.000mm pitch
int8_t LeadscrewProxy::direction_mul = 1;
bool LeadscrewProxy::bounds_exceeded = false;
bool LeadscrewProxy::els_fault = false;

void LeadscrewProxy::init() {
    // Default pitch depends on unit mode: in -> 20 TPI, mm -> 1.0mm
    if (CoordinateSystem::isLinearInchMode()) {
        // 20 TPI => 1/20 inch => 25.4mm/20 = 1.27mm = 1270um
        pitch_um = (int32_t)lroundf(25400.0f / 20.0f);
        pitch_tpi_mode = true;
    } else {
        pitch_tpi_mode = false;
    }
    
    enabled = false;
    direction_mul = 1;
    bounds_exceeded = false;
    els_fault = false;
}

void LeadscrewProxy::setPitchUm(int32_t pitch_um_per_rev) {
    if (pitch_um_per_rev == 0) pitch_um_per_rev = 1;
    // Keep magnitude at least 1um/rev
    if (pitch_um_per_rev > 0 && pitch_um_per_rev < 1) pitch_um_per_rev = 1;
    if (pitch_um_per_rev < 0 && pitch_um_per_rev > -1) pitch_um_per_rev = -1;
    pitch_um = pitch_um_per_rev;
}

void LeadscrewProxy::setEnabled(bool on) {
    enabled = on;
    // Note: The actual enable command is sent via SPI to motion board
    // This just tracks local UI state
}

void LeadscrewProxy::formatPitchLabel(char *out, size_t n) {
    if (!out || n == 0) return;

    if (pitch_tpi_mode) {
        // Display as TPI
        const float abs_pitch = (float)abs(pitch_um);
        float tpi = 25400.0f / abs_pitch;
        if (pitch_um < 0) snprintf(out, n, "-%.3f", tpi);
        else              snprintf(out, n, "%.3f", tpi);
        return;
    }

    // Display as pitch length (unitless): mm/rev in MM mode, inches/rev in INCH mode.
    if (CoordinateSystem::isLinearInchMode()) {
        const float inches = (float)pitch_um / 25400.0f;
        snprintf(out, n, "%.4f", inches);
    } else {
        const float mm = (float)pitch_um / 1000.0f;
        snprintf(out, n, "%.3f", mm);
    }
}
