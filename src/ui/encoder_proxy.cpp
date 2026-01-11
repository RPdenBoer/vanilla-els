#include "encoder_proxy.h"
#include "coordinates_ui.h"

// Static member definitions
int32_t EncoderProxy::c_raw_ticks = 0;
int32_t EncoderProxy::c_total_count = 0;
int32_t EncoderProxy::rpm_raw = 0;
int32_t EncoderProxy::rpm_signed = 0;
int16_t EncoderProxy::target_rpm = 0;
MpgModeProto EncoderProxy::mpg_mode = MpgModeProto::RPM_CONTROL;
bool EncoderProxy::spindle_running = false;

void EncoderProxy::init() {
    c_raw_ticks = 0;
    c_total_count = 0;
    rpm_raw = 0;
    rpm_signed = 0;
	target_rpm = 0;
	mpg_mode = MpgModeProto::RPM_CONTROL;
	spindle_running = false;
}

void EncoderProxy::updateFromMotion(int32_t c_ticks, int16_t rpm_x10, int16_t tgt_rpm, uint8_t mode, bool moving)
{
	c_raw_ticks = c_ticks;
    c_total_count = c_ticks;  // Total count for ELS sync
    
    // Handle RPM direction
	rpm_raw = (rpm_x10 < 0) ? -rpm_x10 : rpm_x10;
	rpm_signed = rpm_x10;

	// Store values from motion board
	target_rpm = tgt_rpm;
	mpg_mode = static_cast<MpgModeProto>(mode);
	spindle_running = moving;

	// Update coordinate system raw C value
    CoordinateSystem::c_raw_ticks = c_ticks;
}
