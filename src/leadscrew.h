#pragma once

#include <stdint.h>
#include <stddef.h>

class LeadscrewManager {
public:
    static void init();

    static bool isEnabled() { return enabled; }
    static void setEnabled(bool on);
    static void toggleEnabled() { setEnabled(!enabled); }

    // Pitch input/display mode is independent of DRO mm/in.
    // When true, pitch is shown/entered as TPI. When false, as mm per rev.
    static bool isPitchTpiMode() { return pitch_tpi_mode; }
    static void setPitchTpiMode(bool on) { pitch_tpi_mode = on; }
    static void togglePitchTpiMode() { pitch_tpi_mode = !pitch_tpi_mode; }

    // Internal representation: pitch in microns per spindle revolution
    static int32_t getPitchUm() { return pitch_um; }
    static void setPitchUm(int32_t pitch_um_per_rev);

    // Direction multiplier (spindle-synced): +1 normal, -1 reversed.
    // Used for momentary jog-right (equivalent of a negative pitch).
    static int8_t getDirectionMul() { return direction_mul; }
    static void setDirectionMul(int8_t mul) { direction_mul = (mul < 0) ? -1 : 1; }

    // UI helpers
    static void formatPitchLabel(char *out, size_t n);

private:
    static void stepTask(void *arg);
    static void updateOnce();
    static void outputSteps(int32_t step_delta);
    static void ensureHardware();

    static bool enabled;
    static bool pitch_tpi_mode;
    static int32_t pitch_um; // microns per spindle revolution

    static volatile int8_t direction_mul;

    static int32_t last_spindle_total;
    static int64_t step_accum_q16;

    static void *task_handle;

    // RMT-backed step generation
    static bool rmt_ready;
};
