#pragma once

// ============================================================================
// Motion board (ESP32) pin definitions and configuration
// ============================================================================

#include "shared/config_shared.h"

// Spindle encoder pins (quadrature, uses PCNT)
static constexpr int C_PINA = 15;  // encoder A
static constexpr int C_PINB = 16;  // encoder B

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
