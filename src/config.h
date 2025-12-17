#pragma once

/* =========================
   Board wiring (JC4827W543)
   ========================= */
static constexpr int SCREEN_W = 480;
static constexpr int SCREEN_H = 272;

static constexpr int LCD_BL_PIN = 1;

// QSPI LCD (NV3041A)
static constexpr int TFT_CS  = 45;
static constexpr int TFT_SCK = 47;
static constexpr int TFT_D0  = 21;
static constexpr int TFT_D1  = 48;
static constexpr int TFT_D2  = 40;
static constexpr int TFT_D3  = 39;

// GT911 touch (I2C)
static constexpr int TOUCH_SCL = 4;
static constexpr int TOUCH_SDA = 8;
static constexpr int TOUCH_RST = 38;
static constexpr int TOUCH_INT = 3;
static constexpr uint8_t TOUCH_ADDR = 0x5D; // if dead, try 0x14

// Encoder pins
static constexpr int C_PINA = 15; // encoder A
static constexpr int C_PINB = 16; // encoder B

// Linear encoder pins (quadrature)
static constexpr int X_PINA = 6;
static constexpr int X_PINB = 7;
static constexpr int Z_PINA = 17;
static constexpr int Z_PINB = 18;

// Electronic leadscrew (step/dir stepper driver)
// NOTE: adjust these pins to match your driver wiring.
// Using only STEP/DIR by default so you can reserve a pin for ADC/button matrix.
static constexpr int ELS_STEP_PIN = 9;
static constexpr int ELS_DIR_PIN  = 14;
// Set to -1 if your driver is always-enabled / enable is wired permanently.
static constexpr int ELS_EN_PIN   = -1;

// Most drivers use active-low enable.
static constexpr bool ELS_EN_ACTIVE_LOW = true;
// Set true if DIR sense is opposite of what you expect.
static constexpr bool ELS_INVERT_DIR = false;

/* =========================
   Configuration constants
   ========================= */
static constexpr int  LCD_ROTATION     = 2;     // 180°
static constexpr bool TOUCH_ROTATE_180 = true;  // invert X/Y

// Tool system
static constexpr int TOOL_COUNT = 8;

// Work offset system (G54/G55 style)
static constexpr int OFFSET_COUNT = 3;

// Encoder configuration
static constexpr int32_t C_COUNTS_PER_REV = 1600;

// ELS gearing configuration
// Stepper: 1600 steps/rev (per your motor/driver setup)
static constexpr int32_t ELS_STEPS_PER_REV = 1600;
// Leadscrew: 2mm pitch => 2000um per rev
static constexpr int32_t ELS_LEADSCREW_PITCH_UM = 2000;
// Step pulse width (us)
static constexpr int32_t ELS_PULSE_US = 2;
// Cap steps per update cycle to avoid extreme bursts if we fall behind
static constexpr int32_t ELS_MAX_STEPS_PER_CYCLE = 800;

// Use ESP-IDF RMT for precise step pulse timing (recommended)
static constexpr bool     ELS_USE_RMT = true;
static constexpr uint32_t ELS_RMT_RES_HZ = 1000000; // 1 MHz tick -> 1us resolution
static constexpr int32_t  ELS_RMT_CHUNK_STEPS = 200; // keep small to reduce phase lag and allow DIR changes

// Linear scales (initial guess; tweak after testing)
// Assumption: one decoded quadrature count corresponds to 5 microns.
static constexpr int32_t X_UM_PER_COUNT = 5;
static constexpr int32_t Z_UM_PER_COUNT = 5;

// Set to true if direction is reversed after wiring.
static constexpr bool X_INVERT_DIR = false;
static constexpr bool Z_INVERT_DIR = false;

// RPM switching thresholds (add hysteresis to avoid flicker)
static constexpr int RPM_SHOW_RPM_ON  = 60; // above this -> show RPM on C row
static constexpr int RPM_SHOW_RPM_OFF = 45; // below this -> show degrees on C row

// PCNT limits (keep comfortably below int16 limits)
static constexpr int16_t PCNT_H_LIM = 12000;
static constexpr int16_t PCNT_L_LIM = -12000;