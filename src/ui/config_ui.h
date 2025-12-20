#pragma once

// ============================================================================
// UI board (ESP32-S3 JC4827W543) pin definitions and configuration
// ============================================================================

#include "shared/config_shared.h"

// Screen dimensions
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
static constexpr uint8_t TOUCH_ADDR = 0x5D;  // if dead, try 0x14

// LCD rotation and touch settings
static constexpr int  LCD_ROTATION     = 2;     // 180°
static constexpr bool TOUCH_ROTATE_180 = true;  // invert X/Y

// SPI Master pins (directly connected to Motion board)
// Using available GPIO bank on JC4827W543
static constexpr int SPI_MASTER_MOSI = 9;
static constexpr int SPI_MASTER_MISO = 46;  // GPIO46 is input-only, perfect for MISO
static constexpr int SPI_MASTER_CLK  = 14;
static constexpr int SPI_MASTER_CS   = 5;
