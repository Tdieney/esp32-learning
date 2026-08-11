#include <stdint.h>
#include "ui_room_control.h"
#include "ui_theme.h"

// Room Control owns the single mock "room model". Home's room grid reads/writes it through
// the getters/setters in ui_room_control.h instead of keeping its own copy, so a toggle
// flipped from Home is never out of sync with this screen.
typedef struct
{
    const char* name;
    int device_count;
    bool on;
    int temperature; // Celsius
    int mode;        // 0 = cooling, 1 = heating, 2 = fan
} room_state_t;

static room_state_t s_rooms[] = {
    {"Bedroom", 5, false, 22, 0},
    {"Living Room", 8, true, 24, 0},
    {"Kitchen", 3, false, 20, 2},
};
#define ROOM_COUNT ((int) (sizeof(s_rooms) / sizeof(s_rooms[0])))

static int s_current_room = 1; // Living Room, matches the reference image's default screen

static lv_obj_t* s_tab_btns[ROOM_COUNT];
static lv_obj_t* s_device_switch;
static lv_obj_t* s_arc;
static lv_obj_t* s_mode_name_label;
static lv_obj_t* s_temp_value_label;
static lv_obj_t* s_mode_chips[3];

static const char* const s_mode_names[3] = {"Cooling", "Heating", "Fan"};

// mode_color() is a function (not a static const array) on purpose: UI_COLOR_ACCENT/HEAT/FAN
// expand to lv_color_hex(...) calls, which aren't constant expressions C allows in a static
// initializer -- computing it at call time sidesteps that entirely.
static lv_color_t mode_color(int mode)
{
    switch (mode)
    {
        case 1:
            return UI_COLOR_ACCENT_HEAT;
        case 2:
            return UI_COLOR_ACCENT_FAN;
        default:
            return UI_COLOR_ACCENT;
    }
}

static void style_tab(lv_obj_t* btn, bool active)
{
    if (active)
    {
        lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0x05070C), 0);
    }
    else
    {
        lv_obj_set_style_bg_color(btn, UI_COLOR_CARD, 0);
        lv_obj_set_style_text_color(btn, UI_COLOR_TEXT_MUTED, 0);
    }
}

static void update_dial_display(void)
{
    room_state_t* room = &s_rooms[s_current_room];
    lv_label_set_text(s_mode_name_label, s_mode_names[room->mode]);
    lv_label_set_text_fmt(s_temp_value_label, "%d" "\xC2\xB0", room->temperature);
    lv_arc_set_value(s_arc, room->temperature);
    lv_obj_set_style_arc_color(s_arc, mode_color(room->mode), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_arc, mode_color(room->mode), LV_PART_KNOB);
}

static void select_room(int index)
{
    s_current_room = index;
    room_state_t* room = &s_rooms[index];

    for (int i = 0; i < ROOM_COUNT; i++)
    {
        style_tab(s_tab_btns[i], i == index);
    }
    if (room->on)
    {
        lv_obj_add_state(s_device_switch, LV_STATE_CHECKED);
    }
    else
    {
        lv_obj_clear_state(s_device_switch, LV_STATE_CHECKED);
    }
    for (int m = 0; m < 3; m++)
    {
        ui_theme_set_chip_active(s_mode_chips[m], m == room->mode, mode_color(m));
    }
    update_dial_display();
}

static void tab_click_cb(lv_event_t* e)
{
    int index = (int) (intptr_t) lv_event_get_user_data(e);
    select_room(index);
}

static void device_switch_cb(lv_event_t* e)
{
    lv_obj_t* sw = lv_event_get_target(e);
    s_rooms[s_current_room].on = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void arc_value_changed_cb(lv_event_t* e)
{
    lv_obj_t* arc = lv_event_get_target(e);
    s_rooms[s_current_room].temperature = lv_arc_get_value(arc);
    lv_label_set_text_fmt(s_temp_value_label, "%d" "\xC2\xB0", s_rooms[s_current_room].temperature);
}

static void mode_click_cb(lv_event_t* e)
{
    int mode = (int) (intptr_t) lv_event_get_user_data(e);
    s_rooms[s_current_room].mode = mode;
    for (int m = 0; m < 3; m++)
    {
        ui_theme_set_chip_active(s_mode_chips[m], m == mode, mode_color(m));
    }
    update_dial_display();
}

static void create_mini_stat(lv_obj_t* parent, lv_coord_t y, const char* value, const char* caption)
{
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_add_style(box, &ui_style_card, 0);
    lv_obj_set_style_pad_left(box, 6, 0);
    lv_obj_set_style_pad_right(box, 6, 0);
    lv_obj_set_style_pad_top(box, 6, 0);
    lv_obj_set_style_pad_bottom(box, 6, 0);
    lv_obj_set_size(box, 150, 52);
    lv_obj_align(box, LV_ALIGN_TOP_LEFT, 220, y);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* value_label = lv_label_create(box);
    lv_label_set_text(value_label, value);
    lv_obj_align(value_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* caption_label = lv_label_create(box);
    lv_obj_set_style_text_color(caption_label, UI_COLOR_TEXT_MUTED, 0);
    lv_label_set_text(caption_label, caption);
    lv_obj_align(caption_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

int ui_room_get_count(void)
{
    return ROOM_COUNT;
}

const char* ui_room_get_name(int index)
{
    return s_rooms[index].name;
}

int ui_room_get_device_count(int index)
{
    return s_rooms[index].device_count;
}

bool ui_room_get_on(int index)
{
    return s_rooms[index].on;
}

void ui_room_set_on(int index, bool on)
{
    s_rooms[index].on = on;
    if (index == s_current_room)
    {
        if (on)
        {
            lv_obj_add_state(s_device_switch, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(s_device_switch, LV_STATE_CHECKED);
        }
    }
}

void ui_room_select_room(int index)
{
    select_room(index);
}

lv_obj_t* ui_room_create(lv_obj_t* parent)
{
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // --- Room tabs ---
    lv_obj_t* tabs_row = lv_obj_create(panel);
    lv_obj_remove_style_all(tabs_row);
    lv_obj_set_size(tabs_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(tabs_row, LV_ALIGN_TOP_LEFT, 16, 10);
    lv_obj_set_flex_flow(tabs_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(tabs_row, 8, 0);
    lv_obj_clear_flag(tabs_row, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < ROOM_COUNT; i++)
    {
        lv_obj_t* tab = lv_btn_create(tabs_row);
        lv_obj_set_size(tab, LV_SIZE_CONTENT, 32);
        lv_obj_set_style_pad_left(tab, 14, 0);
        lv_obj_set_style_pad_right(tab, 14, 0);
        lv_obj_add_event_cb(tab, tab_click_cb, LV_EVENT_CLICKED, (void*) (intptr_t) i);

        lv_obj_t* label = lv_label_create(tab);
        lv_label_set_text(label, s_rooms[i].name);
        lv_obj_center(label);

        s_tab_btns[i] = tab;
    }

    // --- Device toggle ---
    lv_obj_t* device_row = lv_obj_create(panel);
    lv_obj_remove_style_all(device_row);
    lv_obj_set_size(device_row, 384, 28);
    lv_obj_align(device_row, LV_ALIGN_TOP_LEFT, 16, 50);
    lv_obj_set_flex_flow(device_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(device_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(device_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* device_label = lv_label_create(device_row);
    lv_label_set_text(device_label, "Air Conditioner");

    s_device_switch = lv_switch_create(device_row);
    lv_obj_set_size(s_device_switch, 34, 20);
    lv_obj_add_event_cb(s_device_switch, device_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // --- Dial (left half): a real draggable lv_arc, not a display-only ring ---
    s_arc = lv_arc_create(panel);
    lv_obj_set_size(s_arc, 180, 180);
    lv_arc_set_bg_angles(s_arc, 135, 45); // gap at the bottom -- classic thermostat gauge shape
    lv_arc_set_range(s_arc, 16, 30);
    lv_obj_set_style_arc_width(s_arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, UI_COLOR_CARD, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 12, LV_PART_INDICATOR);
    lv_obj_align(s_arc, LV_ALIGN_TOP_LEFT, 20, 90);
    lv_obj_add_event_cb(s_arc, arc_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_mode_name_label = lv_label_create(s_arc);
    lv_obj_set_style_text_color(s_mode_name_label, UI_COLOR_TEXT_MUTED, 0);
    lv_obj_align(s_mode_name_label, LV_ALIGN_CENTER, 0, -20);

    s_temp_value_label = lv_label_create(s_arc);
    lv_obj_set_style_text_font(s_temp_value_label, UI_FONT_HEADER, 0);
    lv_obj_align(s_temp_value_label, LV_ALIGN_CENTER, 0, 8);

    // --- Mode chips (right half, top) ---
    lv_obj_t* mode_col = lv_obj_create(panel);
    lv_obj_remove_style_all(mode_col);
    lv_obj_set_size(mode_col, 150, LV_SIZE_CONTENT);
    lv_obj_align(mode_col, LV_ALIGN_TOP_LEFT, 220, 90);
    lv_obj_set_flex_flow(mode_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(mode_col, 6, 0);
    lv_obj_clear_flag(mode_col, LV_OBJ_FLAG_SCROLLABLE);

    for (int m = 0; m < 3; m++)
    {
        lv_obj_t* chip = lv_btn_create(mode_col);
        lv_obj_add_style(chip, &ui_style_card, 0);
        lv_obj_set_size(chip, 150, 28);
        lv_obj_add_event_cb(chip, mode_click_cb, LV_EVENT_CLICKED, (void*) (intptr_t) m);

        lv_obj_t* label = lv_label_create(chip);
        lv_label_set_text(label, s_mode_names[m]);
        lv_obj_center(label);

        s_mode_chips[m] = chip;
    }

    // --- Stat tiles (right half, bottom) ---
    create_mini_stat(panel, 196, "08 Hrs", "Timer");
    create_mini_stat(panel, 256, "35%", "Humidity");

    select_room(s_current_room); // paints the initial tab/switch/dial/mode state

    return panel;
}
