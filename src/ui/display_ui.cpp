#include "display_ui.h"
#include "config_ui.h"

Arduino_DataBus *DisplayManager::bus = nullptr;
Arduino_NV3041A *DisplayManager::panel = nullptr;

bool DisplayManager::init() {
    // Initialize backlight first
    backlightInit(250);
    
    // Create bus and panel
    bus = new Arduino_ESP32QSPI(
        TFT_CS, TFT_SCK, TFT_D0, TFT_D1, TFT_D2, TFT_D3);
    
    panel = new Arduino_NV3041A(
        bus, GFX_NOT_DEFINED, LCD_ROTATION, true);
    
    return panel->begin();
}

void DisplayManager::setBacklight(uint8_t duty_0_255) {
    backlightInit(duty_0_255);
}

void DisplayManager::backlightInit(uint8_t duty_0_255) {
    ledcAttach(LCD_BL_PIN, 5000, 12);
    ledcWrite(LCD_BL_PIN, (uint32_t)duty_0_255 * 4095 / 255);
}

void DisplayManager::flushCallback(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    (void)disp;
    const uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    const uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);

    panel->startWrite();
    panel->setAddrWindow(area->x1, area->y1, w, h);
    panel->writePixels((uint16_t *)px_map, w * h);
    panel->endWrite();

    lv_display_flush_ready(disp);
}
