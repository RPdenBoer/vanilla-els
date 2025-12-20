#pragma once

// ============================================================================
// Motion board (ESP32) pin definitions and configuration
// ============================================================================

#include "shared/config_shared.h"

// ============================================================================
// SPINDLE MODE CONFIGURATION
// ============================================================================
// Choose ONE spindle mode:
//   SPINDLE_MODE_ENCODER - External DC motor with quadrature encoder feedback
//   SPINDLE_MODE_STEPPER - ESP32-driven stepper motor (step pulses = position)

#define SPINDLE_MODE_ENCODER 0
#define SPINDLE_MODE_STEPPER 1

// >>> SELECT YOUR SPINDLE MODE HERE <<<
#define SPINDLE_MODE SPINDLE_MODE_STEPPER

// ============================================================================
// Spindle Encoder Mode Configuration (SPINDLE_MODE_ENCODER)
// ============================================================================
// Spindle encoder pins (quadrature, uses PCNT)
static constexpr int C_PINA = 15;  // encoder A
static constexpr int C_PINB = 16;  // encoder B

// ============================================================================
// Spindle Stepper Mode Configuration (SPINDLE_MODE_STEPPER)
// ============================================================================
// Stepper output pins
static constexpr int SPINDLE_STEP_PIN = 15; // Reuse encoder pins when not in encoder mode
static constexpr int SPINDLE_DIR_PIN = 16;
static constexpr int SPINDLE_EN_PIN = -1; // -1 = no enable pin / always enabled

// Spindle stepper parameters
// 1600 steps/rev at 3000 RPM = 80kHz pulse rate (well within ESP32 capability)
static constexpr int32_t SPINDLE_STEPS_PER_REV = 1600;
static constexpr int32_t SPINDLE_MAX_RPM = 3000;
static constexpr int32_t SPINDLE_MIN_RPM = 10; // Minimum commanded RPM
static constexpr bool SPINDLE_INVERT_DIR = false;

// MPG (Manual Pulse Generator) encoder for speed control
// Quadrature encoder replaces potentiometer for precise RPM adjustment
static constexpr int MPG_PINA = 12;					  // 34;					  // MPG encoder A (input only pin)
static constexpr int MPG_PINB = 13;					  // 39;					  // MPG encoder B (input only pin, VN)
static constexpr int32_t MPG_COUNTS_TO_MAX_RPM = 800; // 800 counts = 0-3000 RPM range (200 PPR * 4 quadrature)
static constexpr bool MPG_INVERT_DIR = false;

// Direction switch inputs (active LOW with internal pullups)
// Both off = stopped, FWD on = forward, REV on = reverse
static constexpr int SPINDLE_FWD_PIN = 35; // Forward switch
static constexpr int SPINDLE_REV_PIN = 36; // Reverse switch (VP)

// Acceleration limit (RPM per second) - prevents jerky speed changes
static constexpr int32_t SPINDLE_ACCEL_RPM_PER_SEC = 500;

// ============================================================================
// Linear Encoders (always used)
// ============================================================================
// Linear encoder pins (quadrature, GPIO ISR)
static constexpr int X_PINA = 25;
static constexpr int X_PINB = 26;
static constexpr int Z_PINA = 27;
static constexpr int Z_PINB = 14;

// Set to true if direction is reversed after wiring
static constexpr bool X_INVERT_DIR = false;
static constexpr bool Z_INVERT_DIR = false;

// Electronic leadscrew (step/dir stepper driver)
static constexpr int ELS_STEP_PIN = 32;
static constexpr int ELS_DIR_PIN  = 33;
// Set to -1 if your driver is always-enabled / enable is wired permanently
static constexpr int ELS_EN_PIN   = -1;

// Most drivers use active-low enable
static constexpr bool ELS_EN_ACTIVE_LOW = true;
// Set true if DIR sense is opposite of what you expect
static constexpr bool ELS_INVERT_DIR = false;

// Step pulse width (us)
static constexpr int32_t ELS_PULSE_US = 2;
// Cap steps per update cycle to avoid extreme bursts if we fall behind
static constexpr int32_t ELS_MAX_STEPS_PER_CYCLE = 800;

// Use ESP-IDF RMT for precise step pulse timing (recommended)
static constexpr bool     ELS_USE_RMT = true;
static constexpr uint32_t ELS_RMT_RES_HZ = 1000000;  // 1 MHz tick -> 1us resolution
static constexpr int32_t  ELS_RMT_CHUNK_STEPS = 200; // keep small to reduce phase lag

// PCNT limits (keep comfortably below int16 limits)
static constexpr int16_t PCNT_H_LIM = 12000;
static constexpr int16_t PCNT_L_LIM = -12000;

// SPI Slave pins (directly connected to UI board)
static constexpr int SPI_SLAVE_MOSI = 23;
static constexpr int SPI_SLAVE_MISO = 19;
static constexpr int SPI_SLAVE_CLK  = 18;
static constexpr int SPI_SLAVE_CS   = 5;
