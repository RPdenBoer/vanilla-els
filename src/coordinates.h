#pragma once

#include <stdint.h>
#include <cstddef>

#include "config.h"

class CoordinateSystem {
public:
    // Raw values (microns for linear, ticks for rotary)
    static int32_t x_raw_um;
    static int32_t z_raw_um;
    
    // Work offsets (G54/G55 style), indexed by OffsetManager's current offset
    static int32_t x_global_um[OFFSET_COUNT];
    static int32_t z_global_um[OFFSET_COUNT];
    static int32_t c_global_ticks[OFFSET_COUNT];
    
    // Tool offsets (indexed by tool number)
    static int32_t x_tool_um[TOOL_COUNT];
    static int32_t z_tool_um[TOOL_COUNT];
    static int32_t c_tool_ticks[TOOL_COUNT];

    // Display modes
    // X: diameter (default) or radius display
    static bool x_radius_mode;
    // Z: normal (pos) or inverted (neg) display
    static bool z_inverted;

    // Linear units: mm (default) or inches
    static bool linear_inch_mode;

    static bool isXRadiusMode() { return x_radius_mode; }
    static bool isZInverted() { return z_inverted; }
    static bool isLinearInchMode() { return linear_inch_mode; }
    static void toggleXRadiusMode() { x_radius_mode = !x_radius_mode; }
    static void toggleZInverted() { z_inverted = !z_inverted; }
    static void toggleLinearInchMode() { linear_inch_mode = !linear_inch_mode; }

    // Map between internal machine units and user display units
    static int32_t xMachineToUserUm(int32_t machine_um);
    static int32_t xUserToMachineUm(int32_t user_um);
    static int32_t zMachineToUserUm(int32_t machine_um);
    static int32_t zUserToMachineUm(int32_t user_um);
    
    // Coordinate calculations (display = raw - global - tool[current])
    static int32_t getDisplayX(int tool_index);
    static int32_t getDisplayZ(int tool_index);
    static int32_t getDisplayC(int32_t c_raw_ticks, int tool_index);
    
    // Unit conversion helpers
    static void formatMm(char *out, size_t n, int32_t um);
    static void formatDeg(char *out, size_t n, int32_t deg_x100);
    static bool parseMmToUm(const char *s, int32_t *out_um);
    static bool parseDegToDegX100(const char *s, int32_t *out_deg_x100);

    // Linear helpers that respect mm/in mode
    static void formatLinear(char *out, size_t n, int32_t um);
    static bool parseLinearToUm(const char *s, int32_t *out_um);
    
    // Rotation helpers
    static int32_t wrap01599(int32_t t);
    static int32_t ticksToDegX100(int32_t ticks_0_1599);
    static int32_t degX100ToTicks(int32_t deg_x100);
};