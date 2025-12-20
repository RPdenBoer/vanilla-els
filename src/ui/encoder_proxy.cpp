#include "encoder_proxy.h"
#include "coordinates_ui.h"

// Static member definitions
int32_t EncoderProxy::c_raw_ticks = 0;
int32_t EncoderProxy::c_total_count = 0;
int32_t EncoderProxy::rpm_raw = 0;
int32_t EncoderProxy::rpm_signed = 0;
int16_t EncoderProxy::target_rpm = 0;
MpgModeProto EncoderProxy::mpg_mode = MpgModeProto::RPM_CONTROL;
bool EncoderProxy::c_show_rpm = false;
bool EncoderProxy::c_manual_rpm_mode = false;

void EncoderProxy::init() {
    c_raw_ticks = 0;
    c_total_count = 0;
    rpm_raw = 0;
    rpm_signed = 0;
	target_rpm = 0;
	mpg_mode = MpgModeProto::RPM_CONTROL;
	c_show_rpm = false;
    c_manual_rpm_mode = false;
}

void EncoderProxy::updateFromMotion(int32_t c_ticks, int16_t rpm, int16_t tgt_rpm, uint8_t mode)
{
	c_raw_ticks = c_ticks;
    c_total_count = c_ticks;  // Total count for ELS sync
    
    // Handle RPM direction
    rpm_raw = (rpm < 0) ? -rpm : rpm;
    rpm_signed = rpm;

	// Store target RPM and MPG mode from motion board
	target_rpm = tgt_rpm;
	mpg_mode = static_cast<MpgModeProto>(mode);

	// Update coordinate system raw C value
    CoordinateSystem::c_raw_ticks = c_ticks;
    
    // Update display mode with hysteresis
    if (c_manual_rpm_mode) {
        // Manual mode: stay in RPM mode
        c_show_rpm = true;
    } else {
        // Auto mode: switch based on thresholds with hysteresis
        if (rpm_raw > RPM_SHOW_RPM_ON) {
            c_show_rpm = true;
        } else if (rpm_raw < RPM_SHOW_RPM_OFF) {
            c_show_rpm = false;
        }
        // Between thresholds: keep current state (hysteresis)
    }
}

void EncoderProxy::toggleManualRpmMode() {
    if (!canToggleManualMode()) return;
    c_manual_rpm_mode = !c_manual_rpm_mode;
    c_show_rpm = c_manual_rpm_mode;
}
