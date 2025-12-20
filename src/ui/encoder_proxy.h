#pragma once

#include <stdint.h>
#include "config_ui.h"
#include "../shared/protocol.h"

// ============================================================================
// EncoderProxy: Local cache of encoder state received from motion board
// ============================================================================

class EncoderProxy {
public:
    static void init();
    
    // Update from motion board status packet
	static void updateFromMotion(int32_t c_ticks, int16_t rpm, int16_t target_rpm, uint8_t mpg_mode);

	// Get cached values (same API as original EncoderManager)
    static int32_t getRawTicks() { return c_raw_ticks; }
    static int32_t getSpindleTotalCount() { return c_total_count; }
    static int32_t getRpm() { return rpm_raw; }
    static int32_t getRpmSigned() { return rpm_signed; }
    static bool shouldShowRpm() { return c_show_rpm; }

	// Target RPM from MPG encoder (stepper spindle mode)
	static int16_t getTargetRpm() { return target_rpm; }

	// MPG mode from motion board
	static MpgModeProto getMpgMode() { return mpg_mode; }

	// Manual RPM/deg toggle (only effective when below auto-threshold)
    static bool isManualRpmMode() { return c_manual_rpm_mode; }
    static void toggleManualRpmMode();
    static bool canToggleManualMode() { return rpm_raw <= RPM_SHOW_RPM_OFF; }
    
private:
    static int32_t c_raw_ticks;
    static int32_t c_total_count;
    static int32_t rpm_raw;
    static int32_t rpm_signed;
	static int16_t target_rpm;
	static MpgModeProto mpg_mode;
	static bool c_show_rpm;
    static bool c_manual_rpm_mode;
};
