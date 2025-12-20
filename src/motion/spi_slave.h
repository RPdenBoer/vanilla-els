#pragma once

#include <stdint.h>
#include "shared/protocol.h"

// ============================================================================
// SPI Slave: Motion board receives commands from UI board
// ============================================================================

class SpiSlave {
public:
    static bool init();
    
    // Process any pending SPI transactions (call from main loop)
    static void process();
    
    // Get the latest received command
    static const CommandPacket& getCommand() { return last_command; }
    
    // Update status to be sent on next transaction
    static void setStatus(const StatusPacket& status);
    
    // Check if we have valid communication
    static bool isConnected() { return connected; }
    
    // Allow ISR callback to set pending flag
    static volatile bool transaction_pending;
    
private:
    static CommandPacket last_command;
    static StatusPacket current_status;
    static bool connected;
    static uint32_t last_rx_ms;
};
