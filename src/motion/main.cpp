// ============================================================================
// Motion Controller Main (ESP32)
// Handles: Encoders (X, Z), Spindle stepper, ELS sync
// ============================================================================

#include <Arduino.h>
#include "config_motion.h"
#include "shared/protocol.h"
#include "spi_slave.h"
#include "encoder_motion.h"
#include "stepper.h"
#include "els_core.h"
#include "ota_motion.h"
#include "spindle_stepper.h"
#include "mpg_encoder.h"

// ============================================================================
// Motion task runs on Core 1 for deterministic timing
// ============================================================================
static TaskHandle_t motion_task_handle = nullptr;

static void motionTask(void *param) {
    (void)param;
    
    TickType_t last_wake = xTaskGetTickCount();
    
    while (true) {
		// Safety: Skip all motion control during OTA
		if (OtaMotion::isActive()) {
			SpindleStepper::stop();
			ElsCore::setEnabled(false);
			vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
			continue;
		}

		// Update linear encoders (X, Z)
		EncoderMotion::update();

		// Update MPG encoder
		MpgEncoder::update();

		// Handle MPG jog modes - jog and spindle run are mutually exclusive
		MpgMode mpg_mode = MpgEncoder::getMode();
		if (mpg_mode == MpgMode::JOG_Z)
		{
			// Route MPG delta to Z stepper
			// Scale: 1 MPG count = 1 Z step (direct 1:1 for fine control)
			int32_t delta = MpgEncoder::getDelta();
			if (delta != 0 && !ElsCore::isEnabled())
			{
				Stepper::step(delta);
			}
		}
		else if (mpg_mode == MpgMode::JOG_C)
		{
			// Route MPG delta to spindle stepper (spindle run disabled in jog mode)
			// Scale: 1 MPG count = 2 spindle steps (1600 steps/rev, so 800 counts = 1 rev)
			int32_t delta = MpgEncoder::getDelta();
			if (delta != 0)
			{
				SpindleStepper::stepImmediate(delta * 2);
			}
		}

		// Update spindle stepper (read switch, generate steps) - only when not in JOG_C mode
		if (mpg_mode != MpgMode::JOG_C)
		{
			SpindleStepper::update();
		}

		// Run ELS core logic (calculates and outputs steps)
        ElsCore::update();

		// Run at ~1kHz for responsive MPG control
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

	// Initialize linear encoders (X, Z)
	if (!EncoderMotion::init()) {
        Serial.println("[Motion] Encoder init FAILED");
    } else {
        Serial.println("[Motion] Encoders OK");
    }

	// Initialize MPG encoder (for speed control and jogging)
	if (!MpgEncoder::init())
	{
		Serial.println("[Motion] MPG encoder init FAILED");
	}
	else
	{
		Serial.println("[Motion] MPG encoder OK");
	}

	// Initialize spindle stepper driver
	if (!SpindleStepper::init())
	{
		Serial.println("[Motion] Spindle stepper init FAILED");
	}
	else
	{
		Serial.println("[Motion] Spindle stepper OK");
	}

	// Initialize stepper output (for Z axis / ELS)
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
static bool prev_sync_enabled = false;
static int32_t prev_sync_z = 0;
static uint8_t prev_cmd_sequence = 0xFF;

void loop() {
	OtaMotion::handle();

    // Build status packet FIRST (before processing SPI)
    // This ensures TX buffer has fresh data when master initiates transaction
    StatusPacket status = {};
    status.version = PROTOCOL_VERSION;
    status.x_count = EncoderMotion::getXCount();
    status.z_count = EncoderMotion::getZCount();
	status.z_steps = Stepper::getPosition();

	// Spindle data from stepper
	status.c_count = SpindleStepper::getPosition();
	status.rpm_signed = SpindleStepper::getRpmSigned();
	status.target_rpm = MpgEncoder::getRpmSetting();
	status.flags.mpg_mode = static_cast<uint8_t>(MpgEncoder::getMode());

	// Status flags
    status.flags.els_enabled = ElsCore::isEnabled();
    status.flags.els_fault = ElsCore::hasFault();
    status.flags.endstop_hit = ElsCore::endstopTriggered();
    status.flags.spindle_moving = SpindleStepper::isRunning();
    status.flags.comms_ok = SpiSlave::isConnected();
	status.flags.sync_waiting = ElsCore::isSyncWaiting();
	status.ota_active = OtaMotion::isActive() ? 1 : 0;
	status.wifi_connected = OtaMotion::isWifiConnected() ? 1 : 0;
	status.sync_speed_scale_permille = ElsCore::getSyncSpeedScalePermille();
	status.sync_abs_error_um = ElsCore::getSyncAbsErrorUm();
	status.sync_state = SyncStateProto::SYNC_DISABLED;
	if (ElsCore::isSyncEnabled()) {
		if (!ElsCore::isEnabled()) status.sync_state = SyncStateProto::SYNC_OUT_OF_SYNC;
		else if (ElsCore::isSyncIn()) status.sync_state = SyncStateProto::SYNC_IN_SYNC;
		else status.sync_state = SyncStateProto::SYNC_OUT_OF_SYNC;
	}
    
    // Update TX buffer immediately so it's ready when master polls
    SpiSlave::setStatus(status);
    
    // Now process any completed SPI transactions
    SpiSlave::process();
    
    // If we have a valid command from UI, update ELS settings
    if (SpiSlave::isConnected()) {
        const CommandPacket& cmd = SpiSlave::getCommand();
		if (cmd.ota_request != 0)
		{
			OtaMotion::start();
		}
		if (cmd.reboot_request != 0)
		{
			ESP.restart();
		}

		// Handle spindle toggle command from UI
		if (cmd.sequence != prev_cmd_sequence) {
			prev_cmd_sequence = cmd.sequence;
			if (cmd.cmd == MotionCommand::SPINDLE_TOGGLE_FWD) {
				// Only allow spindle toggle when not in JOG_C mode
				if (MpgEncoder::getMode() != MpgMode::JOG_C) {
					SpindleStepper::queueSoftToggle(1);
				}
			}
		}
        
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
		if (cmd.sync_enabled != prev_sync_enabled || cmd.sync_z_um != prev_sync_z) {
			Serial.printf("[Motion] Sync: %s (Z0 ref=%ld um)\n",
				cmd.sync_enabled ? "ON" : "OFF", cmd.sync_z_um);
			prev_sync_enabled = (cmd.sync_enabled != 0);
			prev_sync_z = cmd.sync_z_um;
		}
#endif
        
        // Update ELS state from command
		if (OtaMotion::isActive())
		{
			ElsCore::setEnabled(false);
			ElsCore::setJog(0, false);
		}
		else
		{
			ElsCore::setEnabled(els_en);
			ElsCore::setPitchUm(cmd.pitch_um);
			ElsCore::setDirectionMul(cmd.direction_mul);
			ElsCore::setJog(cmd.jog_dir, cmd.jog_active != 0);
		}
		ElsCore::setSync(cmd.sync_enabled != 0, cmd.sync_z_um, cmd.sync_c_ticks);
        ElsCore::setEndstops(
            cmd.endstop_min_um, 
            cmd.endstop_max_um,
            endstop_min_en,
            endstop_max_en
        );

		// Handle MPG mode changes from UI
		if (!OtaMotion::isActive())
		{
			MpgModeProto requestedMode = static_cast<MpgModeProto>(cmd.mpg_mode);
			MpgMode newMode = static_cast<MpgMode>(requestedMode);
			
			// When switching to JOG_C, stop the spindle
			if (newMode == MpgMode::JOG_C && MpgEncoder::getMode() != MpgMode::JOG_C) {
				SpindleStepper::stop();
			}
			
			MpgEncoder::setMode(newMode);
		}
	} else {
        // No communication - disable ELS for safety
        ElsCore::setEnabled(false);
		ElsCore::setJog(0, false);
		prev_cmd_sequence = 0xFF;
    }
    
    // Small delay - SPI handling doesn't need to be super fast
    delay(1);
}
