#pragma once

#include <lvgl.h>
#include "shared/config_shared.h"

class ToolManager {
public:
    static void init();
    static int getCurrentTool() { return g_tool_index; }
    static int getCurrentToolVariant() { return tool_b[g_tool_index] ? 1 : 0; }
    static bool isToolBActive(int index);
    static void setCurrentTool(int index);
    static void registerButton(int index, lv_obj_t *btn);
    static void registerLabel(int index, lv_obj_t *label);
    static lv_obj_t* getToolButton(int index) { return tool_btns[index]; }
    static void onToolSelect(lv_event_t *e);
    static void onToolLongPress(lv_event_t *e);
    
private:
    static void loadState();
    static void saveState();
    static void updateLabel(int index);

    static int g_tool_index;
    static lv_obj_t *tool_btns[TOOL_COUNT];
    static lv_obj_t *tool_labels[TOOL_COUNT];
    static bool tool_b[TOOL_COUNT];
};
