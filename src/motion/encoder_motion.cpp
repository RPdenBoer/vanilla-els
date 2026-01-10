#include "encoder_motion.h"
#include "config_motion.h"
#include <Arduino.h>
#include "driver/gpio.h"

// Static member initialization
EncoderMotion::QuadAxis EncoderMotion::x_axis = {0, 0, 0, 0, 1};
EncoderMotion::QuadAxis EncoderMotion::z_axis = {0, 0, 0, 0, 1};

// ============================================================================
// Quadrature decoder for X/Z linear encoders (GPIO ISR based)
// ============================================================================

static inline uint8_t read_ab(uint8_t pin_a, uint8_t pin_b) {
    const int a = gpio_get_level((gpio_num_t)pin_a);
    const int b = gpio_get_level((gpio_num_t)pin_b);
    return (uint8_t)(((a ? 1 : 0) << 1) | (b ? 1 : 0));
}

void IRAM_ATTR EncoderMotion::quadIsr(void *arg) {
    QuadAxis *axis = (QuadAxis *)arg;

    // Quadrature state machine transition table
    static const int8_t delta_tbl[16] = {
        0, -1,  1,  0,
        1,  0,  0, -1,
       -1,  0,  0,  1,
        0,  1, -1,  0,
    };

    const uint8_t old_state = axis->state & 0x3;
    const uint8_t new_state = read_ab(axis->pin_a, axis->pin_b);
    axis->state = new_state;

    const uint8_t idx = (uint8_t)((old_state << 2) | new_state);
    const int8_t d = delta_tbl[idx];
    if (d != 0) {
        axis->count += (int32_t)d * (int32_t)axis->dir;
    }
}

void EncoderMotion::initLinearAxis(QuadAxis &axis) {
    pinMode(axis.pin_a, INPUT_PULLUP);
    pinMode(axis.pin_b, INPUT_PULLUP);

    axis.state = read_ab(axis.pin_a, axis.pin_b);

    attachInterruptArg((int)axis.pin_a, quadIsr, (void *)&axis, CHANGE);
    attachInterruptArg((int)axis.pin_b, quadIsr, (void *)&axis, CHANGE);
}

// ============================================================================
// Initialization
// ============================================================================

bool EncoderMotion::init() {
    // Initialize X axis encoder
    x_axis.pin_a = X_PINA;
    x_axis.pin_b = X_PINB;
    x_axis.count = 0;
    x_axis.dir = X_INVERT_DIR ? -1 : 1;
    initLinearAxis(x_axis);

    // Initialize Z axis encoder
    z_axis.pin_a = Z_PINA;
    z_axis.pin_b = Z_PINB;
    z_axis.count = 0;
    z_axis.dir = Z_INVERT_DIR ? -1 : 1;
    initLinearAxis(z_axis);

	return true;
}

// ============================================================================
// Update (called periodically from motion task)
// ============================================================================

void EncoderMotion::update() {
    // Nothing to do - linear encoders are interrupt-driven
    // Spindle RPM comes from SpindleStepper class
}

// ============================================================================
// Getters
// ============================================================================

int32_t EncoderMotion::getXCount() {
    int32_t v;
    noInterrupts();
    v = x_axis.count;
    interrupts();
    return v;
}

int32_t EncoderMotion::getZCount() {
    int32_t v;
    noInterrupts();
    v = z_axis.count;
    interrupts();
    return v;
}
