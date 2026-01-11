#pragma once

#include <stdint.h>
#include "../shared/protocol.h"

// ============================================================================
// EncoderProxy: Local cache of encoder state received from motion board
// ============================================================================

class EncoderProxy {
public:
    static void init();
    
    // Update from motion board status packet
	static void updateFromMotion(int32_t c_ticks, int16_t rpm_x10, int16_t target_rpm, uint8_t mpg_mode, bool spindle_moving);

	// Get cached values
    static int32_t getRawTicks() { return c_raw_ticks; }
    static int32_t getSpindleTotalCount() { return c_total_count; }
    static int32_t getRpm() { return rpm_raw; }
    static int32_t getRpmSigned() { return rpm_signed; }

	// Target RPM from MPG encoder
	static int16_t getTargetRpm() { return target_rpm; }

	// MPG mode from motion board
	static MpgModeProto getMpgMode() { return mpg_mode; }

	// Spindle state
	static bool isSpindleRunning() { return spindle_running; }

	// Display mode: RPM vs degrees
	// In RPM_CONTROL mode: show RPM
	// In JOG_C mode: show degrees
	// In JOG_Z mode: show degrees
	static bool shouldShowRpm() { return mpg_mode == MpgModeProto::RPM_CONTROL; }
    
private:
    static int32_t c_raw_ticks;
    static int32_t c_total_count;
    static int32_t rpm_raw;
    static int32_t rpm_signed;
	static int16_t target_rpm;
	static MpgModeProto mpg_mode;
	static bool spindle_running;
};
