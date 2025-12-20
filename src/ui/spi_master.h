#pragma once

#include <stdint.h>
#include "shared/protocol.h"

// ============================================================================
// SPI Master: UI board talks to Motion board
// ============================================================================

class SpiMaster {
public:
    static bool init();
    
    // Perform a bidirectional transaction: send command, receive status
    // Returns true if transaction succeeded and checksum valid
    static bool transact(const CommandPacket& cmd, StatusPacket& status);
    
    // Convenience: poll with current state (updates internal status)
    static bool poll();
    
    // Get latest received status (updated by poll())
    static const StatusPacket& getStatus() { return last_status; }
    
    // Check communication health
    static bool isConnected() { return connected; }
    static uint32_t getLastSuccessMs() { return last_success_ms; }
    
    // Build a command packet with current settings
    static void buildCommand(CommandPacket& cmd);
    
    // Update internal state from UI settings (call before poll)
    static void setElsEnabled(bool enabled);
    static void setPitchUm(int32_t pitch_um);
    static void setDirectionMul(int8_t mul);
    static void setEndstops(int32_t min_um, int32_t max_um, bool min_en, bool max_en);
    
private:
    static StatusPacket last_status;
    static bool connected;
    static uint32_t last_success_ms;
    static uint8_t sequence;
    
    // Current command state
    static bool els_enabled;
    static int32_t pitch_um;
    static int8_t direction_mul;
    static int32_t endstop_min_um;
    static int32_t endstop_max_um;
    static bool endstop_min_enabled;
    static bool endstop_max_enabled;
};
