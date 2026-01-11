#include "mpg_encoder.h"
#include "config_motion.h"
#include <Arduino.h>

// ============================================================================
// Static member initialization
// ============================================================================
volatile int32_t MpgEncoder::position = 0;
volatile int32_t MpgEncoder::delta_accum = 0;
volatile uint8_t MpgEncoder::last_state = 0;
int16_t MpgEncoder::rpm_setting = 0;
MpgMode MpgEncoder::mode = MpgMode::RPM_CONTROL;

// Quadrature state table for decoding
// Index = (old_state << 2) | new_state
static const int8_t QUAD_TABLE[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

// ============================================================================
// ISR handlers
// ============================================================================
void IRAM_ATTR MpgEncoder::processQuadrature() {
    uint8_t a = digitalRead(MPG_PINA) ? 1 : 0;
    uint8_t b = digitalRead(MPG_PINB) ? 1 : 0;
    uint8_t new_state = (a << 1) | b;
    
    uint8_t idx = (last_state << 2) | new_state;
    int8_t delta = QUAD_TABLE[idx];
    
    if (delta != 0) {
        if (MPG_INVERT_DIR) delta = -delta;
        position += delta;
        delta_accum += delta;
    }
    
    last_state = new_state;
}

void IRAM_ATTR MpgEncoder::isrA() {
    processQuadrature();
}

void IRAM_ATTR MpgEncoder::isrB() {
    processQuadrature();
}

// ============================================================================
// Initialization
// ============================================================================
bool MpgEncoder::init() {
    // Configure pins as inputs with internal pullups
    // MPG encoders typically have open-collector outputs
    pinMode(MPG_PINA, INPUT_PULLUP);
    pinMode(MPG_PINB, INPUT_PULLUP);
    
    // Read initial state
    uint8_t a = digitalRead(MPG_PINA) ? 1 : 0;
    uint8_t b = digitalRead(MPG_PINB) ? 1 : 0;
    last_state = (a << 1) | b;
    
    // Attach interrupts
    attachInterrupt(MPG_PINA, isrA, CHANGE);
    attachInterrupt(MPG_PINB, isrB, CHANGE);
    
    position = 0;
    delta_accum = 0;
    rpm_setting = 0;
    mode = MpgMode::RPM_CONTROL;
    
    Serial.printf("[MPG] Initialized: A=%d, B=%d, %ld counts = %ld RPM max\n",
        MPG_PINA, MPG_PINB, MPG_COUNTS_TO_MAX_RPM, SPINDLE_MAX_RPM);
    
    return true;
}

// ============================================================================
// Get delta counts since last call (atomically)
// ============================================================================
int32_t MpgEncoder::getDelta() {
    noInterrupts();
    int32_t d = delta_accum;
    delta_accum = 0;
    interrupts();
    return d;
}

// ============================================================================
// Update - process position for RPM control
// ============================================================================
void MpgEncoder::update() {
    // In RPM control mode, map position to RPM
    if (mode == MpgMode::RPM_CONTROL) {
        // Clamp position to valid range.
        // We now treat 1 MPG count as 0.1 RPM, so full-scale to 3000.0 RPM
        // requires position up to SPINDLE_MAX_RPM * 10.
        static constexpr int32_t MPG_MAX_POS = (int32_t)SPINDLE_MAX_RPM * 10;
        noInterrupts();
        if (position < 0) position = 0;
        if (position > MPG_MAX_POS) position = MPG_MAX_POS;
        int32_t pos = position;
        interrupts();
        
        // Mapping: 1 count = 0.1 RPM
        rpm_setting = (int16_t)pos;
    }
    // In jog modes, position is unbounded and delta is consumed by stepper
}

// ============================================================================
// Mode control
// ============================================================================
void MpgEncoder::setMode(MpgMode m) {
    if (m != mode) {
        // When switching modes, reset delta but preserve position for RPM
        noInterrupts();
        delta_accum = 0;
        if (m != MpgMode::RPM_CONTROL) {
            // Entering jog mode - don't reset RPM position
        }
        interrupts();
        mode = m;
        
        Serial.printf("[MPG] Mode changed to %d\n", (int)m);
    }
}
