#pragma once

// ============================================================================
// Shared configuration constants used by both UI and Motion boards
// ============================================================================

// Debug logging (set to 1 to enable verbose SPI/state change logging)
#define DEBUG_SPI_LOGGING 1

// Tool system
static constexpr int TOOL_COUNT = 8;

// Work offset system (G54/G55 style)
static constexpr int OFFSET_COUNT = 3;

// ============================================================================
// Spindle Configuration
// ============================================================================
// Encoder mode: external motor with quadrature encoder feedback
// Stepper mode: ESP32-driven stepper motor (step count = position)

// Spindle counts per revolution (applies to both modes)
// For encoder: quadrature counts per rev of the physical encoder
// For stepper: steps per rev of the stepper motor
static constexpr int32_t C_COUNTS_PER_REV = 1600;

// ELS gearing configuration
// Stepper: 1600 steps/rev (per your motor/driver setup)
static constexpr int32_t ELS_STEPS_PER_REV = 1600;
// Leadscrew: 2mm pitch => 2000um per rev
static constexpr int32_t ELS_LEADSCREW_PITCH_UM = 2000;

// Linear scales: one decoded quadrature count corresponds to N microns
static constexpr int32_t X_UM_PER_COUNT = 5;
static constexpr int32_t Z_UM_PER_COUNT = 5;

// RPM switching thresholds (add hysteresis to avoid flicker)
static constexpr int RPM_SHOW_RPM_ON  = 60;  // above this -> show RPM on C row
static constexpr int RPM_SHOW_RPM_OFF = 45;  // below this -> show degrees on C row

// SPI communication settings
// 64 bytes @ 1 MHz = ~512 µs per transaction, plenty fast for 100 Hz polling
static constexpr uint32_t SPI_CLOCK_HZ = 1000000;   // 1 MHz SPI clock (reliable over wires)
static constexpr uint32_t SPI_POLL_INTERVAL_MS = 10; // Poll motion board every 10ms (100 Hz)
