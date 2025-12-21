// ============================================================================
// UI Controller Main (ESP32-S3 JC4827W543)
// Handles: Display, touch, LVGL UI, SPI communication to Motion board
// ============================================================================

#include <Arduino.h>
#include <lvgl.h>

#include "config_ui.h"
#include "shared/protocol.h"
#include "spi_master.h"
#include "display_ui.h"
#include "touch_ui.h"
#include "coordinates_ui.h"
#include "tools_ui.h"
#include "offsets_ui.h"
#include "encoder_proxy.h"
#include "leadscrew_proxy.h"
#include "endstop_proxy.h"
#include "ui_ui.h"

// ============================================================================
// SPI polling and proxy updates
// ============================================================================
static void updateFromMotionBoard() {
    SpiMaster::poll();
    const StatusPacket& status = SpiMaster::getStatus();
    
    // Update raw position values in coordinate system
    CoordinateSystem::x_raw_um = status.x_count * X_UM_PER_COUNT;
    CoordinateSystem::z_raw_um = status.z_count * Z_UM_PER_COUNT;

	// Update encoder proxy with spindle data and MPG state
	EncoderProxy::updateFromMotion(status.c_count, status.rpm_signed,
								   status.target_rpm, status.flags.mpg_mode);

	// Check for endstop hit flag from motion board
    if (status.flags.endstop_hit) {
        LeadscrewProxy::setBoundsExceeded(true);
    }
}

static void sendCommandsToMotionBoard() {
    // Update SPI master state from proxies
    SpiMaster::setElsEnabled(LeadscrewProxy::isEnabled());
    SpiMaster::setPitchUm(LeadscrewProxy::getPitchUm());
    SpiMaster::setDirectionMul(LeadscrewProxy::getDirectionMul());
    SpiMaster::setEndstops(
        EndstopProxy::getMinMachineUm(),
        EndstopProxy::getMaxMachineUm(),
        EndstopProxy::isMinEnabled(),
        EndstopProxy::isMaxEnabled()
    );
}

// ============================================================================
// UI timer callback - updates display
// ============================================================================
static void ui_timer_cb(lv_timer_t *t) {
    (void)t;

	// Poll physical buttons
	UIManager::pollPhysicalButtons();

	// Get latest data from motion board
    updateFromMotionBoard();
    
    // Send commands to motion board
    sendCommandsToMotionBoard();
    
    // Update UI with new values
    UIManager::update();
}

// ============================================================================
// LVGL timing
// ============================================================================
static uint32_t last_lv_ms = 0;

// ============================================================================
// Setup
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.printf("\n[UI] Boot: %s %s\n", __DATE__, __TIME__);
    Serial.flush();

    // Initialize display
    if (!DisplayManager::init()) {
        Serial.println("[UI] Display init FAILED");
        while (true) delay(1000);
    }
    Serial.println("[UI] Display OK");

    // Initialize touch
    bool t_ok = TouchManager::init();
    Serial.printf("[UI] Touch init: %s\n", t_ok ? "OK" : "FAIL");

    // Initialize SPI master (communication with motion board)
    if (!SpiMaster::init()) {
        Serial.println("[UI] SPI master init FAILED");
        // Continue anyway - UI will show disconnected state
    } else {
        Serial.println("[UI] SPI master OK");
    }

    // Initialize LVGL
    lv_init();
    Serial.println("[UI] LVGL init");

    // Partial buffers (40 lines)
    static lv_color_t buf1[SCREEN_W * 40];
    static lv_color_t buf2[SCREEN_W * 40];

    lv_display_t *disp = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_flush_cb(disp, DisplayManager::flushCallback);
    lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, TouchManager::readCallback);

    // Dark theme
    lv_theme_t *th = lv_theme_default_init(
        lv_display_get_default(),
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_RED),
        true,
        LV_FONT_DEFAULT);
    lv_display_set_theme(lv_display_get_default(), th);

    // Initialize UI
    UIManager::init();
	UIManager::initPhysicalButtons();
	Serial.println("[UI] UI init");

    // Create timer for periodic updates (50ms = 20Hz)
    lv_timer_create(ui_timer_cb, 50, nullptr);
    last_lv_ms = millis();
    
    Serial.println("[UI] Boot complete");
}

// ============================================================================
// Main loop
// ============================================================================
void loop() {
    uint32_t now = millis();
    uint32_t diff = now - last_lv_ms;
    last_lv_ms = now;

    lv_tick_inc(diff);
    lv_timer_handler();

    // Heartbeat
    static uint32_t last_hb = 0;
    if (now - last_hb > 5000) {
        last_hb = now;
        Serial.printf("[UI] hb - motion %s\n", 
            SpiMaster::isConnected() ? "connected" : "DISCONNECTED");
    }
}
