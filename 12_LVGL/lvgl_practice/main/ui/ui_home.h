#pragma once

#include "lvgl.h"

// Builds the Home page (greeting, stat tiles, room grid) as a child of `parent` (the shared
// content container) and returns the panel. Called once by ui_init().
lv_obj_t* ui_home_create(lv_obj_t* parent);
