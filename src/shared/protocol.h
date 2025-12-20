#pragma once

#include <stdint.h>
#include <stddef.h>

// ============================================================================
// SPI Protocol between UI (ESP32-S3) and Motion (ESP32) boards
// ============================================================================

// Fixed packet size for SPI DMA transfers (must match on both sides)
static constexpr size_t PROTOCOL_PACKET_SIZE = 32;

// Protocol version for compatibility checking
static constexpr uint8_t PROTOCOL_VERSION = 2;

// ============================================================================
// MPG Mode (Manual Pulse Generator routing)
// ============================================================================
enum class MpgModeProto : uint8_t
{
	RPM_CONTROL = 0, // Default: MPG controls spindle RPM
	JOG_Z = 1,		 // MPG jogs Z axis
	JOG_C = 2,		 // MPG jogs C axis (spindle position)
};

// ============================================================================
// Commands from UI → Motion
// ============================================================================
enum class MotionCommand : uint8_t
{
	NOP = 0,		 // No operation, just exchange status
	SET_CONFIG,		 // Update pitch/direction/endstops
	ENABLE_ELS,		 // Enable electronic leadscrew
	DISABLE_ELS,	 // Disable electronic leadscrew
	RESET_POSITION,	 // Reset encoder counts to zero
	SET_ENDSTOP_MIN, // Set minimum endstop
	SET_ENDSTOP_MAX, // Set maximum endstop
	CLEAR_ENDSTOPS,	 // Clear endstop limits
	SYNC_REQUEST,	 // Request full state sync
	SET_MPG_MODE,	 // Set MPG routing mode (RPM/Z jog/C jog)
};

// ============================================================================
// Status flags from Motion → UI
// ============================================================================
struct MotionStatusFlags {
    uint8_t els_enabled     : 1;  // ELS is currently running
    uint8_t els_fault       : 1;  // ELS stopped due to fault
    uint8_t endstop_hit     : 1;  // Soft endstop was triggered
    uint8_t spindle_moving  : 1;  // Spindle is rotating (RPM > threshold)
    uint8_t comms_ok        : 1;  // Communication healthy
	uint8_t mpg_mode : 2;		  // Current MPG mode (MpgModeProto)
	uint8_t reserved : 1;
};

// ============================================================================
// Command packet: UI → Motion (32 bytes)
// ============================================================================
struct __attribute__((packed)) CommandPacket {
    uint8_t version;              // Protocol version        [1]
    MotionCommand cmd;            // Command to execute      [1]
    uint8_t flags;                // Enable flags, direction [1]
    int8_t  direction_mul;        // +1 normal, -1 reverse   [1]
    
    int32_t pitch_um;             // Thread pitch in microns [4]
    int32_t endstop_min_um;       // Soft endstop min        [4]
    int32_t endstop_max_um;       // Soft endstop max        [4]
    
    uint8_t endstop_min_enabled;  // Min endstop active      [1]
    uint8_t endstop_max_enabled;  // Max endstop active      [1]
	MpgModeProto mpg_mode;		  // MPG routing mode        [1]

	uint8_t reserved[11];		  // Padding                 [11]
	uint8_t sequence;             // Packet sequence number  [1]
    uint8_t checksum;             // XOR checksum            [1]
};                                // Total: 32 bytes
static_assert(sizeof(CommandPacket) == PROTOCOL_PACKET_SIZE, "CommandPacket size mismatch");

// ============================================================================
// Status packet: Motion → UI (32 bytes)
// ============================================================================
struct __attribute__((packed)) StatusPacket {
    uint8_t version;              // Protocol version        [1]
    MotionStatusFlags flags;      // Status flags            [1]
    uint8_t fault_code;           // Fault code if any       [1]
    uint8_t reserved1;            // Padding                 [1]
    
    int32_t x_count;              // X encoder raw count     [4]
    int32_t z_count;              // Z encoder raw count     [4]
    int32_t c_count;              // Spindle total count     [4]
    int32_t z_steps;              // Stepper position        [4]
    
    int16_t rpm_signed;           // Spindle RPM with sign   [2]
	int16_t target_rpm;			  // Target RPM from MPG     [2]
	uint8_t reserved2[6];		  // Padding                 [6]

	uint8_t sequence;             // Echo of command seq     [1]
    uint8_t checksum;             // XOR checksum            [1]
};                                // Total: 32 bytes
static_assert(sizeof(StatusPacket) == PROTOCOL_PACKET_SIZE, "StatusPacket size mismatch");

// ============================================================================
// Checksum calculation
// ============================================================================
inline uint8_t protocolChecksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    // XOR all bytes except the last one (which is the checksum itself)
    for (size_t i = 0; i < len - 1; i++) {
        sum ^= data[i];
    }
    return sum;
}

inline bool protocolValidate(const uint8_t* data, size_t len) {
    if (len < 2) return false;
    return data[len - 1] == protocolChecksum(data, len);
}

inline void protocolSign(uint8_t* data, size_t len) {
    if (len < 1) return;
    data[len - 1] = protocolChecksum(data, len);
}
