# Copilot Instructions: Vanilla ELS (Electronic Leadscrew)

## Project Overview
This is an ESP32-S3 based Electronic Leadscrew (ELS) system for lathes using **PlatformIO** with Arduino framework. It provides a touchscreen DRO (Digital Readout) with precision position tracking for X/Z linear axes and C spindle rotation axis.

## Hardware Architecture
- **Board**: JC4827W543 (ESP32-S3 with 480x272 QSPI LCD + GT911 touch)
- **Display**: 480x272 NV3041A QSPI LCD (Arduino_GFX library) 
- **Touch**: GT911 capacitive touch controller (TouchLib, I2C at 0x5D)
- **Spindle Encoder**: Quadrature encoder on pins 15/16 using **ESP-IDF v5 pulse counter** (not ESP32Encoder library)
- **UI Framework**: LVGL v9 with custom coordinate system handling

## Critical Architecture Patterns

### Coordinate System & Offsets
The project implements a **3-tier offset system**: `display = raw - global - tool[current]`
- `x_raw_um`, `z_raw_um`: Raw sensor readings in microns
- `c_raw_ticks`: Raw spindle position (0-1599 per revolution)  
- Global offsets: `x_global_um`, `z_global_um`, `c_global_ticks`
- Tool offsets: `x_tool_um[TOOL_COUNT]`, `z_tool_um[TOOL_COUNT]`, `c_tool_ticks[TOOL_COUNT]`

### ESP-IDF Pulse Counter Implementation
**IMPORTANT**: This project uses **ESP-IDF v5 pulse counter APIs directly**, NOT the ESP32Encoder library dependency:
```cpp
// Uses driver/pulse_cnt.h, NOT ESP32Encoder
#include "driver/pulse_cnt.h"
pcnt_unit_handle_t c_pcnt_unit;
pcnt_channel_handle_t c_pcnt_chan;
```
The ESP32Encoder dependency exists but is unused - all encoder functionality is custom implemented.

### RPM Calculation & Display Switching  
C-axis displays **degrees (0-359.99°) OR RPM** based on spindle speed:
- Above 60 RPM → show RPM with hysteresis (45 RPM switch-back)
- Calculated from encoder delta over 100ms intervals using `C_COUNTS_PER_REV = 1600`

### Precision & Units
- Linear axes: **microns** internally, displayed as **mm** (3 decimal places: "123.456")
- Rotary axis: **ticks** internally (0-1599), displayed as **degrees** (2 decimal places: "123.45°")
- Use helper functions: `fmt_mm()`, `fmt_deg()`, `parse_mm_to_um()`, `parse_deg_to_deg_x100()`

## Build & Development Workflow

### PlatformIO Commands
```bash
# Build for JC4827W543 environment
pio run -e jc4827w543

# Upload and monitor
pio run -t upload -t monitor -e jc4827w543

# Clean build
pio run -t fullclean -e jc4827w543
```

### Key Configuration Files
- [`platformio.ini`](platformio.ini): ESP32-S3 config with QSPI display flags
- [`include/lv_conf.h`](include/lv_conf.h): LVGL configuration (16-bit color, 64KB memory)
- [`src/main.cpp`](src/main.cpp): App wiring (LVGL init, timers)
- [`src/config.h`](src/config.h): Board pins + system constants
- [`src/display.{h,cpp}`](src/display.h): Arduino_GFX + LVGL flush callback
- [`src/touch.{h,cpp}`](src/touch.h): TouchLib + LVGL indev read callback
- [`src/encoder.{h,cpp}`](src/encoder.h): ESP-IDF v5 PCNT (driver/pulse_cnt.h) + RPM logic
- [`src/coordinates.{h,cpp}`](src/coordinates.h): Offsets model + unit formatting/parsing
- [`src/modal.{h,cpp}`](src/modal.h): Modal numpad (X/T/G)
- [`src/tools.{h,cpp}`](src/tools.h): Tool selection state
- [`src/ui.{h,cpp}`](src/ui.h): LVGL UI construction + UI update loop

## UI Architecture & Modal System

### LVGL v9 Integration
- **Dual buffers**: 40-line partial rendering for memory efficiency
- **Custom flush callback**: `DisplayManager::flushCallback()` interfaces with Arduino_GFX
- **Touch integration**: `TouchManager::readCallback()` with 180° rotation support
- **Dark theme**: Uses LVGL default theme with blue/red palette

### Modal Zeroing System
Touch any axis label → modal with numpad appears:
- **X button**: Cancel
- **T button**: Set tool offset (most common workflow)
- **G button**: Set global reference
Modal handles parsing and applies coordinate transforms automatically.

### Tool Management
8-tool system with visual selection buttons. Tool switching affects all offset calculations immediately.

## EEZ Studio Integration
Project includes `.eez-project` files for potential GUI design, though current implementation is pure LVGL code. The EEZ files appear to be templates/placeholders.

## Common Development Patterns

### Adding New Axes
1. Add raw value variables (`x_raw_um` pattern)
2. Add global/tool offset arrays 
3. Update modal system enum and display logic
4. Add formatting/parsing functions for units

### Modifying Encoder Resolution
Change `C_COUNTS_PER_REV` and update `wrap_0_1599()` function range. RPM calculation scales automatically.

### LVGL Custom Widgets
Follow existing pattern in `make_axis_row()` - uses grid layout with custom styling for consistent DRO appearance.

## Hardware Pin Definitions
All pin assignments in [`src/config.h`](src/config.h):
- QSPI display: pins 45,47,21,48,40,39 
- Touch I2C: SDA=8, SCL=4, RST=38, INT=3
- Encoder: A=15, B=16
- Backlight PWM: pin 1

Critical for hardware-specific modifications or porting to different boards.