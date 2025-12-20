#include "offsets_ui.h"

int OffsetManager::g_offset_index = 0;
lv_obj_t *OffsetManager::offset_btns[OFFSET_COUNT] = {0};

void OffsetManager::init() {
    g_offset_index = 0;
    for (int i = 0; i < OFFSET_COUNT; i++) {
        offset_btns[i] = nullptr;
    }
}

void OffsetManager::setCurrentOffset(int index) {
    if (index < 0 || index >= OFFSET_COUNT) return;
    g_offset_index = index;

    for (int i = 0; i < OFFSET_COUNT; i++) {
        if (!offset_btns[i]) continue;
        if (i == index) lv_obj_add_state(offset_btns[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(offset_btns[i], LV_STATE_CHECKED);
    }
}

void OffsetManager::onOffsetSelect(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
        return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    setCurrentOffset(idx);
}

void OffsetManager::registerButton(int index, lv_obj_t *btn) {
    if (index < 0 || index >= OFFSET_COUNT) return;
    offset_btns[index] = btn;
}

int OffsetManager::getMainOffsetButtonWidth() {
    // Prefer G1 if available; fall back to any registered button.
    lv_obj_t *btn = offset_btns[0];
    if (!btn) {
        for (int i = 1; i < OFFSET_COUNT; i++) {
            if (offset_btns[i]) {
                btn = offset_btns[i];
                break;
            }
        }
    }

    if (!btn)
        return 40;
    lv_obj_update_layout(btn);
    const int w = (int)lv_obj_get_width(btn);
    return (w > 0) ? w : 40;
}
