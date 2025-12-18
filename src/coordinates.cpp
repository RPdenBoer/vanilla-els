#include "coordinates.h"

#include "offsets.h"

#include <Arduino.h>
#include <cstring>

// Static member definitions
int32_t CoordinateSystem::x_raw_um = 0;
int32_t CoordinateSystem::z_raw_um = 0;

int32_t CoordinateSystem::x_global_um[OFFSET_COUNT] = {0};
int32_t CoordinateSystem::z_global_um[OFFSET_COUNT] = {0};
int32_t CoordinateSystem::c_global_ticks[OFFSET_COUNT] = {0};

int32_t CoordinateSystem::x_tool_um[TOOL_COUNT] = {0};
int32_t CoordinateSystem::z_tool_um[TOOL_COUNT] = {0};
int32_t CoordinateSystem::c_tool_ticks[TOOL_COUNT] = {0};

bool CoordinateSystem::x_radius_mode = true;
bool CoordinateSystem::z_inverted = false;
bool CoordinateSystem::linear_inch_mode = false;

static int32_t div2_round_away_from_zero(int32_t v) {
    if (v >= 0) return (v + 1) / 2;
    return -(((-v) + 1) / 2);
}

int32_t CoordinateSystem::xMachineToUserUm(int32_t machine_um) {
    // Physical linear scale is effectively radius-based.
    // Therefore:
    // - rad mode displays radius (1x)
    // - dia mode displays diameter (2x)
    return x_radius_mode ? machine_um : (machine_um * 2);
}

int32_t CoordinateSystem::xUserToMachineUm(int32_t user_um) {
    // Convert user-entered/displayed value back to machine (radius-based) units.
    return x_radius_mode ? user_um : div2_round_away_from_zero(user_um);
}

int32_t CoordinateSystem::zMachineToUserUm(int32_t machine_um) {
    return z_inverted ? -machine_um : machine_um;
}

int32_t CoordinateSystem::zUserToMachineUm(int32_t user_um) {
    return z_inverted ? -user_um : user_um;
}

int32_t CoordinateSystem::getDisplayX(int tool_index) {
    int off = OffsetManager::getCurrentOffset();
    if (off < 0 || off >= OFFSET_COUNT) off = 0;
    int32_t machine_um = x_raw_um - x_global_um[off] - x_tool_um[tool_index];
    return xMachineToUserUm(machine_um);
}

int32_t CoordinateSystem::getDisplayZ(int tool_index) {
    int off = OffsetManager::getCurrentOffset();
    if (off < 0 || off >= OFFSET_COUNT) off = 0;
    int32_t machine_um = z_raw_um - z_global_um[off] - z_tool_um[tool_index];
    return zMachineToUserUm(machine_um);
}

int32_t CoordinateSystem::getDisplayC(int32_t c_raw_ticks, int tool_index) {
    int off = OffsetManager::getCurrentOffset();
    if (off < 0 || off >= OFFSET_COUNT) off = 0;
    int32_t raw = wrap01599(c_raw_ticks);
    return wrap01599(raw - c_global_ticks[off] - c_tool_ticks[tool_index]);
}

void CoordinateSystem::formatMm(char *out, size_t n, int32_t um) {
    int32_t sign = (um < 0) ? -1 : 1;
    int32_t a = (um < 0) ? -um : um;
    int32_t mm_i = (a / 1000);
	int32_t mm_f = ((a % 1000) + 5) / 10; // round to 2 decimals
	if (mm_f >= 100)
	{
		mm_i++;
		mm_f = 0;
	} // handle rounding overflow
	if (sign < 0)
		snprintf(out, n, "-%ld.%02ld", (long)mm_i, (long)mm_f);
	else
		snprintf(out, n, "%ld.%02ld", (long)mm_i, (long)mm_f);
}

void CoordinateSystem::formatDeg(char *out, size_t n, int32_t deg_x100) {
    int32_t d_i = deg_x100 / 100;
    int32_t d_f = deg_x100 % 100;
    snprintf(out, n, "%ld.%02ld", (long)d_i, (long)d_f);
}

bool CoordinateSystem::parseMmToUm(const char *s, int32_t *out_um) {
    if (!s || !out_um) return false;
    float v = atof(s);
    *out_um = (int32_t)lroundf(v * 1000.0f);
    return true;
}

bool CoordinateSystem::parseDegToDegX100(const char *s, int32_t *out_deg_x100) {
    if (!s || !out_deg_x100) return false;
    float v = atof(s);
    *out_deg_x100 = (int32_t)lroundf(v * 100.0f);
    return true;
}

int32_t CoordinateSystem::wrap01599(int32_t t) {
    int32_t r = t % 1600;
    if (r < 0) r += 1600;
    return r;
}

int32_t CoordinateSystem::ticksToDegX100(int32_t ticks_0_1599) {
    return (ticks_0_1599 * 36000) / 1600; // 0..35977
}

int32_t CoordinateSystem::degX100ToTicks(int32_t deg_x100) {
    int32_t t = (int32_t)lroundf((float)deg_x100 * 1600.0f / 36000.0f);
    return wrap01599(t);
}

void CoordinateSystem::formatLinear(char *out, size_t n, int32_t um) {
    if (!out || n == 0) return;

    if (!linear_inch_mode) {
        formatMm(out, n, um);
        return;
    }

    // Inches: 1 inch = 25.4mm = 25400um
    float inches = (float)um / 25400.0f;
	// 3 decimals is a reasonable DRO-style resolution for inches
	snprintf(out, n, "%.3f", inches);
}

bool CoordinateSystem::parseLinearToUm(const char *s, int32_t *out_um) {
    if (!s || !out_um) return false;
    float v = atof(s);
    if (!linear_inch_mode) {
        *out_um = (int32_t)lroundf(v * 1000.0f);
        return true;
    }

    *out_um = (int32_t)lroundf(v * 25400.0f);
    return true;
}