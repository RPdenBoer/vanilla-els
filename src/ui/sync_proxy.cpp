#include "sync_proxy.h"
#include "coordinates_ui.h"
#include "offsets_ui.h"
#include "tools_ui.h"

// Static member definitions
int32_t SyncProxy::sync_z_um = 0;
bool SyncProxy::enabled = false;
bool SyncProxy::waiting = false;
bool SyncProxy::in_sync = false;
bool SyncProxy::has_value = true;

static int32_t getZeroMachineUmForTool(int tool_index) {
    int off = OffsetManager::getCurrentOffset();
    if (off < 0 || off >= OFFSET_COUNT) off = 0;
    const int off_b = OffsetManager::isOffsetBActive(off) ? 1 : 0;
    const int tool_b = ToolManager::isToolBActive(tool_index) ? 1 : 0;
    return CoordinateSystem::z_global_um[off][off_b] + CoordinateSystem::z_tool_um[tool_index][tool_b];
}

void SyncProxy::init() {
    sync_z_um = 0;
    enabled = false;
    waiting = false;
    in_sync = false;
    has_value = true;
}

void SyncProxy::setFromCurrentZ() {
    const int tool = ToolManager::getCurrentTool();
    sync_z_um = getZeroMachineUmForTool(tool);
    has_value = true;
}

void SyncProxy::setFromDisplayUm(int32_t display_um, int tool_index) {
    (void)display_um;
    sync_z_um = getZeroMachineUmForTool(tool_index);
    has_value = true;
}

int32_t SyncProxy::getDisplayUm(int tool_index) {
    (void)tool_index;
    if (!has_value) return 0;
    return 0;
}

uint16_t SyncProxy::getPhaseTicks() {
    const int tool = ToolManager::getCurrentTool();
    int off = OffsetManager::getCurrentOffset();
    if (off < 0 || off >= OFFSET_COUNT) off = 0;
	const int off_b = OffsetManager::isOffsetBActive(off) ? 1 : 0;
	const int tool_b = ToolManager::isToolBActive(tool) ? 1 : 0;

    int32_t raw = CoordinateSystem::c_global_ticks[off][off_b] + CoordinateSystem::c_tool_ticks[tool][tool_b];
    return (uint16_t)CoordinateSystem::wrap01599(raw);
}

int32_t SyncProxy::getMachineUm() {
    const int tool = ToolManager::getCurrentTool();
    return getZeroMachineUmForTool(tool);
}

void SyncProxy::setEnabled(bool en) {
	if (en) has_value = true;
    enabled = en;
    if (!enabled) {
        waiting = false;
		in_sync = false;
    }
}

void SyncProxy::toggleEnabled() {
    setEnabled(!enabled);
}

void SyncProxy::setWaiting(bool is_waiting) {
    waiting = is_waiting && enabled;
	if (waiting) in_sync = false;
}

void SyncProxy::setInSync(bool is_in_sync) {
	in_sync = is_in_sync && enabled;
	if (in_sync) waiting = false;
}
