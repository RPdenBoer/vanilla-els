#pragma once

#include <Wire.h>
#include <TouchLib.h>
#include <lvgl.h>

class TouchManager {
public:
    static bool init();
    static void readCallback(lv_indev_t *indev, lv_indev_data_t *data);
    
private:
    static TouchLib touch;
};