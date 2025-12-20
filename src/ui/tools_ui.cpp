#include "tools_ui.h"
#include "shared/config_shared.h"

int ToolManager::g_tool_index = 0;
lv_obj_t *ToolManager::tool_btns[TOOL_COUNT] = {0};

void ToolManager::init() {
    g_tool_index = 0;
    for (int i = 0; i < TOOL_COUNT; i++) {
        tool_btns[i] = nullptr;
    }
}

void ToolManager::setCurrentTool(int index) {
    if (index >= 0 && index < TOOL_COUNT) {
        g_tool_index = index;
        
        // Update button states
        for (int i = 0; i < TOOL_COUNT; i++) {
            if (tool_btns[i]) {
                if (i == index) {
                    lv_obj_add_state(tool_btns[i], LV_STATE_CHECKED);
                } else {
                    lv_obj_clear_state(tool_btns[i], LV_STATE_CHECKED);
                }
            }
        }
    }
}

void ToolManager::onToolSelect(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    setCurrentTool(idx);
}

void ToolManager::registerButton(int index, lv_obj_t *btn) {
    if (index < 0 || index >= TOOL_COUNT) return;
    tool_btns[index] = btn;
}
