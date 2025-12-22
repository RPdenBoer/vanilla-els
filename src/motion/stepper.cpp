#include "stepper.h"
#include "config_motion.h"
#include <Arduino.h>
#include "esp32-hal-rmt.h"

// Static member initialization
int32_t Stepper::position = 0;
bool Stepper::rmt_ready = false;

bool Stepper::init() {
    // Configure GPIO pins
    pinMode(ELS_STEP_PIN, OUTPUT);
    pinMode(ELS_DIR_PIN, OUTPUT);
    digitalWrite(ELS_STEP_PIN, LOW);
    digitalWrite(ELS_DIR_PIN, LOW);
    
    if (ELS_EN_PIN >= 0) {
        pinMode(ELS_EN_PIN, OUTPUT);
        digitalWrite(ELS_EN_PIN, ELS_EN_ACTIVE_LOW ? HIGH : LOW);  // Disabled
    }
    
    rmt_ready = rmtInit(ELS_STEP_PIN, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, ELS_RMT_RES_HZ);
    if (!rmt_ready) {
        Serial.println("[Stepper] RMT init failed");
        return false;
    }
    rmtSetEOT(ELS_STEP_PIN, 0);

    Serial.println("[Stepper] RMT mode initialized");
    return true;
}

void Stepper::setDirection(bool forward) {
    bool dir_level = forward;
    if (ELS_INVERT_DIR) dir_level = !dir_level;
    digitalWrite(ELS_DIR_PIN, dir_level ? HIGH : LOW);
}

void Stepper::step(int32_t count) {
    if (count == 0) return;
    
    // Set direction
    bool forward = (count > 0);
    setDirection(forward);
    
    // Allow direction settle time
    delayMicroseconds(2);
    
    int32_t steps = abs(count);
    
    // Limit steps per call to prevent blocking too long
    if (steps > ELS_MAX_STEPS_PER_CYCLE) {
        steps = ELS_MAX_STEPS_PER_CYCLE;
    }

    if (!rmt_ready) return;

    static rmt_data_t rmt_buf[ELS_RMT_CHUNK_STEPS];
    static bool rmt_buf_init = false;
    if (!rmt_buf_init) {
        rmt_data_t pulse = {};
        pulse.level0 = 1;
        pulse.duration0 = ELS_PULSE_US;
        pulse.level1 = 0;
        pulse.duration1 = ELS_PULSE_US;
        for (int32_t i = 0; i < ELS_RMT_CHUNK_STEPS; i++) {
            rmt_buf[i] = pulse;
        }
        rmt_buf_init = true;
    }

    int32_t remaining = steps;
    while (remaining > 0) {
        int32_t chunk = remaining;
        if (chunk > ELS_RMT_CHUNK_STEPS) chunk = ELS_RMT_CHUNK_STEPS;
        (void)rmtWrite(ELS_STEP_PIN, rmt_buf, (size_t)chunk, RMT_WAIT_FOR_EVER);
        remaining -= chunk;
    }
    
    // Update position
    position += forward ? steps : -steps;
}
