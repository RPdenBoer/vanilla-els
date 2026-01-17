#pragma once

// ============================================================================
// Motion board (ESP32) pin definitions and configuration
// ============================================================================

#include "shared/config_shared.h"

// ============================================================================
// Spindle Stepper Configuration
// ============================================================================
// Stepper output pins
static constexpr int SPINDLE_STEP_PIN = 16;
static constexpr int SPINDLE_DIR_PIN = 17;
static constexpr int SPINDLE_EN_PIN = -1; // -1 = no enable pin / always enabled

// Spindle stepper parameters
// 1600 steps/rev at 3000 RPM = 80kHz pulse rate (well within ESP32 capability)
static constexpr int32_t SPINDLE_STEPS_PER_REV = 1600;
static constexpr int32_t SPINDLE_MAX_RPM = 3000;
static constexpr int32_t SPINDLE_MIN_RPM = 1; // Minimum commanded RPM
static constexpr int32_t SPINDLE_JOG_RPM = 200; // Constant RPM for long-press jog
static constexpr uint32_t SPINDLE_JOG_PRESS_MS = 350; // Long-press threshold for jog
static constexpr bool SPINDLE_INVERT_DIR = false;
static constexpr uint32_t SPINDLE_PULSE_US = 2;          // Step pulse width
static constexpr uint32_t SPINDLE_RMT_RES_HZ = 10000000; // 10MHz (0.1us) tick for fine RPM resolution
static constexpr uint32_t SPINDLE_MIN_STEP_PERIOD_US = 12;   // ~83k steps/s cap
static constexpr uint32_t SPINDLE_MAX_STEP_PERIOD_US = 50000; // 20 Hz min

// MPG (Manual Pulse Generator) encoder for speed control and jogging
// Quadrature encoder for precise RPM adjustment and spindle positioning
// Uses GPIO with internal pullup support for simpler wiring.
static constexpr int MPG_PINA = 25;					  // MPG encoder A
static constexpr int MPG_PINB = 26;					  // MPG encoder B
static constexpr int32_t MPG_COUNTS_TO_MAX_RPM = 200 * 4 * 3; // 0-3000 RPM range (200 PPR * 4 quadrature)
static constexpr bool MPG_INVERT_DIR = false;

// MPG jog scaling
// - Z jog: number of Z stepper steps per MPG count
// - C jog: number of spindle stepper steps per MPG count
static constexpr int32_t MPG_JOG_Z_STEPS_PER_COUNT = 2;
static constexpr int32_t MPG_JOG_C_STEPS_PER_COUNT = 1;

// Direction switch inputs (active LOW, external pullups recommended)
// Both off = stopped, FWD on = forward, REV on = reverse
static constexpr int SPINDLE_FWD_PIN = 21; // Forward switch (input only pin)
static constexpr int SPINDLE_REV_PIN = 22; // Reverse switch (input only pin, VN)

// Acceleration limit (RPM per second) - prevents jerky speed changes
static constexpr int32_t SPINDLE_ACCEL_RPM_PER_SEC = 500;

// ============================================================================
// Linear Encoders (X and Z axes)
// ============================================================================
// Linear encoder pins (quadrature, GPIO ISR)
// GPIO 34/35 are input-only but linear encoders have built-in pull resistors.
static constexpr int X_PINA = 34;
static constexpr int X_PINB = 35;
static constexpr int Z_PINA = 27;
static constexpr int Z_PINB = 14;

// Set to true if direction is reversed after wiring
static constexpr bool X_INVERT_DIR = false;
static constexpr bool Z_INVERT_DIR = false;

// ============================================================================
// Electronic Leadscrew (Z axis stepper driver)
// ============================================================================
static constexpr int ELS_STEP_PIN = 32;
static constexpr int ELS_DIR_PIN  = 33;
// Set to -1 if your driver is always-enabled / enable is wired permanently
static constexpr int ELS_EN_PIN   = -1;

// Most drivers use active-low enable
static constexpr bool ELS_EN_ACTIVE_LOW = true;
// Set true if DIR sense is opposite of what you expect
static constexpr bool ELS_INVERT_DIR = true;

// Step pulse width (us)
static constexpr int32_t ELS_PULSE_US = 2;
// Cap steps per update cycle to avoid extreme bursts if we fall behind
static constexpr int32_t ELS_MAX_STEPS_PER_CYCLE = 800;
// Constant jog feed (Z axis), used for long-press jog buttons
static constexpr int32_t ELS_JOG_MM_PER_MIN = 1000;

// Use ESP-IDF RMT for precise step pulse timing (recommended)
static constexpr bool     ELS_USE_RMT = true;
static constexpr uint32_t ELS_RMT_RES_HZ = 1000000;  // 1 MHz tick -> 1us resolution
static constexpr int32_t  ELS_RMT_CHUNK_STEPS = 200; // keep small to reduce phase lag

// ============================================================================
// Continuous Sync (ELS speed trim)
// ============================================================================
// Sync runs continuously when enabled: it trims the ELS step output by a bounded
// multiplier to converge toward the theoretical Z/C 0/0 crossing reference.
//
// How often to update the trim controller (ms)
static constexpr uint32_t SYNC_ADJUST_INTERVAL_MS = 50;

// Don't adjust for very small errors (microns)
static constexpr int32_t SYNC_DEADBAND_UM = 25;

// Sync state hysteresis thresholds (microns)
static constexpr int32_t SYNC_IN_TOL_UM = 100;
static constexpr int32_t SYNC_OUT_TOL_UM = 200;

// Clamp speed trim multiplier range (percent of nominal)
static constexpr int32_t SYNC_SPEED_MIN_PCT = 5;   // 0.05x
static constexpr int32_t SYNC_SPEED_MAX_PCT = 200;  // 2.00x

// Proportional gain K = SYNC_K_NUM / SYNC_K_DEN applied to (z_error / pitch)
static constexpr int32_t SYNC_K_NUM = 1;
static constexpr int32_t SYNC_K_DEN = 4;

// Smoothing factor for the scale update: new += (target-new)/SYNC_SMOOTH_DEN
// Larger = slower response, less risk of oscillation.
static constexpr int32_t SYNC_SMOOTH_DEN = 4;

// ============================================================================
// SPI Slave pins (communication with UI board)
// ============================================================================
static constexpr int SPI_SLAVE_MOSI = 23;
static constexpr int SPI_SLAVE_MISO = 19;
static constexpr int SPI_SLAVE_CLK  = 18;
static constexpr int SPI_SLAVE_CS   = 13;

// ============================================================================
// Reserved pins (future X stepper axis)
// ============================================================================
// X step should use RMT-capable output for clean pulse timing.
// static constexpr int X_STEP_PIN = 21;
// static constexpr int X_DIR_PIN  = 22;
// static constexpr int X_EN_PIN   = -1; // set if enable line is needed

// Reserved pins (future ELS physical buttons)
// NOTE: GPIO1/3 are UART0. Using these will interfere with Serial logging/programming.
static constexpr int ELS_BTN_LEFT_PIN = 15;
static constexpr int ELS_BTN_RIGHT_PIN = 5;
