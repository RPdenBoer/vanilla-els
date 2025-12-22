#pragma once

#include <stdint.h>

// ============================================================================
// SyncProxy: Manages Z sync target and state on UI
// ============================================================================

class SyncProxy {
public:
    static void init();

    static void setFromCurrentZ();
    static void setFromDisplayUm(int32_t display_um, int tool_index);
    static int32_t getDisplayUm(int tool_index);

    static int32_t getMachineUm() { return sync_z_um; }
    static bool hasValue() { return has_value; }

    static bool isEnabled() { return enabled; }
    static void setEnabled(bool en);
    static void toggleEnabled();

    static void setWaiting(bool waiting);
    static bool isWaiting() { return waiting; }
    static void setInSync(bool in_sync);
    static bool isInSync() { return in_sync; }

private:
    static int32_t sync_z_um;
    static bool enabled;
    static bool waiting;
    static bool in_sync;
    static bool has_value;
};
