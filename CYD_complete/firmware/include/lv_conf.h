// LVGL 8.3 config for ESP32 CYD 320x240 (no PSRAM). Unlisted options use LVGL defaults.
#if 1  /* enable */
#ifndef LV_CONF_H
#define LV_CONF_H
#include <stdint.h>

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0           // flush uses pushImageDMA with native RGB565 (swap done there)

// LVGL memory: ONE fixed pool that lv_init() takes from the heap with malloc (not a static array -> no ".dram0.bss will not fit").
// lvUiPreInit() runs in setup() BEFORE WiFi starts, so the pool is always available. With LV_MEM_CUSTOM=1 (old setting) every LVGL object
// was a separate malloc: WiFi + web server fragmented the heap, an allocation failed, LVGL had asserts off and crashed on a NULL pointer.
// Size: -DLV_MEM_KB=.. in platformio.ini (the serial log prints "lvgl: used x% free=.. biggest=.." every 10 s: raise it if "free" drops under ~8 KB).
#ifndef LV_MEM_KB
#define LV_MEM_KB 56
#endif
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (LV_MEM_KB * 1024U)
#define LV_MEM_ADR 0
#define LV_MEM_POOL_INCLUDE <stdlib.h>
#define LV_MEM_POOL_ALLOC   malloc

#define LV_DISP_DEF_REFR_PERIOD 30   // ~33 fps is the most this SPI panel can show anyway; leaves CPU for WiFi
#define LV_INDEV_DEF_READ_PERIOD 20

#define LV_TICK_CUSTOM 0             // lv_tick_inc() is called from lvUiTick()

#define LV_DPI_DEF 130
#define LV_LAYER_SIMPLE_BUF_SIZE (4 * 1024)          // smaller temp layer buffers (RAM!)
#define LV_LAYER_SIMPLE_FALLBACK_BUF_SIZE (1 * 1024)
#define LV_SHADOW_CACHE_SIZE 0
#define LV_IMG_CACHE_DEF_SIZE 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 0
#define LV_USE_ASSERT_MALLOC 0
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ 0

#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_GPU_NXP_PXP 0
#define LV_USE_GPU_NXP_VG_LITE 0
#define LV_USE_GPU_SDL 0

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14
#define LV_USE_FONT_PLACEHOLDER 1

#define LV_USE_USER_DATA 1
#define LV_ATTRIBUTE_FAST_MEM

/* widgets */
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BTN 1
#define LV_USE_BTNMATRIX 1
#define LV_USE_CANVAS 1
#define LV_USE_CHECKBOX 1
#define LV_USE_DROPDOWN 1
#define LV_USE_IMG 1
#define LV_USE_LABEL 1
#define LV_USE_LINE 1
#define LV_USE_ROLLER 1
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_TEXTAREA 1
#define LV_USE_TABLE 1
#define LV_USE_CHART 1
#define LV_USE_KEYBOARD 1
#define LV_USE_LIST 1
#define LV_USE_METER 1
#define LV_USE_TABVIEW 1
#define LV_USE_TILEVIEW 1
#define LV_USE_MSGBOX 1
#define LV_USE_SPAN 1
#define LV_USE_WIN 0
#define LV_USE_CALENDAR 0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN 1
#define LV_USE_LED 1
#define LV_USE_MENU 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 1
#define LV_USE_ANIMIMG 0

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_THEME_DEFAULT_GROW 0
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

#define LV_USE_FLEX 1
#define LV_USE_GRID 1
#endif
#endif
