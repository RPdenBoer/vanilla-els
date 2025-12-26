#include "offsets_ui.h"
#include <cstdio>

int OffsetManager::g_offset_index = 0;
lv_obj_t *OffsetManager::offset_btns[OFFSET_COUNT] = {0};
lv_obj_t *OffsetManager::offset_labels[OFFSET_COUNT] = {0};
bool OffsetManager::offset_b[OFFSET_COUNT] = {0};

void OffsetManager::init() {
    g_offset_index = 0;
    for (int i = 0; i < OFFSET_COUNT; i++) {
        offset_btns[i] = nullptr;
        offset_labels[i] = nullptr;
        offset_b[i] = false;
    }
}

void OffsetManager::setCurrentOffset(int index) {
    if (index < 0 || index >= OFFSET_COUNT) return;
    g_offset_index = index;

    for (int i = 0; i < OFFSET_COUNT; i++) {
        if (!offset_btns[i]) continue;
        if (i == index) lv_obj_add_state(offset_btns[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(offset_btns[i], LV_STATE_CHECKED);
        updateLabel(i);
    }
}

void OffsetManager::onOffsetSelect(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
        return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    setCurrentOffset(idx);
}

void OffsetManager::onOffsetLongPress(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= OFFSET_COUNT) return;
    setCurrentOffset(idx);
    offset_b[idx] = !offset_b[idx];
    updateLabel(idx);
}

void OffsetManager::registerButton(int index, lv_obj_t *btn) {
    if (index < 0 || index >= OFFSET_COUNT) return;
    offset_btns[index] = btn;
}

void OffsetManager::registerLabel(int index, lv_obj_t *label) {
    if (index < 0 || index >= OFFSET_COUNT) return;
    offset_labels[index] = label;
    updateLabel(index);
}

bool OffsetManager::isOffsetBActive(int index) {
    if (index < 0 || index >= OFFSET_COUNT) return false;
    return offset_b[index];
}

void OffsetManager::updateLabel(int index) {
    if (index < 0 || index >= OFFSET_COUNT) return;
    if (!offset_labels[index]) return;
    char txt[12];
    if (offset_b[index]) snprintf(txt, sizeof(txt), "G%d B", index + 1);
    else snprintf(txt, sizeof(txt), "G%d", index + 1);
    lv_label_set_text(offset_labels[index], txt);

    if (offset_btns[index]) {
        const lv_color_t bg = lv_palette_darken(LV_PALETTE_BLUE, offset_b[index] ? 4 : 2);
        lv_obj_set_style_bg_color(offset_btns[index], bg, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(offset_btns[index], bg, LV_PART_MAIN | LV_STATE_CHECKED);
    }
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
