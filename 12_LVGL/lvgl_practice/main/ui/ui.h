#pragma once

// Creates the root screen (persistent sidebar + content container), builds all 3 pages once,
// and shows Home. Call after the display/touch drivers are registered with LVGL (i.e. at the
// end of init_lvgl(), before creating gui_task).
void ui_init(void);

// Navigation: shows one page in the content container (fades it in) and hides the other two.
// Nothing is destroyed or recreated -- see UI_PLAN.md's revised architecture section.
void ui_show_home(void);
void ui_show_devices(void);
void ui_show_room(void);
