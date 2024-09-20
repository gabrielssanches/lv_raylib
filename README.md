## lv\_raylib

This is the middleware so raylib can use lvgl (opengles driver)

It is based on lvgl opengles driver from v9.2

### using it

Just include lv_raylib.h in your projects include path and compile lv_raylib.c

CFLAGS and LDLIBS from raylib's Makefile template for PLATFORM_DESKTOP are enough to compile this

see reference project https://github.com/gabrielssanches/raylib-lvgl-demo-widgets
