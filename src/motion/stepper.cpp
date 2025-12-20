#include "stepper.h"
#include "config_motion.h"
#include <Arduino.h>

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
    
    // For now, just use GPIO bit-bang (RMT can be added later for better timing)
    rmt_ready = false;
    
    Serial.println("[Stepper] GPIO mode initialized");
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
    
    // GPIO bit-bang step generation
    for (int32_t i = 0; i < steps; i++) {
        digitalWrite(ELS_STEP_PIN, HIGH);
        delayMicroseconds(ELS_PULSE_US);
        digitalWrite(ELS_STEP_PIN, LOW);
        delayMicroseconds(ELS_PULSE_US);
    }
    
    // Update position
    position += forward ? steps : -steps;
}
