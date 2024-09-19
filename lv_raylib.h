#ifndef LV_RAYLIB_H
#define LV_RAYLIB_H

#include "lvgl/lvgl.h"

void lv_raylib_window_create(unsigned int width, unsigned int height);
void lv_raylib_window_delete(void);
lv_display_t *lv_raylib_display_create(unsigned int x, unsigned int y, unsigned int width, unsigned int height);
void lv_raylib_display_draw(void);
void lv_raylib_display_update(void);

#endif // LV_RAYLIB_H
