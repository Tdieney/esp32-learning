#pragma once

#include <stdbool.h>
#include "lvgl.h"

// Builds the Room Control page (room tabs, interactive temperature dial, mode chips, stat
// tiles) as a child of `parent` (the shared content container) and returns the panel. Called
// once by ui_init().
lv_obj_t* ui_room_create(lv_obj_t* parent);

// Selects which room's mock state Room Control displays -- called by the Home page before
// navigating here, so tapping a room card lands directly on that room.
void ui_room_select_room(int index);

// Room Control owns the single mock "room model" (name, on/off, temperature, mode, device
// count) so Home's room grid and Room Control's own tabs never see two different copies of
// the same state -- Home reads/writes through these instead of keeping its own room array.
int ui_room_get_count(void);
const char* ui_room_get_name(int index);
int ui_room_get_device_count(int index);
bool ui_room_get_on(int index);
void ui_room_set_on(int index, bool on);
