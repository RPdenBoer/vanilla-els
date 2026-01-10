#pragma once

#include <stdint.h>
#include <Arduino.h>  // For IRAM_ATTR

// ============================================================================
// Encoder handling for Motion board (ESP32)
// Reads X, Z linear encoders via GPIO ISR quadrature decoding
// Spindle position/RPM comes from SpindleStepper class
// ============================================================================

class EncoderMotion {
public:
    static bool init();
    static void update();

	// Get raw encoder counts for linear axes
	static int32_t getXCount();
    static int32_t getZCount();

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

	static void initLinearAxis(QuadAxis &axis);
	static void IRAM_ATTR quadIsr(void *arg);
};
