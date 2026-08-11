#include "ui_devices.h"
#include "ui_theme.h"

// Mock device list: purely local to this page (unlike the room summaries, nothing else in
// the app needs to know about these, so there's no shared "device model" the way Home and
// Room Control share the room model).
typedef struct
{
    const char* name;
    const char* status;
    bool on;
} device_t;

typedef struct
{
    const char* group_name;
    device_t devices[2];
} device_group_t;

static device_group_t s_groups[] = {
    {"Office", {{"Router", "3 Connected", true}, {"Smart Lamp", "6 Connected", false}}},
    {"Living Room", {{"Smart Lamp", "Warm", true}, {"CCTV", "Standby", false}}},
};
#define GROUP_COUNT ((int) (sizeof(s_groups) / sizeof(s_groups[0])))

static void device_toggle_cb(lv_event_t* e)
{
    lv_obj_t* sw = lv_event_get_target(e);
    device_t* dev = (device_t*) lv_event_get_user_data(e);
    dev->on = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void create_device_card(lv_obj_t* parent, device_t* dev)
{
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_add_style(card, &ui_style_card, 0);
    lv_obj_set_size(card, 186, 58);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* icon_chip = lv_obj_create(card);
    lv_obj_add_style(icon_chip, &ui_style_chip, 0);
    lv_obj_set_size(icon_chip, 30, 30);
    lv_obj_clear_flag(icon_chip, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* icon_label = lv_label_create(icon_chip);
    lv_label_set_text(icon_label, LV_SYMBOL_CHARGE); // generic "device" placeholder icon
    lv_obj_center(icon_label);

    lv_obj_t* text_col = lv_obj_create(card);
    lv_obj_remove_style_all(text_col);
    lv_obj_set_size(text_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(text_col, 8, 0);
    lv_obj_set_flex_flow(text_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(text_col, 1); // fills the gap between the icon and the switch
    lv_obj_clear_flag(text_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* name_label = lv_label_create(text_col);
    lv_label_set_text(name_label, dev->name);

    lv_obj_t* status_label = lv_label_create(text_col);
    lv_obj_set_style_text_color(status_label, UI_COLOR_TEXT_MUTED, 0);
    lv_label_set_text(status_label, dev->status);

    lv_obj_t* sw = lv_switch_create(card);
    lv_obj_set_size(sw, 34, 20);
    if (dev->on)
    {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, device_toggle_cb, LV_EVENT_VALUE_CHANGED, dev);
}

lv_obj_t* ui_devices_create(lv_obj_t* parent)
{
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_left(panel, 16, 0);
    lv_obj_set_style_pad_right(panel, 16, 0);
    lv_obj_set_style_pad_top(panel, 16, 0);
    lv_obj_set_style_pad_bottom(panel, 16, 0);
    lv_obj_set_style_pad_row(panel, 12, 0);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER); // the only page that scrolls -- more devices than fit at once

    for (int g = 0; g < GROUP_COUNT; g++)
    {
        lv_obj_t* header = lv_label_create(panel);
        lv_label_set_text(header, s_groups[g].group_name);

        lv_obj_t* grid = lv_obj_create(panel);
        lv_obj_remove_style_all(grid);
        lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_pad_column(grid, 12, 0);
        lv_obj_set_style_pad_row(grid, 8, 0);
        lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

        for (int d = 0; d < 2; d++)
        {
            create_device_card(grid, &s_groups[g].devices[d]);
        }
    }

    return panel;
}
