#include "spi_master.h"
#include "config_ui.h"
#include <Arduino.h>
#include <SPI.h>

// Static member initialization
StatusPacket SpiMaster::last_status = {};
bool SpiMaster::connected = false;
uint32_t SpiMaster::last_success_ms = 0;
uint8_t SpiMaster::sequence = 0;

bool SpiMaster::els_enabled = false;
int32_t SpiMaster::pitch_um = 1000;  // Default 1mm pitch
int8_t SpiMaster::direction_mul = 1;
int32_t SpiMaster::endstop_min_um = 0;
int32_t SpiMaster::endstop_max_um = 0;
bool SpiMaster::endstop_min_enabled = false;
bool SpiMaster::endstop_max_enabled = false;
MpgModeProto SpiMaster::mpg_mode = MpgModeProto::RPM_CONTROL;

// Use HSPI for communication with motion board
static SPIClass hspi(HSPI);

bool SpiMaster::init() {
    pinMode(SPI_MASTER_CS, OUTPUT);
    digitalWrite(SPI_MASTER_CS, HIGH);  // Deselect
    
    hspi.begin(SPI_MASTER_CLK, SPI_MASTER_MISO, SPI_MASTER_MOSI, SPI_MASTER_CS);
    
    Serial.println("[SPI] Master initialized");
    return true;
}

bool SpiMaster::transact(const CommandPacket& cmd, StatusPacket& status) {
    // Prepare command with checksum
    CommandPacket cmd_signed = cmd;
    protocolSign((uint8_t*)&cmd_signed, sizeof(cmd_signed));
    
    // Clear status buffer
    memset(&status, 0, sizeof(status));
    
    // Small delay to ensure slave has prepared TX buffer
    delayMicroseconds(50);
    
    // Perform full-duplex transfer
    hspi.beginTransaction(SPISettings(SPI_CLOCK_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(SPI_MASTER_CS, LOW);
    delayMicroseconds(10);  // Setup time for slave
    
    hspi.transferBytes(
        (uint8_t*)&cmd_signed,
        (uint8_t*)&status,
        PROTOCOL_PACKET_SIZE
    );
    
    delayMicroseconds(10);  // Hold time
    digitalWrite(SPI_MASTER_CS, HIGH);
    hspi.endTransaction();
    
    // Validate response
    if (!protocolValidate((uint8_t*)&status, sizeof(status))) {
        static uint32_t last_err = 0;
        if (millis() - last_err > 1000) {
            last_err = millis();
            Serial.printf("[SPI] Checksum fail: ver=%d, got_chk=0x%02X, calc=0x%02X\n",
                status.version, 
                ((uint8_t*)&status)[PROTOCOL_PACKET_SIZE-1],
                protocolChecksum((uint8_t*)&status, sizeof(status)));
        }
        return false;
    }
    
    if (status.version != PROTOCOL_VERSION) {
        static uint32_t last_ver_err = 0;
        if (millis() - last_ver_err > 1000) {
            last_ver_err = millis();
            uint8_t* raw = (uint8_t*)&status;
            Serial.printf("[SPI] Version mismatch: got %d, expected %d\n", 
                status.version, PROTOCOL_VERSION);
            Serial.printf("[SPI] Raw bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7]);
        }
        return false;
    }
    
    return true;
}

void SpiMaster::buildCommand(CommandPacket& cmd) {
    memset(&cmd, 0, sizeof(cmd));
    cmd.version = PROTOCOL_VERSION;
    cmd.cmd = MotionCommand::NOP;
    cmd.flags = els_enabled ? 1 : 0;
    cmd.direction_mul = direction_mul;
    cmd.pitch_um = pitch_um;
    cmd.endstop_min_um = endstop_min_um;
    cmd.endstop_max_um = endstop_max_um;
    cmd.endstop_min_enabled = endstop_min_enabled ? 1 : 0;
    cmd.endstop_max_enabled = endstop_max_enabled ? 1 : 0;
	cmd.mpg_mode = mpg_mode;
	cmd.sequence = sequence++;
}

bool SpiMaster::poll() {
    CommandPacket cmd;
    buildCommand(cmd);
    
    StatusPacket status;
    bool ok = transact(cmd, status);
    
    if (ok) {
#if DEBUG_SPI_LOGGING
        // Log significant status changes from motion board
        static bool prev_els_enabled = false;
        static bool prev_els_fault = false;
        static bool prev_endstop_hit = false;
        
        if (status.flags.els_enabled != prev_els_enabled) {
            Serial.printf("[Motion->UI] ELS is %s\n", 
                status.flags.els_enabled ? "RUNNING" : "STOPPED");
            prev_els_enabled = status.flags.els_enabled;
        }
        if (status.flags.els_fault && !prev_els_fault) {
            Serial.printf("[Motion->UI] ELS FAULT! Code: %d\n", status.fault_code);
        }
        prev_els_fault = status.flags.els_fault;
        
        if (status.flags.endstop_hit && !prev_endstop_hit) {
            Serial.println("[Motion->UI] ENDSTOP HIT!");
        }
        prev_endstop_hit = status.flags.endstop_hit;
#endif
        last_status = status;
        last_success_ms = millis();
        connected = true;
    } else {
        // Check for timeout
        if (millis() - last_success_ms > 1000) {
            connected = false;
        }
    }
    
    return ok;
}

void SpiMaster::setElsEnabled(bool enabled) {
#if DEBUG_SPI_LOGGING
    if (enabled != els_enabled) {
        Serial.printf("[UI->Motion] ELS %s\n", enabled ? "ENABLE" : "DISABLE");
    }
#endif
    els_enabled = enabled;
}

void SpiMaster::setPitchUm(int32_t pitch) {
#if DEBUG_SPI_LOGGING
    if (pitch != pitch_um) {
        Serial.printf("[UI->Motion] Pitch: %ld um (%.3f mm)\n", pitch, pitch / 1000.0f);
    }
#endif
    pitch_um = pitch;
}

void SpiMaster::setDirectionMul(int8_t mul) {
    int8_t new_mul = (mul < 0) ? -1 : 1;
#if DEBUG_SPI_LOGGING
    if (new_mul != direction_mul) {
        Serial.printf("[UI->Motion] Direction: %s\n", new_mul > 0 ? "NORMAL" : "REVERSE");
    }
#endif
    direction_mul = new_mul;
}

void SpiMaster::setEndstops(int32_t min_um, int32_t max_um, bool min_en, bool max_en) {
#if DEBUG_SPI_LOGGING
    if (min_en != endstop_min_enabled || min_um != endstop_min_um) {
        Serial.printf("[UI->Motion] Endstop MIN: %s @ %ld um\n", min_en ? "ON" : "OFF", min_um);
    }
    if (max_en != endstop_max_enabled || max_um != endstop_max_um) {
        Serial.printf("[UI->Motion] Endstop MAX: %s @ %ld um\n", max_en ? "ON" : "OFF", max_um);
    }
#endif
    endstop_min_um = min_um;
    endstop_max_um = max_um;
    endstop_min_enabled = min_en;
    endstop_max_enabled = max_en;
}

void SpiMaster::setMpgMode(MpgModeProto mode)
{
#if DEBUG_SPI_LOGGING
	if (mode != mpg_mode)
	{
		const char *mode_str = "RPM";
		if (mode == MpgModeProto::JOG_Z)
			mode_str = "JOG_Z";
		else if (mode == MpgModeProto::JOG_C)
			mode_str = "JOG_C";
		Serial.printf("[UI->Motion] MPG Mode: %s\n", mode_str);
	}
#endif
	mpg_mode = mode;
}