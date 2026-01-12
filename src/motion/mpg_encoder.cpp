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
        // Non-linear mapping using shared config constants.
        // Fine control at low RPM, coarse at high RPM to mask timing quantization.
        //
        // Calculate position thresholds from RPM segment config
        static constexpr int32_t SEG1_RPM_RANGE = RPM_SEG1_END;
        static constexpr int32_t SEG2_RPM_RANGE = RPM_SEG2_END - RPM_SEG1_END;
        static constexpr int32_t SEG3_RPM_RANGE = RPM_SEG3_END - RPM_SEG2_END;
        static constexpr int32_t SEG4_RPM_RANGE = RPM_SEG4_END - RPM_SEG3_END;
        
        // Position counts needed for each segment (RPM * 10 / step_x10)
        static constexpr int32_t SEG1_COUNTS = (SEG1_RPM_RANGE * 10) / RPM_SEG1_STEP_X10;
        static constexpr int32_t SEG2_COUNTS = (SEG2_RPM_RANGE * 10) / RPM_SEG2_STEP_X10;
        static constexpr int32_t SEG3_COUNTS = (SEG3_RPM_RANGE * 10) / RPM_SEG3_STEP_X10;
        static constexpr int32_t SEG4_COUNTS = (SEG4_RPM_RANGE * 10) / RPM_SEG4_STEP_X10;
        
        static constexpr int32_t SEG1_MAX_POS = SEG1_COUNTS;
        static constexpr int32_t SEG2_MAX_POS = SEG1_MAX_POS + SEG2_COUNTS;
        static constexpr int32_t SEG3_MAX_POS = SEG2_MAX_POS + SEG3_COUNTS;
        static constexpr int32_t SEG4_MAX_POS = SEG3_MAX_POS + SEG4_COUNTS;
        static constexpr int32_t MPG_MAX_POS = SEG4_MAX_POS;
        
        noInterrupts();
        if (position < 0) position = 0;
        if (position > MPG_MAX_POS) position = MPG_MAX_POS;
        int32_t pos = position;
        interrupts();

        int32_t rpm_x10 = 0;
        if (pos <= SEG1_MAX_POS) {
            rpm_x10 = pos * RPM_SEG1_STEP_X10;
        } else if (pos <= SEG2_MAX_POS) {
            rpm_x10 = (RPM_SEG1_END * 10) + (pos - SEG1_MAX_POS) * RPM_SEG2_STEP_X10;
        } else if (pos <= SEG3_MAX_POS) {
            rpm_x10 = (RPM_SEG2_END * 10) + (pos - SEG2_MAX_POS) * RPM_SEG3_STEP_X10;
        } else {
            rpm_x10 = (RPM_SEG3_END * 10) + (pos - SEG3_MAX_POS) * RPM_SEG4_STEP_X10;
        }
        if (rpm_x10 < 0) rpm_x10 = 0;
        if (rpm_x10 > (int32_t)SPINDLE_MAX_RPM * 10) rpm_x10 = (int32_t)SPINDLE_MAX_RPM * 10;
        rpm_setting = (int16_t)rpm_x10;
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
