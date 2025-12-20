#pragma once

#include <lvgl.h>
#include "shared/config_shared.h"

class ToolManager {
public:
    static void init();
    static int getCurrentTool() { return g_tool_index; }
    static void setCurrentTool(int index);
    static void registerButton(int index, lv_obj_t *btn);
    static lv_obj_t* getToolButton(int index) { return tool_btns[index]; }
    static void onToolSelect(lv_event_t *e);
    
private:
    static int g_tool_index;
    static lv_obj_t *tool_btns[TOOL_COUNT];
};
