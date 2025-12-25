#include "tools_ui.h"
#include "shared/config_shared.h"
#include <Preferences.h>
#include <cstdio>

int ToolManager::g_tool_index = 0;
lv_obj_t *ToolManager::tool_btns[TOOL_COUNT] = {0};
lv_obj_t *ToolManager::tool_labels[TOOL_COUNT] = {0};
bool ToolManager::tool_b[TOOL_COUNT] = {0};

static constexpr uint32_t TOOL_STATE_MAGIC = 0x544C5354; // "TLST"
static const char *TOOL_STATE_NS = "ui_state";
static const char *TOOL_STATE_KEY = "tool_state_v1";

struct ToolStateBlob {
    uint32_t magic;
    uint8_t tool_index;
    uint8_t tool_b[TOOL_COUNT];
};

void ToolManager::init() {
    g_tool_index = 0;
    for (int i = 0; i < TOOL_COUNT; i++) {
        tool_btns[i] = nullptr;
        tool_labels[i] = nullptr;
        tool_b[i] = false;
    }
    loadState();
    if (g_tool_index < 0 || g_tool_index >= TOOL_COUNT)
        g_tool_index = 0;
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
            updateLabel(i);
        }
        saveState();
    }
}

void ToolManager::onToolSelect(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    setCurrentTool(idx);
}

void ToolManager::onToolLongPress(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= TOOL_COUNT) return;
    setCurrentTool(idx);
    tool_b[idx] = !tool_b[idx];
    updateLabel(idx);
    saveState();
}

void ToolManager::registerButton(int index, lv_obj_t *btn) {
    if (index < 0 || index >= TOOL_COUNT) return;
    tool_btns[index] = btn;
}

void ToolManager::registerLabel(int index, lv_obj_t *label) {
    if (index < 0 || index >= TOOL_COUNT) return;
    tool_labels[index] = label;
    updateLabel(index);
}

bool ToolManager::isToolBActive(int index) {
    if (index < 0 || index >= TOOL_COUNT) return false;
    return tool_b[index];
}

void ToolManager::updateLabel(int index) {
    if (index < 0 || index >= TOOL_COUNT) return;
    if (!tool_labels[index]) return;
    char txt[8];
    if (tool_b[index]) snprintf(txt, sizeof(txt), "T%dB", index + 1);
    else snprintf(txt, sizeof(txt), "T%d", index + 1);
    lv_label_set_text(tool_labels[index], txt);
}

void ToolManager::loadState() {
    Preferences prefs;
    if (!prefs.begin(TOOL_STATE_NS, true)) return;
    ToolStateBlob blob = {};
    size_t len = prefs.getBytes(TOOL_STATE_KEY, &blob, sizeof(blob));
    prefs.end();
    if (len != sizeof(blob) || blob.magic != TOOL_STATE_MAGIC) return;
    if (blob.tool_index < TOOL_COUNT) g_tool_index = blob.tool_index;
    for (int i = 0; i < TOOL_COUNT; i++) {
        tool_b[i] = (blob.tool_b[i] != 0);
    }
}

void ToolManager::saveState() {
    Preferences prefs;
    if (!prefs.begin(TOOL_STATE_NS, false)) return;
    ToolStateBlob blob = {};
    blob.magic = TOOL_STATE_MAGIC;
    blob.tool_index = (uint8_t)g_tool_index;
    for (int i = 0; i < TOOL_COUNT; i++) {
        blob.tool_b[i] = tool_b[i] ? 1 : 0;
    }
    prefs.putBytes(TOOL_STATE_KEY, &blob, sizeof(blob));
    prefs.end();
}
