#include "endstops.h"
#include "coordinates.h"
#include "tools.h"
#include "offsets.h"

#include <Arduino.h>

// Static member definitions
int32_t EndstopManager::min_z_um = INT32_MIN;
int32_t EndstopManager::max_z_um = INT32_MAX;
bool EndstopManager::endstops_configured = false;
bool EndstopManager::min_enabled = false;
bool EndstopManager::max_enabled = false;

void EndstopManager::init() {
    min_z_um = INT32_MIN;
    max_z_um = INT32_MAX;
    endstops_configured = false;
    min_enabled = false;
    max_enabled = false;
}

void EndstopManager::setMinFromCurrentZ() {
    min_z_um = CoordinateSystem::z_raw_um;
    // Ensure min <= max; if user sets min beyond max, swap them.
    if (max_z_um != INT32_MAX && min_z_um > max_z_um) {
        int32_t tmp = min_z_um;
        min_z_um = max_z_um;
        max_z_um = tmp;
        // Swap enabled states too
        bool tmp_en = min_enabled;
        min_enabled = max_enabled;
        max_enabled = tmp_en;
    }
    min_enabled = true;  // Auto-enable when set
    endstops_configured = (min_enabled || max_enabled);
}

void EndstopManager::setMaxFromCurrentZ() {
    max_z_um = CoordinateSystem::z_raw_um;
    // Ensure min <= max; if user sets max below min, swap them.
    if (min_z_um != INT32_MIN && min_z_um > max_z_um) {
        int32_t tmp = min_z_um;
        min_z_um = max_z_um;
        max_z_um = tmp;
        // Swap enabled states too
        bool tmp_en = min_enabled;
        min_enabled = max_enabled;
        max_enabled = tmp_en;
    }
    max_enabled = true;  // Auto-enable when set
    endstops_configured = (min_enabled || max_enabled);
}

void EndstopManager::setMinFromDisplayUm(int32_t display_um, int tool_index) {
    // Convert from user display coordinates back to machine coordinates.
    // display = machine - global - tool  =>  machine = display + global + tool
    int off = OffsetManager::getCurrentOffset();
    if (off < 0 || off >= OFFSET_COUNT) off = 0;

    // First convert from user display units to machine direction
    int32_t machine_um = CoordinateSystem::zUserToMachineUm(display_um);
    // Then add back offsets to get raw machine position
    min_z_um = machine_um + CoordinateSystem::z_global_um[off] + CoordinateSystem::z_tool_um[tool_index];

    // Ensure min <= max
    if (max_z_um != INT32_MAX && min_z_um > max_z_um) {
        int32_t tmp = min_z_um;
        min_z_um = max_z_um;
        max_z_um = tmp;
        // Swap enabled states too
        bool tmp_en = min_enabled;
        min_enabled = max_enabled;
        max_enabled = tmp_en;
    }
    min_enabled = true;  // Auto-enable when set
    endstops_configured = (min_enabled || max_enabled);
}

void EndstopManager::setMaxFromDisplayUm(int32_t display_um, int tool_index) {
    // Convert from user display coordinates back to machine coordinates.
    int off = OffsetManager::getCurrentOffset();
    if (off < 0 || off >= OFFSET_COUNT) off = 0;

    // First convert from user display units to machine direction
    int32_t machine_um = CoordinateSystem::zUserToMachineUm(display_um);
    // Then add back offsets to get raw machine position
    max_z_um = machine_um + CoordinateSystem::z_global_um[off] + CoordinateSystem::z_tool_um[tool_index];

    // Ensure min <= max
    if (min_z_um != INT32_MIN && min_z_um > max_z_um) {
        int32_t tmp = min_z_um;
        min_z_um = max_z_um;
        max_z_um = tmp;
        // Swap enabled states too
        bool tmp_en = min_enabled;
        min_enabled = max_enabled;
        max_enabled = tmp_en;
    }
    max_enabled = true;  // Auto-enable when set
    endstops_configured = (min_enabled || max_enabled);
}

int32_t EndstopManager::getMinDisplayUm(int tool_index) {
    // Convert from machine to display coordinates
    int off = OffsetManager::getCurrentOffset();
    if (off < 0 || off >= OFFSET_COUNT) off = 0;

    int32_t machine_um = min_z_um - CoordinateSystem::z_global_um[off] - CoordinateSystem::z_tool_um[tool_index];
    return CoordinateSystem::zMachineToUserUm(machine_um);
}

int32_t EndstopManager::getMaxDisplayUm(int tool_index) {
    // Convert from machine to display coordinates
    int off = OffsetManager::getCurrentOffset();
    if (off < 0 || off >= OFFSET_COUNT) off = 0;

    int32_t machine_um = max_z_um - CoordinateSystem::z_global_um[off] - CoordinateSystem::z_tool_um[tool_index];
    return CoordinateSystem::zMachineToUserUm(machine_um);
}

bool EndstopManager::isZInBounds(int32_t z_raw_um) {
    // Only check bounds that are enabled
    if (min_enabled && z_raw_um < min_z_um) return false;
    if (max_enabled && z_raw_um > max_z_um) return false;
    return true;
}

void EndstopManager::clearEndstops() {
    min_z_um = INT32_MIN;
    max_z_um = INT32_MAX;
    min_enabled = false;
    max_enabled = false;
    endstops_configured = false;
}

void EndstopManager::clearMin() {
    min_z_um = INT32_MIN;
    min_enabled = false;
    endstops_configured = max_enabled;
}

void EndstopManager::clearMax() {
    max_z_um = INT32_MAX;
    max_enabled = false;
    endstops_configured = min_enabled;
}

void EndstopManager::setMinEnabled(bool en) {
    // Only allow enabling if a value has been set
    if (en && min_z_um == INT32_MIN) return;
    min_enabled = en;
    endstops_configured = (min_enabled || max_enabled);
}

void EndstopManager::setMaxEnabled(bool en) {
    // Only allow enabling if a value has been set
    if (en && max_z_um == INT32_MAX) return;
    max_enabled = en;
    endstops_configured = (min_enabled || max_enabled);
}

void EndstopManager::toggleMinEnabled() {
    if (min_z_um == INT32_MIN) return;  // No value set, can't toggle
    min_enabled = !min_enabled;
    endstops_configured = (min_enabled || max_enabled);
}

void EndstopManager::toggleMaxEnabled() {
    if (max_z_um == INT32_MAX) return;  // No value set, can't toggle
    max_enabled = !max_enabled;
    endstops_configured = (min_enabled || max_enabled);
}
