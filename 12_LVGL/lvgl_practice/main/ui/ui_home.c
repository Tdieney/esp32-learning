#include <stdint.h>
#include "ui_home.h"
#include "ui_theme.h"
#include "ui_room_control.h"
#include "ui.h"

static void room_card_click_cb(lv_event_t* e)
{
    int index = (int) (intptr_t) lv_event_get_user_data(e);
    ui_room_select_room(index);
    ui_show_room();
}

static void room_toggle_cb(lv_event_t* e)
{
    // LVGL only dispatches a click/value-change to the topmost widget under the touch point
    // (the switch itself) -- since we never enable LV_OBJ_FLAG_EVENT_BUBBLE on it, tapping the
    // switch never also fires the card's own CLICKED handler above.
    lv_obj_t* sw = lv_event_get_target(e);
    int index = (int) (intptr_t) lv_event_get_user_data(e);
    ui_room_set_on(index, lv_obj_has_state(sw, LV_STATE_CHECKED));
}

static void create_room_card(lv_obj_t* parent, int index)
{
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_add_style(card, &ui_style_card, 0);
    lv_obj_set_size(card, 186, 58);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(card, room_card_click_cb, LV_EVENT_CLICKED, (void*) (intptr_t) index);

    lv_obj_t* icon_chip = lv_obj_create(card);
    lv_obj_add_style(icon_chip, &ui_style_chip, 0);
    lv_obj_set_size(icon_chip, 30, 30);
    lv_obj_clear_flag(icon_chip, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* icon_label = lv_label_create(icon_chip);
    lv_label_set_text(icon_label, LV_SYMBOL_HOME);
    lv_obj_center(icon_label);

    lv_obj_t* text_col = lv_obj_create(card);
    lv_obj_remove_style_all(text_col);
    lv_obj_set_size(text_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(text_col, 8, 0);
    lv_obj_set_flex_flow(text_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(text_col, 1); // fills the gap between the icon and the switch
    lv_obj_clear_flag(text_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* name_label = lv_label_create(text_col);
    lv_label_set_text(name_label, ui_room_get_name(index));

    lv_obj_t* count_label = lv_label_create(text_col);
    lv_obj_set_style_text_color(count_label, UI_COLOR_TEXT_MUTED, 0);
    lv_label_set_text_fmt(count_label, "%d Devices", ui_room_get_device_count(index));

    lv_obj_t* sw = lv_switch_create(card);
    lv_obj_set_size(sw, 34, 20);
    if (ui_room_get_on(index))
    {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, room_toggle_cb, LV_EVENT_VALUE_CHANGED, (void*) (intptr_t) index);
}

lv_obj_t* ui_home_create(lv_obj_t* parent)
{
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // --- Greeting ---
    lv_obj_t* greeting = lv_label_create(panel);
    lv_obj_set_style_text_font(greeting, UI_FONT_HEADER, 0);
    lv_label_set_text(greeting, "Hi, Thinh");
    lv_obj_align(greeting, LV_ALIGN_TOP_LEFT, 16, 12);

    lv_obj_t* subtitle = lv_label_create(panel);
    lv_obj_set_style_text_color(subtitle, UI_COLOR_TEXT_MUTED, 0);
    lv_label_set_text(subtitle, "Your home at a glance");
    lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 16, 42);

    // --- Stat tiles: all mock -- this board has no temperature/power sensors wired up ---
    ui_theme_create_stat_tile(panel,
                              120,
                              70,
                              LV_ALIGN_TOP_LEFT,
                              16,
                              68,
                              LV_SYMBOL_TINT,
                              "24"
                              "\xC2\xB0"
                              "C",
                              "HCM");
    ui_theme_create_stat_tile(panel, 120, 70, LV_ALIGN_TOP_LEFT, 148, 68, LV_SYMBOL_OK, "13", "Active");
    ui_theme_create_stat_tile(panel, 120, 70, LV_ALIGN_TOP_LEFT, 280, 68, LV_SYMBOL_CHARGE, "312", "kWh Usage");

    // --- Rooms grid ---
    lv_obj_t* rooms_label = lv_label_create(panel);
    lv_label_set_text(rooms_label, "Rooms");
    lv_obj_align(rooms_label, LV_ALIGN_TOP_LEFT, 16, 146);

    lv_obj_t* grid = lv_obj_create(panel);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, 384, 132);
    lv_obj_align(grid, LV_ALIGN_TOP_LEFT, 16, 176);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(grid, 12, 0);
    lv_obj_set_style_pad_row(grid, 8, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    int room_count = ui_room_get_count();
    for (int i = 0; i < room_count; i++)
    {
        create_room_card(grid, i);
    }

    return panel;
}
