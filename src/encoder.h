#pragma once

#include <Arduino.h>
#include "driver/pulse_cnt.h"

// X/Z use GPIO interrupts (no PCNT). C uses ESP-IDF PCNT.

class EncoderManager {
public:
    static bool init();
    static void update();
    static int32_t getRawTicks() { return c_raw_ticks; }
    // Full-resolution spindle count (extended beyond PCNT limits)
    static int32_t getSpindleTotalCount();
    static int32_t getRpm() { return rpm_raw; }
    static bool shouldShowRpm() { return c_show_rpm; }

    static int32_t getXCount();
    static int32_t getZCount();
    
private:
    static pcnt_unit_handle_t c_pcnt_unit;
    static pcnt_channel_handle_t c_pcnt_chan_a;
    static pcnt_channel_handle_t c_pcnt_chan_b;
    static volatile int32_t c_pcnt_accum;
    
    static int32_t c_raw_ticks;
    static int32_t rpm_raw;
    static bool c_show_rpm;

    struct QuadAxis {
        uint8_t pin_a;
        uint8_t pin_b;
        volatile int32_t count;
        volatile uint8_t state;
        int8_t dir;
    };

    static QuadAxis x_axis;
    static QuadAxis z_axis;

    static void initLinearAxis(QuadAxis &axis);
    static void updateLinearAxes();
    static void IRAM_ATTR quadIsr(void *arg);
    
    static int32_t getTotalCount();
    static bool IRAM_ATTR onReachCallback(pcnt_unit_handle_t unit,
                                         const pcnt_watch_event_data_t *edata,
                                         void *user_ctx);
};