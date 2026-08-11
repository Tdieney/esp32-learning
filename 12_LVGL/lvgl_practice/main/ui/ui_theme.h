#pragma once

#include <stdbool.h>
#include "lvgl.h"

// Palette adapted from Image.png: dark glass cards over a navy-to-black gradient, with a
// single solid accent color swapped per context instead of the reference's multi-stop
// rainbow gradients (LVGL v8 only does simple 2-color linear gradients -- see UI_PLAN.md's
// Performance & Optimization Guidelines for why this matters on an MCU).
#define UI_COLOR_ACCENT        lv_color_hex(0x22D3EE) // cyan -- default / cooling mode
#define UI_COLOR_ACCENT_HEAT   lv_color_hex(0xFB923C) // orange -- heating mode
#define UI_COLOR_ACCENT_FAN    lv_color_hex(0x2DD4BF) // teal -- fan mode
#define UI_COLOR_BG_TOP        lv_color_hex(0x101A2E)
#define UI_COLOR_BG_BOTTOM     lv_color_hex(0x05070C)
#define UI_COLOR_BG_DIM_TOP    lv_color_hex(0x080C16)
#define UI_COLOR_BG_DIM_BOTTOM lv_color_hex(0x000000)
#define UI_COLOR_CARD          lv_color_hex(0x131B2E)
#define UI_COLOR_TEXT_MUTED    lv_color_hex(0x7C8AA5)

// Fonts enabled via menuconfig/sdkconfig (Component config -> LVGL configuration -> Font usage).
#define UI_FONT_BODY   (&lv_font_montserrat_14)
#define UI_FONT_HEADER (&lv_font_montserrat_24)

// Applies the shared theme and builds the reusable styles below. Call once, before creating
// any screen.
void ui_theme_init(void);

// Shared styles -- pages add() these instead of one-off style calls, so the whole app stays
// visually consistent and can be restyled everywhere at once.
extern lv_style_t ui_style_screen; // gradient background, applied to the single root screen
extern lv_style_t ui_style_card;   // glass card: translucent bg, accent border, soft glow shadow
extern lv_style_t ui_style_chip;   // small circular icon chip (sidebar nav, stat tile icons)

// A small "stat tile": icon chip + big value + caption, positioned/sized by the caller in one
// call. Returns the value label so the caller can update it later.
lv_obj_t* ui_theme_create_stat_tile(lv_obj_t* parent, lv_coord_t w, lv_coord_t h, lv_align_t align, lv_coord_t x_ofs,
                                    lv_coord_t y_ofs, const char* icon, const char* value, const char* caption);

// Toggles a chip between its neutral (muted icon/border, no glow) and active (accent-colored,
// glowing) look -- used for the sidebar's current-page indicator and Room Control's mode
// chips. `accent` lets each caller pick which color counts as "active" (sidebar always uses
// the default cyan; Room Control's mode chips each use their own mode color).
void ui_theme_set_chip_active(lv_obj_t* chip, bool active, lv_color_t accent);
