#include "endstop_proxy.h"
#include "coordinates_ui.h"
#include "tools_ui.h"
#include "offsets_ui.h"

#include <Arduino.h>

// Static member definitions
int32_t EndstopProxy::min_z_um = 0;          // Default 0mm
int32_t EndstopProxy::max_z_um = 100000;     // Default 100mm
bool EndstopProxy::endstops_configured = false;
bool EndstopProxy::min_enabled = false;
bool EndstopProxy::max_enabled = false;

void EndstopProxy::init() {
    // Keep current values - they have sensible defaults
    // min_z_um and max_z_um already have defaults (0mm and 100mm)
    endstops_configured = false;
    min_enabled = false;
    max_enabled = false;
}

void EndstopProxy::setMinFromCurrentZ() {
    min_z_um = CoordinateSystem::z_raw_um;
    // Ensure min <= max; if user sets min beyond max, swap them
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

void EndstopProxy::setMaxFromCurrentZ() {
    max_z_um = CoordinateSystem::z_raw_um;
    // Ensure min <= max; if user sets max below min, swap them
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

void EndstopProxy::setMinFromDisplayUm(int32_t display_um, int tool_index) {
    // Convert from user display coordinates back to machine coordinates
    int off = OffsetManager::getCurrentOffset();
    if (off < 0 || off >= OFFSET_COUNT) off = 0;
    const int off_b = OffsetManager::isOffsetBActive(off) ? 1 : 0;
    const int tool_b = ToolManager::isToolBActive(tool_index) ? 1 : 0;

    // First convert from user display units to machine direction
    int32_t machine_um = CoordinateSystem::zUserToMachineUm(display_um);
    // Then add back offsets to get raw machine position
    min_z_um = machine_um + CoordinateSystem::z_global_um[off][off_b] + CoordinateSystem::z_tool_um[tool_index][tool_b];

    // Ensure min <= max
    if (max_z_um != INT32_MAX && min_z_um > max_z_um) {
        int32_t tmp = min_z_um;
        min_z_um = max_z_um;
        max_z_um = tmp;
        bool tmp_en = min_enabled;
        min_enabled = max_enabled;
        max_enabled = tmp_en;
    }
    min_enabled = true;
    endstops_configured = (min_enabled || max_enabled);
}

void EndstopProxy::setMaxFromDisplayUm(int32_t display_um, int tool_index) {
    // Convert from user display coordinates back to machine coordinates
    int off = OffsetManager::getCurrentOffset();
    if (off < 0 || off >= OFFSET_COUNT) off = 0;
    const int off_b = OffsetManager::isOffsetBActive(off) ? 1 : 0;
    const int tool_b = ToolManager::isToolBActive(tool_index) ? 1 : 0;

    int32_t machine_um = CoordinateSystem::zUserToMachineUm(display_um);
    max_z_um = machine_um + CoordinateSystem::z_global_um[off][off_b] + CoordinateSystem::z_tool_um[tool_index][tool_b];

    // Ensure min <= max
    if (min_z_um != INT32_MIN && min_z_um > max_z_um) {
        int32_t tmp = min_z_um;
        min_z_um = max_z_um;
        max_z_um = tmp;
        bool tmp_en = min_enabled;
        min_enabled = max_enabled;
        max_enabled = tmp_en;
    }
    max_enabled = true;
    endstops_configured = (min_enabled || max_enabled);
}

int32_t EndstopProxy::getMinDisplayUm(int tool_index) {
    int off = OffsetManager::getCurrentOffset();
    if (off < 0 || off >= OFFSET_COUNT) off = 0;
    const int off_b = OffsetManager::isOffsetBActive(off) ? 1 : 0;
    const int tool_b = ToolManager::isToolBActive(tool_index) ? 1 : 0;

    int32_t machine_um = min_z_um - CoordinateSystem::z_global_um[off][off_b] - CoordinateSystem::z_tool_um[tool_index][tool_b];
    return CoordinateSystem::zMachineToUserUm(machine_um);
}

int32_t EndstopProxy::getMaxDisplayUm(int tool_index) {
    int off = OffsetManager::getCurrentOffset();
    if (off < 0 || off >= OFFSET_COUNT) off = 0;
    const int off_b = OffsetManager::isOffsetBActive(off) ? 1 : 0;
    const int tool_b = ToolManager::isToolBActive(tool_index) ? 1 : 0;

    int32_t machine_um = max_z_um - CoordinateSystem::z_global_um[off][off_b] - CoordinateSystem::z_tool_um[tool_index][tool_b];
    return CoordinateSystem::zMachineToUserUm(machine_um);
}

bool EndstopProxy::isZInBounds(int32_t z_raw_um) {
    if (min_enabled && z_raw_um < min_z_um) return false;
    if (max_enabled && z_raw_um > max_z_um) return false;
    return true;
}

void EndstopProxy::clearEndstops() {
    min_z_um = INT32_MIN;
    max_z_um = INT32_MAX;
    min_enabled = false;
    max_enabled = false;
    endstops_configured = false;
}

void EndstopProxy::clearMin() {
    min_z_um = INT32_MIN;
    min_enabled = false;
    endstops_configured = max_enabled;
}

void EndstopProxy::clearMax() {
    max_z_um = INT32_MAX;
    max_enabled = false;
    endstops_configured = min_enabled;
}

void EndstopProxy::setMinEnabled(bool en) {
    if (en && min_z_um == INT32_MIN) return;
    min_enabled = en;
    endstops_configured = (min_enabled || max_enabled);
}

void EndstopProxy::setMaxEnabled(bool en) {
    max_enabled = en;
    endstops_configured = (min_enabled || max_enabled);
}

void EndstopProxy::toggleMinEnabled() {
    min_enabled = !min_enabled;
    endstops_configured = (min_enabled || max_enabled);
}

void EndstopProxy::toggleMaxEnabled() {
    max_enabled = !max_enabled;
    endstops_configured = (min_enabled || max_enabled);
}
