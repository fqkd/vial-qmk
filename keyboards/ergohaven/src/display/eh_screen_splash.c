#include "eh_display.h"
#include <lvgl.h>

#include "ergohaven.h"
#include "lvgl_helpers.h"

static lv_obj_t *screen_splash;

#ifdef ERGOHAVEN_MACROPAD_REV3_OZON_SPLASH
LV_IMG_DECLARE(ozon_tech_gosha_splash);
#else
LV_IMG_DECLARE(eh_logo);
#endif

void splash_screen_init(void) {
    screen_splash = lv_obj_create(NULL);
    lv_obj_add_style(screen_splash, &style_screen, 0);
#ifdef ERGOHAVEN_MACROPAD_REV3_OZON_SPLASH
    lv_obj_set_scrollbar_mode(screen_splash, LV_SCROLLBAR_MODE_OFF);
#else
    use_flex_column(screen_splash);
#endif

    lv_obj_t *img = lv_img_create(screen_splash);
#ifdef ERGOHAVEN_MACROPAD_REV3_OZON_SPLASH
    lv_img_set_src(img, &ozon_tech_gosha_splash);
    lv_obj_set_pos(img, 0, 0);
#else
    lv_img_set_src(img, &eh_logo);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_top(img, 60, 0);
    lv_obj_set_style_pad_bottom(img, 60, 0);
#endif
}

void splash_screen_load(void) {
    lv_scr_load(screen_splash);
}

void splash_screen_housekeep(void) {
}

const eh_screen_t eh_screen_splash = {
    .init      = splash_screen_init,
    .load      = splash_screen_load,
    .housekeep = splash_screen_housekeep,
};
