#include "ui.h"
#include "ui_theme.h"
#include "ui_home.h"
#include "ui_devices.h"
#include "ui_room_control.h"

#define SIDEBAR_WIDTH 64
#define PAGE_COUNT    3

static lv_obj_t* s_content;
static lv_obj_t* s_panels[PAGE_COUNT];
static lv_obj_t* s_chips[PAGE_COUNT];

static void panel_opa_anim_cb(void* var, int32_t v)
{
    lv_obj_set_style_opa(var, (lv_opa_t) v, 0);
}

// One-shot fade-in only (not continuous) -- cheap, and only runs on the rare event of a page
// switch, never every frame. See UI_PLAN.md's Performance & Optimization Guidelines, rule 6.
static void fade_in(lv_obj_t* obj)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, panel_opa_anim_cb);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 150);
    lv_anim_start(&a);
}

// Hides every panel except `index` and re-styles the sidebar's active indicator -- nothing is
// destroyed or recreated, matching UI_PLAN.md's revised architecture (one root screen, content
// panels swapped by visibility instead of separate lv_scr_t screens + lv_scr_load_anim).
static void show_page(int index)
{
    for (int i = 0; i < PAGE_COUNT; i++)
    {
        if (i == index)
        {
            lv_obj_clear_flag(s_panels[i], LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(s_panels[i], LV_OBJ_FLAG_HIDDEN);
        }
        ui_theme_set_chip_active(s_chips[i], i == index, UI_COLOR_ACCENT);
    }
    fade_in(s_panels[index]);
}

void ui_show_home(void)
{
    show_page(0);
}

void ui_show_devices(void)
{
    show_page(1);
}

void ui_show_room(void)
{
    show_page(2);
}

static void nav_home_cb(lv_event_t* e)
{
    LV_UNUSED(e);
    ui_show_home();
}

static void nav_devices_cb(lv_event_t* e)
{
    LV_UNUSED(e);
    ui_show_devices();
}

static void nav_room_cb(lv_event_t* e)
{
    LV_UNUSED(e);
    ui_show_room();
}

void ui_init(void)
{
    ui_theme_init();

    lv_coord_t hor_res = lv_disp_get_hor_res(NULL);
    lv_coord_t ver_res = lv_disp_get_ver_res(NULL);

    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &ui_style_screen, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // --- Persistent sidebar: never rebuilt, never re-laid-out after this ---
    lv_obj_t* sidebar = lv_obj_create(scr);
    lv_obj_remove_style_all(sidebar);
    lv_obj_set_size(sidebar, SIDEBAR_WIDTH, ver_res);
    lv_obj_align(sidebar, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_flex_flow(sidebar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sidebar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(sidebar, 22, 0);
    lv_obj_clear_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);

    static const char* icons[PAGE_COUNT] = {LV_SYMBOL_HOME, LV_SYMBOL_LIST, LV_SYMBOL_SETTINGS};
    static const lv_event_cb_t callbacks[PAGE_COUNT] = {nav_home_cb, nav_devices_cb, nav_room_cb};
    for (int i = 0; i < PAGE_COUNT; i++)
    {
        lv_obj_t* chip = lv_btn_create(sidebar);
        lv_obj_add_style(chip, &ui_style_chip, 0);
        lv_obj_set_size(chip, 44, 44);
        lv_obj_add_event_cb(chip, callbacks[i], LV_EVENT_CLICKED, NULL);

        lv_obj_t* label = lv_label_create(chip);
        lv_label_set_text(label, icons[i]);
        lv_obj_center(label);

        s_chips[i] = chip;
    }

    // --- Content container: the only thing that changes on navigation ---
    s_content = lv_obj_create(scr);
    lv_obj_remove_style_all(s_content);
    lv_obj_set_size(s_content, hor_res - SIDEBAR_WIDTH, ver_res);
    lv_obj_align(s_content, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

    // All 3 pages are built once and kept alive for the life of the app -- navigation just
    // shows/hides which one is visible.
    s_panels[0] = ui_home_create(s_content);
    s_panels[1] = ui_devices_create(s_content);
    s_panels[2] = ui_room_create(s_content);

    show_page(0);

    lv_scr_load(scr);
}
