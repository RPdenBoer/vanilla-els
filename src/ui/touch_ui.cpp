#include "touch_ui.h"
#include "config_ui.h"

TouchLib TouchManager::touch(Wire, TOUCH_SDA, TOUCH_SCL, TOUCH_ADDR);

bool TouchManager::init() {
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    bool ok = touch.begin(Wire, TOUCH_SDA, TOUCH_SCL, TOUCH_ADDR, TOUCH_RST);
    pinMode(TOUCH_INT, INPUT_PULLUP);
    return ok;
}

void TouchManager::readCallback(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;

    bool down = touch.read();
    if (!down) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    TP_Point p = touch.getPoint(0);

    int16_t x = (int16_t)constrain((int)p.x, 0, SCREEN_W - 1);
    int16_t y = (int16_t)constrain((int)p.y, 0, SCREEN_H - 1);

    if (TOUCH_ROTATE_180) {
        x = (SCREEN_W - 1) - x;
        y = (SCREEN_H - 1) - y;
    }

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
}
