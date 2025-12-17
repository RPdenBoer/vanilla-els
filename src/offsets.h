#pragma once

#include <lvgl.h>
#include "config.h"

class OffsetManager {
public:
    static void init();
    static int getCurrentOffset() { return g_offset_index; }
    static void setCurrentOffset(int index);
    static void registerButton(int index, lv_obj_t *btn);
    static void onOffsetSelect(lv_event_t *e);

private:
    static int g_offset_index;
    static lv_obj_t *offset_btns[OFFSET_COUNT];
};
