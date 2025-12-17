#include "leadscrew.h"

#include <Arduino.h>
#include <lvgl.h>
#include <cmath>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "coordinates.h"
#include "encoder.h"

#include "driver/gpio.h"

#if ELS_USE_RMT
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_types.h"
#endif

bool LeadscrewManager::enabled = false;
bool LeadscrewManager::pitch_tpi_mode = false;
int32_t LeadscrewManager::pitch_um = 1000; // default: 1.000mm pitch
volatile int8_t LeadscrewManager::direction_mul = 1;
int32_t LeadscrewManager::last_spindle_total = 0;
int64_t LeadscrewManager::step_accum_q16 = 0;
void *LeadscrewManager::task_handle = nullptr;
bool LeadscrewManager::rmt_ready = false;

#if ELS_USE_RMT
static rmt_channel_handle_t s_rmt_chan = nullptr;
static rmt_encoder_handle_t s_rmt_encoder = nullptr;
static rmt_symbol_word_t s_rmt_symbols[ELS_RMT_CHUNK_STEPS];
#endif

void LeadscrewManager::init() {
    // Default pitch depends on unit mode: in -> 20 TPI, mm -> 1.0mm
    if (CoordinateSystem::isLinearInchMode()) {
        // 20 TPI => 1/20 inch => 25.4mm/20 = 1.27mm = 1270um
        pitch_um = (int32_t)lroundf(25400.0f / 20.0f);
        pitch_tpi_mode = true;
    } else {
        pitch_tpi_mode = false;
    }

    // Intentionally do NOT configure GPIO or start the step task here.
    // We defer all real-time step generation until the user toggles ELS ON.
    enabled = false;
    task_handle = nullptr;
    rmt_ready = false;

    direction_mul = 1;
}

void LeadscrewManager::ensureHardware() {
    // Configure STEP/DIR/(optional EN) pins individually.
    // Avoids any pin-bitmask shifting and keeps behavior explicit.
    if (ELS_STEP_PIN >= 0) {
        (void)gpio_reset_pin((gpio_num_t)ELS_STEP_PIN);
        (void)gpio_set_direction((gpio_num_t)ELS_STEP_PIN, GPIO_MODE_OUTPUT);
        (void)gpio_set_level((gpio_num_t)ELS_STEP_PIN, 0);
    }

    if (ELS_DIR_PIN >= 0) {
        (void)gpio_reset_pin((gpio_num_t)ELS_DIR_PIN);
        (void)gpio_set_direction((gpio_num_t)ELS_DIR_PIN, GPIO_MODE_OUTPUT);
        (void)gpio_set_level((gpio_num_t)ELS_DIR_PIN, ELS_INVERT_DIR ? 1 : 0);
    }

    if (ELS_EN_PIN >= 0) {
        (void)gpio_reset_pin((gpio_num_t)ELS_EN_PIN);
        (void)gpio_set_direction((gpio_num_t)ELS_EN_PIN, GPIO_MODE_OUTPUT);
        if (ELS_EN_ACTIVE_LOW) (void)gpio_set_level((gpio_num_t)ELS_EN_PIN, 1);
        else                   (void)gpio_set_level((gpio_num_t)ELS_EN_PIN, 0);
    }

#if ELS_USE_RMT
    // Configure RMT TX on STEP pin for precise pulse timing.
    if (!rmt_ready && ELS_STEP_PIN >= 0) {
        rmt_tx_channel_config_t tx_cfg = {};
        tx_cfg.gpio_num = (gpio_num_t)ELS_STEP_PIN;
        tx_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
        tx_cfg.resolution_hz = ELS_RMT_RES_HZ;
        tx_cfg.mem_block_symbols = 64; // enough for short chunks
        tx_cfg.trans_queue_depth = 2;

        rmt_copy_encoder_config_t enc_cfg = {};
        if (rmt_new_tx_channel(&tx_cfg, &s_rmt_chan) == ESP_OK &&
            rmt_new_copy_encoder(&enc_cfg, &s_rmt_encoder) == ESP_OK &&
            rmt_enable(s_rmt_chan) == ESP_OK) {
            rmt_ready = true;
        }
    }
#endif

    // Start the step task once.
    if (task_handle == nullptr) {
        xTaskCreatePinnedToCore(
            stepTask,
            "els_step",
            4096,
            nullptr,
            2,
            (TaskHandle_t *)&task_handle,
            0);
    }
}

void LeadscrewManager::setPitchUm(int32_t pitch_um_per_rev) {
    if (pitch_um_per_rev == 0) pitch_um_per_rev = 1;
    // Keep magnitude at least 1um/rev
    if (pitch_um_per_rev > 0 && pitch_um_per_rev < 1) pitch_um_per_rev = 1;
    if (pitch_um_per_rev < 0 && pitch_um_per_rev > -1) pitch_um_per_rev = -1;
    pitch_um = pitch_um_per_rev;
}

void LeadscrewManager::setEnabled(bool on) {
    // Lazy init on first enable to avoid any boot-time side effects.
    if (on) ensureHardware();

    enabled = on;

    // Default direction when enabling.
    if (enabled) direction_mul = 1;

    // Reset synchronization so we don't do a large burst on enable.
    last_spindle_total = EncoderManager::getSpindleTotalCount();
    step_accum_q16 = 0;

    // Enable/disable driver (optional)
    if (ELS_EN_PIN >= 0) {
        if (enabled) {
            if (ELS_EN_ACTIVE_LOW) gpio_set_level((gpio_num_t)ELS_EN_PIN, 0);
            else                  gpio_set_level((gpio_num_t)ELS_EN_PIN, 1);
        } else {
            if (ELS_EN_ACTIVE_LOW) gpio_set_level((gpio_num_t)ELS_EN_PIN, 1);
            else                  gpio_set_level((gpio_num_t)ELS_EN_PIN, 0);
        }
    }
}

void LeadscrewManager::formatPitchLabel(char *out, size_t n) {
    if (!out || n == 0) return;

    if (pitch_tpi_mode) {
        // Display as TPI
        const float abs_pitch = (float)abs(pitch_um);
        float tpi = 25400.0f / abs_pitch;
        // Prefer integer if close
        float nearest = roundf(tpi);
        if (fabsf(tpi - nearest) < 0.01f) {
            if (pitch_um < 0) snprintf(out, n, "-%.0f", nearest);
            else              snprintf(out, n, "%.0f", nearest);
        } else {
            if (pitch_um < 0) snprintf(out, n, "-%.2f", tpi);
            else              snprintf(out, n, "%.2f", tpi);
        }
        return;
    }

    // Display as pitch length (unitless): mm/rev in MM mode, inches/rev in INCH mode.
    if (CoordinateSystem::isLinearInchMode()) {
        const float inches = (float)pitch_um / 25400.0f;
        snprintf(out, n, "%.4f", inches);
    } else {
        const float mm = (float)pitch_um / 1000.0f;
        snprintf(out, n, "%.3f", mm);
    }
}

void LeadscrewManager::stepTask(void *arg) {
    (void)arg;
    for (;;) {
        if (enabled) {
            updateOnce();
            // Fast loop; 1ms cadence keeps phase tight
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void LeadscrewManager::updateOnce() {
    const int32_t total = EncoderManager::getSpindleTotalCount();
    int32_t delta_counts = total - last_spindle_total;
    last_spindle_total = total;

    // Apply direction multiplier (used for jog-right / reversed feed)
    if (direction_mul < 0) delta_counts = -delta_counts;

    if (delta_counts == 0) return;

    // Steps per rev = (ELS_STEPS_PER_REV * thread_pitch_um) / leadscrew_pitch_um
    // Steps per count = steps_per_rev / C_COUNTS_PER_REV
    // Use Q16.16 fixed point for good fractional behavior.
    const int64_t steps_per_rev_q16 =
        (int64_t)ELS_STEPS_PER_REV * (int64_t)pitch_um * (int64_t)(1 << 16) / (int64_t)ELS_LEADSCREW_PITCH_UM;

    const int64_t steps_per_count_q16 = steps_per_rev_q16 / (int64_t)C_COUNTS_PER_REV;

    step_accum_q16 += (int64_t)delta_counts * steps_per_count_q16;

    int32_t step_delta = 0;
    if (step_accum_q16 >= 0) {
        step_delta = (int32_t)(step_accum_q16 >> 16);
    } else {
        step_delta = -(int32_t)(((-step_accum_q16) >> 16));
    }

    if (step_delta == 0) return;

    // Avoid pathological bursts if we fell behind.
    if (step_delta > ELS_MAX_STEPS_PER_CYCLE) step_delta = ELS_MAX_STEPS_PER_CYCLE;
    if (step_delta < -ELS_MAX_STEPS_PER_CYCLE) step_delta = -ELS_MAX_STEPS_PER_CYCLE;

    outputSteps(step_delta);
    // Subtract only what we actually tried to output (clamped)
    step_accum_q16 -= ((int64_t)step_delta << 16);
}

void LeadscrewManager::outputSteps(int32_t step_delta) {
    if (step_delta == 0) return;

    const bool reverse = step_delta < 0;
    uint32_t steps = (uint32_t)(reverse ? -step_delta : step_delta);

    bool dir_level = reverse ? 1 : 0;
    if (ELS_INVERT_DIR) dir_level = !dir_level;

    gpio_set_level((gpio_num_t)ELS_DIR_PIN, dir_level ? 1 : 0);

#if ELS_USE_RMT
    if (rmt_ready && s_rmt_chan && s_rmt_encoder) {
        // Transmit in short chunks to keep latency low and allow rapid DIR changes.
        uint32_t remaining = steps;
        const uint16_t dur = (uint16_t)ELS_PULSE_US; // 1us ticks at 1MHz

        while (remaining > 0) {
            uint32_t chunk = remaining;
            if (chunk > (uint32_t)ELS_RMT_CHUNK_STEPS) chunk = (uint32_t)ELS_RMT_CHUNK_STEPS;

            for (uint32_t i = 0; i < chunk; i++) {
                rmt_symbol_word_t sym;
                sym.level0 = 1;
                sym.duration0 = dur;
                sym.level1 = 0;
                sym.duration1 = dur;
                s_rmt_symbols[i] = sym;
            }

            rmt_transmit_config_t tx_cfg = {};
            tx_cfg.loop_count = 0;

            // Submit then wait for this chunk so DIR changes are safe.
            (void)rmt_transmit(s_rmt_chan, s_rmt_encoder, s_rmt_symbols, chunk * sizeof(rmt_symbol_word_t), &tx_cfg);
            (void)rmt_tx_wait_all_done(s_rmt_chan, pdMS_TO_TICKS(20));

            remaining -= chunk;
        }
        return;
    }
#endif

    // Fallback: bit-bang pulses (lower performance, higher CPU load)
    if (steps > (uint32_t)ELS_MAX_STEPS_PER_CYCLE) steps = (uint32_t)ELS_MAX_STEPS_PER_CYCLE;
    for (uint32_t i = 0; i < steps; i++) {
        gpio_set_level((gpio_num_t)ELS_STEP_PIN, 1);
        delayMicroseconds(ELS_PULSE_US);
        gpio_set_level((gpio_num_t)ELS_STEP_PIN, 0);
        delayMicroseconds(ELS_PULSE_US);
    }
}
