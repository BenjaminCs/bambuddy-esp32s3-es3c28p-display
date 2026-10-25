#pragma once

#include <lvgl.h>

// Builds the whole UI. Call once after the LVGL display and input device exist.
void ui_begin(lv_display_t *disp);

// Called from loop(): takes over fresh state from the network task and handles
// command results. Must run on the same task as lv_timer_handler().
void ui_tick();
