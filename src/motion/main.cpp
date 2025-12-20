// ============================================================================
// Motion Controller Main (ESP32)
// Handles: Encoders (X, Z, C/Spindle), Stepper output, ELS sync
// ============================================================================

#include <Arduino.h>
#include "config_motion.h"
#include "shared/protocol.h"
#include "spi_slave.h"
#include "encoder_motion.h"
#include "stepper.h"
#include "els_core.h"

// ============================================================================
// Motion task runs on Core 1 for deterministic timing
// ============================================================================
static TaskHandle_t motion_task_handle = nullptr;

static void motionTask(void *param) {
    (void)param;
    
    TickType_t last_wake = xTaskGetTickCount();
    
    while (true) {
        // Update encoders
        EncoderMotion::update();
        
        // Run ELS core logic (calculates and outputs steps)
        ElsCore::update();
        
        // Run at ~20kHz (50us period) for smooth step generation
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1));
    }
}

// ============================================================================
// Setup
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.printf("\n[Motion] Boot: %s %s\n", __DATE__, __TIME__);
    
    // Initialize encoders
    if (!EncoderMotion::init()) {
        Serial.println("[Motion] Encoder init FAILED");
    } else {
        Serial.println("[Motion] Encoders OK");
    }
    
    // Initialize stepper output
    if (!Stepper::init()) {
        Serial.println("[Motion] Stepper init FAILED");
    } else {
        Serial.println("[Motion] Stepper OK");
    }
    
    // Initialize ELS core
    ElsCore::init();
    Serial.println("[Motion] ELS core OK");
    
    // Initialize SPI slave (for communication with UI board)
    if (!SpiSlave::init()) {
        Serial.println("[Motion] SPI slave init FAILED");
    } else {
        Serial.println("[Motion] SPI slave OK");
    }
    
    // Start motion task on Core 1 (high priority, uninterrupted)
    xTaskCreatePinnedToCore(
        motionTask,
        "motion",
        4096,
        nullptr,
        24,         // High priority
        &motion_task_handle,
        1           // Core 1
    );
    
    Serial.println("[Motion] Boot complete");
}

// ============================================================================
// Main loop (Core 0) - handles SPI communication
// ============================================================================

// Track previous command values for change detection
static bool prev_els_enabled = false;
static int32_t prev_pitch_um = 0;
static int8_t prev_direction_mul = 1;
static bool prev_endstop_min_en = false;
static bool prev_endstop_max_en = false;
static int32_t prev_endstop_min = 0;
static int32_t prev_endstop_max = 0;

void loop() {
    // Build status packet FIRST (before processing SPI)
    // This ensures TX buffer has fresh data when master initiates transaction
    StatusPacket status = {};
    status.version = PROTOCOL_VERSION;
    status.x_count = EncoderMotion::getXCount();
    status.z_count = EncoderMotion::getZCount();
    status.c_count = EncoderMotion::getSpindleCount();
    status.z_steps = Stepper::getPosition();
    status.rpm_signed = EncoderMotion::getRpmSigned();
    
    // Status flags
    status.flags.els_enabled = ElsCore::isEnabled();
    status.flags.els_fault = ElsCore::hasFault();
    status.flags.endstop_hit = ElsCore::endstopTriggered();
    status.flags.spindle_moving = (abs(status.rpm_signed) > 10);
    status.flags.comms_ok = SpiSlave::isConnected();
    
    // Update TX buffer immediately so it's ready when master polls
    SpiSlave::setStatus(status);
    
    // Now process any completed SPI transactions
    SpiSlave::process();
    
    // If we have a valid command from UI, update ELS settings
    if (SpiSlave::isConnected()) {
        const CommandPacket& cmd = SpiSlave::getCommand();
        
        bool els_en = (cmd.flags & 0x01);
        bool endstop_min_en = (cmd.endstop_min_enabled != 0);
        bool endstop_max_en = (cmd.endstop_max_enabled != 0);
        
#if DEBUG_SPI_LOGGING
        // Log changes from UI
        if (els_en != prev_els_enabled) {
            Serial.printf("[Motion] ELS %s (from UI)\n", els_en ? "ENABLED" : "DISABLED");
            prev_els_enabled = els_en;
        }
        if (cmd.pitch_um != prev_pitch_um) {
            Serial.printf("[Motion] Pitch changed: %ld um (%.3f mm)\n", 
                cmd.pitch_um, cmd.pitch_um / 1000.0f);
            prev_pitch_um = cmd.pitch_um;
        }
        if (cmd.direction_mul != prev_direction_mul) {
            Serial.printf("[Motion] Direction: %s\n", 
                cmd.direction_mul > 0 ? "NORMAL" : "REVERSE");
            prev_direction_mul = cmd.direction_mul;
        }
        if (endstop_min_en != prev_endstop_min_en || cmd.endstop_min_um != prev_endstop_min) {
            Serial.printf("[Motion] Endstop MIN: %s @ %ld um\n",
                endstop_min_en ? "ON" : "OFF", cmd.endstop_min_um);
            prev_endstop_min_en = endstop_min_en;
            prev_endstop_min = cmd.endstop_min_um;
        }
        if (endstop_max_en != prev_endstop_max_en || cmd.endstop_max_um != prev_endstop_max) {
            Serial.printf("[Motion] Endstop MAX: %s @ %ld um\n",
                endstop_max_en ? "ON" : "OFF", cmd.endstop_max_um);
            prev_endstop_max_en = endstop_max_en;
            prev_endstop_max = cmd.endstop_max_um;
        }
#endif
        
        // Update ELS state from command
        ElsCore::setEnabled(els_en);
        ElsCore::setPitchUm(cmd.pitch_um);
        ElsCore::setDirectionMul(cmd.direction_mul);
        ElsCore::setEndstops(
            cmd.endstop_min_um, 
            cmd.endstop_max_um,
            endstop_min_en,
            endstop_max_en
        );
    } else {
        // No communication - disable ELS for safety
        ElsCore::setEnabled(false);
    }
    
    // Small delay - SPI handling doesn't need to be super fast
    delay(1);
}
