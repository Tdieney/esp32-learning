#include "ui_theme.h"

lv_style_t ui_style_screen;
lv_style_t ui_style_card;
lv_style_t ui_style_chip;

void ui_theme_init(void)
{
    // Every widget created after this call inherits this theme unless a page's own style
    // overrides it -- the "shared style" half of shared-vs-local styling.
    lv_theme_t* theme =
        lv_theme_default_init(lv_disp_get_default(), UI_COLOR_ACCENT, UI_COLOR_ACCENT_HEAT, true /* dark */, UI_FONT_BODY);
    lv_disp_set_theme(lv_disp_get_default(), theme);

    // Deep navy-to-black vertical gradient instead of a flat color -- gives the background
    // some depth without needing any image assets or a runtime blur filter.
    lv_style_init(&ui_style_screen);
    lv_style_set_bg_color(&ui_style_screen, UI_COLOR_BG_TOP);
    lv_style_set_bg_grad_color(&ui_style_screen, UI_COLOR_BG_BOTTOM);
    lv_style_set_bg_grad_dir(&ui_style_screen, LV_GRAD_DIR_VER);

    // "Glass panel" card: translucent, thin accent border, soft colored glow instead of a
    // realistic (and more expensive) blurred drop shadow.
    lv_style_init(&ui_style_card);
    lv_style_set_bg_color(&ui_style_card, UI_COLOR_CARD);
    lv_style_set_bg_opa(&ui_style_card, LV_OPA_90);
    lv_style_set_radius(&ui_style_card, 16);
    lv_style_set_border_color(&ui_style_card, UI_COLOR_ACCENT);
    lv_style_set_border_opa(&ui_style_card, LV_OPA_30);
    lv_style_set_border_width(&ui_style_card, 1);
    lv_style_set_shadow_color(&ui_style_card, UI_COLOR_ACCENT);
    lv_style_set_shadow_width(&ui_style_card, 16);
    lv_style_set_shadow_opa(&ui_style_card, LV_OPA_20);
    lv_style_set_pad_all(&ui_style_card, 10);

    // Circular icon chip -- neutral/muted by default; ui_theme_set_chip_active() brightens it.
    // Used for the sidebar, and reused (via ui_theme_create_stat_tile) for tile icons.
    lv_style_init(&ui_style_chip);
    lv_style_set_radius(&ui_style_chip, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(&ui_style_chip, UI_COLOR_CARD);
    lv_style_set_bg_opa(&ui_style_chip, LV_OPA_COVER);
    lv_style_set_border_color(&ui_style_chip, UI_COLOR_TEXT_MUTED);
    lv_style_set_border_opa(&ui_style_chip, LV_OPA_30);
    lv_style_set_border_width(&ui_style_chip, 2);
    lv_style_set_text_color(&ui_style_chip, UI_COLOR_TEXT_MUTED);
    lv_style_set_shadow_width(&ui_style_chip, 0);
}

lv_obj_t* ui_theme_create_stat_tile(lv_obj_t* parent, lv_coord_t w, lv_coord_t h, lv_align_t align, lv_coord_t x_ofs,
                                    lv_coord_t y_ofs, const char* icon, const char* value, const char* caption)
{
    // Flex row (icon | text column), cross-axis centered -- LVGL centers both regardless of
    // their exact rendered heights, more robust than hand-picked pixel offsets.
    lv_obj_t* tile = lv_obj_create(parent);
    lv_obj_add_style(tile, &ui_style_card, 0);
    lv_obj_set_size(tile, w, h);
    lv_obj_align(tile, align, x_ofs, y_ofs);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* icon_chip = lv_obj_create(tile);
    lv_obj_add_style(icon_chip, &ui_style_chip, 0);
    lv_obj_set_size(icon_chip, 28, 28);
    lv_obj_clear_flag(icon_chip, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* icon_label = lv_label_create(icon_chip);
    lv_label_set_text(icon_label, icon);
    lv_obj_center(icon_label);

    lv_obj_t* text_col = lv_obj_create(tile);
    lv_obj_remove_style_all(text_col);
    lv_obj_set_size(text_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(text_col, 8, 0);
    lv_obj_set_flex_flow(text_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(text_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* value_label = lv_label_create(text_col);
    lv_label_set_text(value_label, value);

    lv_obj_t* caption_label = lv_label_create(text_col);
    lv_obj_set_style_text_color(caption_label, UI_COLOR_TEXT_MUTED, 0);
    lv_label_set_text(caption_label, caption);

    return value_label;
}

void ui_theme_set_chip_active(lv_obj_t* chip, bool active, lv_color_t accent)
{
    if (active)
    {
        lv_obj_set_style_border_color(chip, accent, 0);
        lv_obj_set_style_border_opa(chip, LV_OPA_70, 0);
        lv_obj_set_style_text_color(chip, accent, 0);
        lv_obj_set_style_shadow_color(chip, accent, 0);
        lv_obj_set_style_shadow_width(chip, 14, 0);
        lv_obj_set_style_shadow_opa(chip, LV_OPA_40, 0);
    }
    else
    {
        lv_obj_set_style_border_color(chip, UI_COLOR_TEXT_MUTED, 0);
        lv_obj_set_style_border_opa(chip, LV_OPA_30, 0);
        lv_obj_set_style_text_color(chip, UI_COLOR_TEXT_MUTED, 0);
        lv_obj_set_style_shadow_width(chip, 0, 0);
    }
}
