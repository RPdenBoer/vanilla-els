#pragma once

#include <stdint.h>
#include <Arduino.h>  // For IRAM_ATTR
#include "config_motion.h"

// ============================================================================
// Encoder handling for Motion board (ESP32)
// Reads X, Z linear encoders
// In ENCODER mode: also reads C spindle encoder via PCNT
// In STEPPER mode: spindle data comes from SpindleStepper class
// ============================================================================

class EncoderMotion {
public:
    static bool init();
    static void update();

	// Get raw encoder counts for linear axes (always available)
	static int32_t getXCount();
    static int32_t getZCount();

#if SPINDLE_MODE == SPINDLE_MODE_ENCODER
	// Spindle encoder functions (only in encoder mode)
	static int32_t getSpindleCount();  // Extended count (beyond PCNT limits)
    
    // RPM (updated periodically)
    static int16_t getRpmSigned() { return rpm_signed; }
    static int16_t getRpmAbs() { return rpm_abs; }
    
    // Allow PCNT callback to access accumulator
    static volatile int32_t c_pcnt_accum;
#endif

private:
    struct QuadAxis {
        uint8_t pin_a;
        uint8_t pin_b;
        volatile int32_t count;
        volatile uint8_t state;
        int8_t dir;
    };
    
    static QuadAxis x_axis;
    static QuadAxis z_axis;

#if SPINDLE_MODE == SPINDLE_MODE_ENCODER
	static int16_t rpm_signed;
    static int16_t rpm_abs;

	static int32_t getTotalSpindleCount();
#endif

	static void initLinearAxis(QuadAxis &axis);
	static void IRAM_ATTR quadIsr(void *arg);
};
