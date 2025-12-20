#pragma once

#include <stdint.h>

// Virtual endstops for Z-axis.
// When ELS is enabled, it will only run if Z is within [min, max] bounds.
// If Z goes outside, ELS is immediately disabled (force-stop).
class EndstopManager {
public:
    static void init();

    // Endstop values in microns (machine coordinates, same as z_raw_um)
    static int32_t getMinZUm() { return min_z_um; }
    static int32_t getMaxZUm() { return max_z_um; }

    // Set endstops from current Z position.
    // These capture the *raw* machine coordinate directly for consistency.
    static void setMinFromCurrentZ();
    static void setMaxFromCurrentZ();

    // Set endstops from user-entered value (in display coordinates with current tool/offset).
    // Converts from user display um back to machine coordinates.
    static void setMinFromDisplayUm(int32_t display_um, int tool_index);
    static void setMaxFromDisplayUm(int32_t display_um, int tool_index);

    // Get display values (for showing in modal prefill).
    // Converts from machine to user display coordinates.
    static int32_t getMinDisplayUm(int tool_index);
    static int32_t getMaxDisplayUm(int tool_index);

    // Check if the given Z raw coordinate is within bounds.
    // Returns true if in-bounds (ELS can run), false if out-of-bounds.
    // Only checks bounds that are enabled.
    static bool isZInBounds(int32_t z_raw_um);

    // Check if endstops are configured (both min and max are different from defaults).
    static bool areEndstopsSet() { return endstops_configured; }

    // Clear/reset endstops to disabled state
    static void clearEndstops();
    static void clearMin();
    static void clearMax();

    // Enable/disable endstops (toggle on long press)
    static bool isMinEnabled() { return min_enabled; }
    static bool isMaxEnabled() { return max_enabled; }
    static void setMinEnabled(bool en);
    static void setMaxEnabled(bool en);
    static void toggleMinEnabled();
    static void toggleMaxEnabled();

    // Check if a value has been set (even if disabled)
    static bool hasMinValue() { return min_z_um != INT32_MIN; }
    static bool hasMaxValue() { return max_z_um != INT32_MAX; }

private:
    static int32_t min_z_um;    // Machine coordinates
    static int32_t max_z_um;    // Machine coordinates
    static bool endstops_configured;
    static bool min_enabled;    // Whether min endstop is active for bounds checking
    static bool max_enabled;    // Whether max endstop is active for bounds checking
};
