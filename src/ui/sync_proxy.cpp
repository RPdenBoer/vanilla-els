#include "sync_proxy.h"
#include "coordinates_ui.h"
#include "offsets_ui.h"
#include "tools_ui.h"

// Static member definitions
int32_t SyncProxy::sync_z_um = 0;
bool SyncProxy::enabled = false;
bool SyncProxy::waiting = false;
bool SyncProxy::in_sync = false;
bool SyncProxy::has_value = false;

void SyncProxy::init() {
    sync_z_um = 0;
    enabled = false;
    waiting = false;
    in_sync = false;
    has_value = false;
}

void SyncProxy::setFromCurrentZ() {
    sync_z_um = CoordinateSystem::z_raw_um;
    has_value = true;
}

void SyncProxy::setFromDisplayUm(int32_t display_um, int tool_index) {
    int off = OffsetManager::getCurrentOffset();
    if (off < 0 || off >= OFFSET_COUNT) off = 0;

    int32_t machine_um = CoordinateSystem::zUserToMachineUm(display_um);
    sync_z_um = machine_um + CoordinateSystem::z_global_um[off] + CoordinateSystem::z_tool_um[tool_index];
    has_value = true;
}

int32_t SyncProxy::getDisplayUm(int tool_index) {
    if (!has_value) return 0;
    int off = OffsetManager::getCurrentOffset();
    if (off < 0 || off >= OFFSET_COUNT) off = 0;

    int32_t machine_um = sync_z_um - CoordinateSystem::z_global_um[off] - CoordinateSystem::z_tool_um[tool_index];
    return CoordinateSystem::zMachineToUserUm(machine_um);
}

void SyncProxy::setEnabled(bool en) {
    if (en && !has_value) {
        setFromCurrentZ();
    }
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
