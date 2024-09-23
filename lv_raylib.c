#include "lv_raylib.h"
#include "raylib.h"
#include "raymath.h"

#include "lvgl/src/core/lv_refr_private.h"
#include "lvgl/src/display/lv_display_private.h"
#include "lvgl/src/misc/lv_area_private.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <GLFW/glfw3.h>

typedef struct {
    unsigned int texture_id;
    uint8_t * fb1;
} lv_raylib_opengles_texture_t;

struct lv_raylib_window_t {
    int32_t hor_res;
    int32_t ver_res;
    lv_ll_t textures;
    lv_point_t mouse_last_point;
    lv_indev_state_t mouse_last_state;
    lv_key_t keyboard_key;
    lv_indev_state_t keyboard_last_state;
};
typedef struct lv_raylib_window_t lv_raylib_window_t;

struct lv_raylib_texture_t {
    lv_raylib_window_t * window;
    unsigned int texture_id;
    lv_area_t area;
    lv_opa_t opa;
    lv_indev_t *lv_indev_mouse;
    lv_point_t indev_last_point;
    lv_indev_state_t indev_last_state;
    lv_indev_t *lv_indev_keyboard;
    lv_key_t indev_key;
    lv_indev_state_t indev_key_state;
    Texture2D rlTexture;
    Vector2 rlPosition;
};
typedef struct lv_raylib_texture_t lv_raylib_texture_t;

struct lv_raylib_window_t *lv_raylib_window = NULL;

static void flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
    LV_UNUSED(area);
    LV_UNUSED(px_map);

    if(lv_display_flush_is_last(display)) {

        lv_raylib_opengles_texture_t *dsc = lv_display_get_driver_data(display);

        glBindTexture(GL_TEXTURE_2D, dsc->texture_id);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        /*Color depth: 8 (A8), 16 (RGB565), 24 (RGB888), 32 (XRGB8888)*/
#if LV_COLOR_DEPTH == 8
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, display->hor_res, display->ver_res, 0, GL_RED, GL_UNSIGNED_BYTE, dsc->fb1);
#elif LV_COLOR_DEPTH == 16
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, display->hor_res, display->ver_res, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, dsc->fb1);
#elif LV_COLOR_DEPTH == 24
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, display->hor_res, display->ver_res, 0, GL_RGB, GL_UNSIGNED_BYTE, dsc->fb1);
#elif LV_COLOR_DEPTH == 32
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, display->hor_res, display->ver_res, 0, GL_RGBA, GL_UNSIGNED_BYTE, dsc->fb1);
#else
#error("Unsupported color format")
#endif
    }

    lv_display_flush_ready(display);
}

static void release_disp_cb(lv_event_t *ev) {
    lv_display_t *display = lv_event_get_user_data(ev);
    lv_raylib_opengles_texture_t *dsc = lv_display_get_driver_data(display);
    free(dsc->fb1);
    lv_free(dsc);
}

static lv_display_t *lv_raylib_opengles_texture_create(int32_t w, int32_t h) {
    lv_display_t *display = lv_display_create(w, h);
    if(display == NULL) {
        return NULL;
    }
    lv_raylib_opengles_texture_t * dsc = lv_malloc_zeroed(sizeof(lv_raylib_opengles_texture_t));
    LV_ASSERT_MALLOC(dsc);
    if(dsc == NULL) {
        lv_display_delete(display);
        return NULL;
    }
    uint32_t stride = lv_draw_buf_width_to_stride(w, lv_display_get_color_format(display));
    uint32_t buf_size = stride * w;
    dsc->fb1 = malloc(buf_size);
    if(dsc->fb1 == NULL) {
        lv_free(dsc);
        lv_display_delete(display);
        return NULL;
    }

    glGenTextures(1, &dsc->texture_id);
    glBindTexture(GL_TEXTURE_2D, dsc->texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    lv_display_set_buffers(display, dsc->fb1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display, flush_cb);
    lv_display_set_driver_data(display, dsc);
    lv_display_add_event_cb(display, release_disp_cb, LV_EVENT_DELETE, display);

    return display;
}

static unsigned int lv_raylib_opengles_texture_get_texture_id(lv_display_t *display)
{
    if(display->flush_cb != flush_cb) {
        return 0;
    }
    lv_raylib_opengles_texture_t *dsc = lv_display_get_driver_data(display);
    return dsc->texture_id;
}

static lv_display_t *lv_raylib_opengles_texture_get_from_texture_id(unsigned int texture_id) {
    lv_display_t *display = NULL;
    while(NULL != (display = lv_display_get_next(display))) {
        unsigned int disp_texture_id = lv_raylib_opengles_texture_get_texture_id(display);
        if(disp_texture_id == texture_id) {
            return display;
        }
    }
    return NULL;
}

static void lv_raylib_indev_mouse_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    lv_raylib_texture_t * texture = lv_indev_get_driver_data(indev);
    data->point = texture->indev_last_point;
    data->state = texture->indev_last_state;
}

static void lv_raylib_proc_mouse(lv_raylib_window_t *window) {
    /* mouse activity will affect the topmost LVGL display texture */
    lv_raylib_texture_t * texture;
    LV_LL_READ_BACK(&window->textures, texture) {
        if(lv_area_is_point_on(&texture->area, &window->mouse_last_point, 0)) {
            /* adjust the mouse pointer coordinates so that they are relative to the texture */
            texture->indev_last_point.x = window->mouse_last_point.x - texture->area.x1;
            texture->indev_last_point.y = window->mouse_last_point.y - texture->area.y1;
            texture->indev_last_state = window->mouse_last_state;
            lv_indev_read(texture->lv_indev_mouse);
            break;
        }
    }
}
#if 0
static void lv_raylib_indev_keyboard_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    lv_raylib_texture_t * texture = lv_indev_get_driver_data(indev);
    data->key = texture->indev_key;
    data->state = texture->indev_last_state;
    printf("key: %c\n", data->key);
}
static void lv_raylib_proc_keyboard(lv_raylib_window_t *window) {
    /* mouse activity will affect the topmost LVGL display texture */
    lv_raylib_texture_t * texture;
    LV_LL_READ_BACK(&window->textures, texture) {
        if(lv_area_is_point_on(&texture->area, &window->mouse_last_point, 0)) {
            /* adjust the mouse pointer coordinates so that they are relative to the texture */
            texture->indev_key_state = window->keyboard_last_state;
            texture->indev_key = window->keyboard_key;
            lv_indev_read(texture->lv_indev_keyboard);
            break;
        }
    }
}
#endif

void lv_raylib_window_create(unsigned int width, unsigned int height) {
    assert(lv_raylib_window == NULL);

    lv_raylib_window = lv_malloc(sizeof(struct lv_raylib_window_t));

    lv_raylib_window->hor_res = width;
    lv_raylib_window->ver_res = height;
    lv_ll_init(&lv_raylib_window->textures, sizeof(lv_raylib_texture_t));
}

lv_display_t *lv_raylib_display_create(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
    struct lv_raylib_window_t *window = lv_raylib_window;
    lv_display_t *display = lv_raylib_opengles_texture_create(width, height);
    if (display == NULL) {
        return NULL;
    }
    lv_display_set_default(display);
    unsigned int texture_id = lv_raylib_opengles_texture_get_texture_id(display);

    lv_raylib_texture_t *texture = lv_ll_ins_tail(&window->textures);
    LV_ASSERT_MALLOC(texture);
    if (texture == NULL) {
        return NULL;
    }
    lv_memzero(texture, sizeof(*texture));

    texture->window = window;
    texture->texture_id = texture_id;
    lv_area_set(&texture->area, 0, 0, width - 1, height - 1);
    texture->opa = LV_OPA_COVER;

    lv_indev_t *indev;
    indev = lv_indev_create();
    if(indev == NULL) {
        lv_ll_remove(&window->textures, texture);
        lv_free(texture);
        return NULL;
    }
    texture->lv_indev_mouse = indev;
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lv_raylib_indev_mouse_read_cb);
    lv_indev_set_driver_data(indev, texture);
    lv_indev_set_mode(indev, LV_INDEV_MODE_EVENT);
    lv_indev_set_display(indev, display);

#if 0
    indev = lv_indev_create();
    if(indev == NULL) {
        lv_ll_remove(&window->textures, texture);
        lv_free(texture);
        return NULL;
    }
    texture->lv_indev_keyboard = indev;
    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, lv_raylib_indev_keyboard_read_cb);
    lv_indev_set_driver_data(indev, texture);
    lv_indev_set_mode(indev, LV_INDEV_MODE_EVENT);
    lv_indev_set_display(indev, display);
#endif

    lv_area_set_pos(&texture->area, x, y);
    lv_display_delete_refr_timer(display);

    texture->rlTexture.id = texture_id;
    texture->rlTexture.width = width;
    texture->rlTexture.height = height;
    texture->rlTexture.mipmaps = 1;
    texture->rlTexture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8; // 32 bpp
    texture->rlPosition = (Vector2){ x, y };

    return display;
}

void lv_raylib_display_update(void) {
    struct lv_raylib_window_t *window = lv_raylib_window;

    lv_tick_inc(GetFrameTime()*1000);
    //
    // mouse
    static Vector2 mouseDelta = { 0.0, 0.0 };
    if (Vector2Length(Vector2Subtract(mouseDelta, GetMouseDelta()))) {
        window->mouse_last_point.x = (int32_t)GetMouseX();
        window->mouse_last_point.y = (int32_t)GetMouseY();
        lv_raylib_proc_mouse(window);
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        window->mouse_last_state = LV_INDEV_STATE_PRESSED;
        lv_raylib_proc_mouse(window);
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        window->mouse_last_state = LV_INDEV_STATE_RELEASED;
        lv_raylib_proc_mouse(window);
    }

#if 0
    // keyboard
    for (int i = 0; i < 10; i++) {
        int keyPressed = GetKeyPressed();
        if (keyPressed == 0) {
            break;
        }
        window->keyboard_last_state = LV_INDEV_STATE_PRESSED;
        window->keyboard_key = keyPressed;
        lv_raylib_proc_keyboard(window);
        window->keyboard_last_state = LV_INDEV_STATE_RELEASED;
        lv_raylib_proc_keyboard(window);
    }
#endif
}

void lv_raylib_display_draw(void) {
    struct lv_raylib_window_t *window = lv_raylib_window;
    /* render each texture in the window */
    lv_raylib_texture_t *texture;
    LV_LL_READ(&window->textures, texture) {
        /* if the added texture is an LVGL opengles texture display, refresh it before rendering it */
        lv_display_t *display = lv_raylib_opengles_texture_get_from_texture_id(texture->texture_id);
        lv_display_set_default(display);
        _lv_display_refr_timer(NULL);
        lv_refr_now(display); // animation and display handled now

        DrawTexture(texture->rlTexture, texture->rlPosition.x, texture->rlPosition.y, WHITE);
    }
}

void lv_raylib_window_delete(void) {
    struct lv_raylib_window_t *window = lv_raylib_window;
    lv_raylib_texture_t * texture;
    LV_LL_READ(&window->textures, texture) {
        lv_indev_delete(texture->lv_indev_mouse);
        lv_indev_delete(texture->lv_indev_keyboard);
    }
    lv_ll_clear(&window->textures);
    lv_free(window);
    lv_raylib_window = NULL;
}


void lv_raylib_display_remove(lv_raylib_texture_t *texture) {
    lv_indev_delete(texture->lv_indev_mouse);
    lv_indev_delete(texture->lv_indev_keyboard);
    lv_ll_remove(&texture->window->textures, texture);
    lv_free(texture);
}
