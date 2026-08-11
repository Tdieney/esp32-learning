#pragma once

#include "lvgl.h"

// Builds the Devices page (grouped, scrollable 2-column device card grid) as a child of
// `parent` (the shared content container) and returns the panel. Called once by ui_init().
lv_obj_t* ui_devices_create(lv_obj_t* parent);
