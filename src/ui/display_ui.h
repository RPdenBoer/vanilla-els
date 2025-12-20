#pragma once

#include <Arduino_GFX_Library.h>
#include <lvgl.h>

class DisplayManager {
public:
    static bool init();
    static void setBacklight(uint8_t duty_0_255);
    static Arduino_NV3041A* getPanel() { return panel; }
    
    // LVGL callbacks
    static void flushCallback(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

private:
    static Arduino_DataBus *bus;
    static Arduino_NV3041A *panel;
    static void backlightInit(uint8_t duty_0_255);
};
