#pragma once

#include <Arduino.h>
#include "driver/pulse_cnt.h"
#include "config.h"

// X/Z use GPIO interrupts (no PCNT). C uses ESP-IDF PCNT.

class EncoderManager {
public:
    static bool init();
    static void update();
    static int32_t getRawTicks() { return c_raw_ticks; }
    // Full-resolution spindle count (extended beyond PCNT limits)
    static int32_t getSpindleTotalCount();
    static int32_t getRpm() { return rpm_raw; }
	static int32_t getRpmSigned() { return rpm_signed; } // Includes direction
	static bool shouldShowRpm() { return c_show_rpm; }

	// Manual RPM/deg toggle (only effective when below auto-threshold)
	static bool isManualRpmMode() { return c_manual_rpm_mode; }
	static void toggleManualRpmMode();
	static bool canToggleManualMode() { return rpm_raw <= RPM_SHOW_RPM_OFF; }

	static int32_t getXCount();
    static int32_t getZCount();
    
private:
    static pcnt_unit_handle_t c_pcnt_unit;
    static pcnt_channel_handle_t c_pcnt_chan_a;
    static pcnt_channel_handle_t c_pcnt_chan_b;
    static volatile int32_t c_pcnt_accum;
    
    static int32_t c_raw_ticks;
    static int32_t rpm_raw;
	static int32_t rpm_signed; // RPM with direction sign
	static bool c_show_rpm;
	static bool c_manual_rpm_mode; // User preference when below auto-threshold

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