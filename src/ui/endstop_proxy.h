#pragma once

#include <stdint.h>
#include "shared/config_shared.h"

// ============================================================================
// EndstopProxy: Manages endstop state on UI, sends to motion board
// ============================================================================

class EndstopProxy {
public:
    static void init();

    // Endstop values in microns (machine coordinates)
    static int32_t getMinZUm() { return min_z_um; }
    static int32_t getMaxZUm() { return max_z_um; }

    // Set endstops from current Z position
    static void setMinFromCurrentZ();
    static void setMaxFromCurrentZ();

    // Set endstops from user-entered value (display coordinates with current tool/offset)
    static void setMinFromDisplayUm(int32_t display_um, int tool_index);
    static void setMaxFromDisplayUm(int32_t display_um, int tool_index);

    // Get display values (for showing in modal prefill)
    static int32_t getMinDisplayUm(int tool_index);
    static int32_t getMaxDisplayUm(int tool_index);

    // Check if endstops are configured
    static bool areEndstopsSet() { return endstops_configured; }

    // Clear/reset endstops
    static void clearEndstops();
    static void clearMin();
    static void clearMax();

    // Enable/disable endstops
    static bool isMinEnabled() { return min_enabled; }
    static bool isMaxEnabled() { return max_enabled; }
    static void setMinEnabled(bool en);
    static void setMaxEnabled(bool en);
    static void toggleMinEnabled();
    static void toggleMaxEnabled();

    // Check if a value has been set
    static bool hasMinValue() { return min_z_um != INT32_MIN; }
    static bool hasMaxValue() { return max_z_um != INT32_MAX; }
    
    // Get machine coordinate values for SPI transmission
    static int32_t getMinMachineUm() { return min_z_um; }
    static int32_t getMaxMachineUm() { return max_z_um; }
    
    // Check bounds (for local UI indication)
    static bool isZInBounds(int32_t z_raw_um);

private:
    static int32_t min_z_um;
    static int32_t max_z_um;
    static bool endstops_configured;
    static bool min_enabled;
    static bool max_enabled;
};
