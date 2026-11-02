// Phone-style UI for the 320x240 CYD (LVGL 8.3): lock screen, home screen with app icons, status bar,
// pull-down control center, home pill, app transitions.  Apps: Monitor, CPU, Memory (+disk read/write), Network, Tasks, Remote, Gallery (SD videos / GIFs / photos), Clock, Settings, Info.
// Alerts: red blinking status bar (tap = snooze 5 min), screen never sleeps while an alert is active.
// Same integration contract as the other UIs: main.cpp calls lvUiInit() / lvUiTick().
#pragma once
#ifdef USE_LVGL
#include <lvgl.h>
#include <time.h>
#include <Preferences.h>

#define LV_HOR 320
#define LV_VER 240

#ifndef PH_TZ
#define PH_TZ "<+06>-6"                          // POSIX TZ string. Dhaka = UTC+6
#endif

// ---------------- palette ----------------
#define PC_WALL1 0x1B1464
#define PC_WALL2 0x061A33
#define PC_APPBG 0x0A0E16
#define PC_CARD  0x141A26
#define PC_CARD2 0x1D2535
#define PC_TX    0xF1F5F9
#define PC_MUT   0x8B97AB
#define PC_CY    0x22D3EE
#define PC_VIO   0xA78BFA
#define PC_GRN   0x34D399
#define PC_ORG   0xFB923C
#define PC_RED   0xF87171
#define PC_PNK   0xF472B6
#define PC_BLU   0x60A5FA
#define PC_YEL   0xFACC15
static inline lv_color_t K(uint32_t h) { return lv_color_hex(h); }
// lv_label_set_text() always invalidates the label. Most labels are refreshed every second with the SAME text -> skip those.
static inline void phSet(lv_obj_t* l, const char* t) {
  const char* c = lv_label_get_text(l);
  if (c && !strcmp(c, t)) return;
  lv_label_set_text(l, t);
}
static lv_color_t phC565(uint16_t c) { return lv_color_make(((c >> 11) & 31) * 255 / 31, ((c >> 5) & 63) * 255 / 63, (c & 31) * 255 / 31); }

// ---------------- display + touch drivers ----------------
static lv_disp_draw_buf_t phDraw;
static lv_disp_drv_t phDrv;
static lv_indev_drv_t phIn;
static lv_color_t *phBuf1 = nullptr, *phBuf2 = nullptr;
static bool phSwallowNext = false;               // finger that closed the media player must not click the UI underneath
static bool phMediaArmed = true;                 // media player: wait until the finger that started it is lifted

static void phFlush(lv_disp_drv_t* d, const lv_area_t* a, lv_color_t* c) {
  int32_t w = a->x2 - a->x1 + 1, h = a->y2 - a->y1 + 1;
  tft.waitDMA();
  tft.pushImageDMA(a->x1, a->y1, w, h, (const lgfx::rgb565_t*)c);
  lv_disp_flush_ready(d);
}

// raw XPT2046 values -> screen pixels (same mapping for LVGL and for the video player)
static void phMapTouch(int rx, int ry, int& x, int& y) {
#ifdef TOUCH_SWAP_XY
  { int t = rx; rx = ry; ry = t; }
#endif
  touchToScreen(rx, ry, x, y);
#ifdef TOUCH_INV_X
  x = LV_HOR - 1 - x;
#endif
#ifdef TOUCH_INV_Y
  y = LV_VER - 1 - y;
#endif
  if (cfg.rotation == 3) { x = LV_HOR - 1 - x; y = LV_VER - 1 - y; }
  x = constrain(x, 0, LV_HOR - 1); y = constrain(y, 0, LV_VER - 1);
}

static void phTouchRead(lv_indev_drv_t*, lv_indev_data_t* d) {
  static int lx = 0, ly = 0, miss = 0;
  static bool pressed = false, swallow = false;
  static uint32_t last = 0;
  uint32_t n = millis();
  if (n - last >= 10) {
    last = n;
    int rx, ry;
    bool p = touchRaw(rx, ry);
    if (p) {
      gLastAct = n;
      if (phSwallowNext) { swallow = true; phSwallowNext = false; }
      if (gAsleep) { gAsleep = false; swallow = true; applyBrightness(); }
      int x, y;
      phMapTouch(rx, ry, x, y);
      lx = x; ly = y;
      miss = 0; pressed = true;
    } else if (pressed && ++miss >= 4) { pressed = false; swallow = false; }
  }
  d->point.x = lx; d->point.y = ly;
  d->state = (pressed && !swallow) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// =====================================================================
//  small helpers
// =====================================================================
enum PhScr { PH_NONE, PH_LOCK, PH_HOME, PH_MON, PH_CPU, PH_MEM, PH_NET, PH_CLK, PH_GAL, PH_SET, PH_INFO, PH_TASKS, PH_RMT };
static PhScr phCur = PH_NONE;
static bool phLockOnWake = true;                 // show the lock screen again after the screen slept (NVS "phui")
static bool phNeedLock = false;
static float phNetPeak = 200;

static uint32_t phLastTick = 0;
static inline void phTickInc() { uint32_t n = millis(); if (n != phLastTick) { lv_tick_inc(n - phLastTick); phLastTick = n; } }

static void phOpen(PhScr id);
static void phHome();
static void phLeaveCur();
static void phShowLock();

static bool phNow(struct tm& t) {
  time_t n = time(nullptr);
  if (n < 1700000000) return false;              // NTP not synced yet
  localtime_r(&n, &t);
  return true;
}

static lv_obj_t* phBox(lv_obj_t* p, int x, int y, int w, int h, uint32_t col, int r, int opa = 255) {
  lv_obj_t* o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, K(col), 0);
  lv_obj_set_style_bg_opa(o, opa, 0);
  lv_obj_set_style_radius(o, r, 0);
  lv_obj_clear_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  return o;
}
static lv_obj_t* phLbl(lv_obj_t* p, const char* t, const lv_font_t* f, uint32_t col) {
  lv_obj_t* l = lv_label_create(p);
  phSet(l, t);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, K(col), 0);
  return l;
}
static lv_obj_t* phCard(lv_obj_t* p, int x, int y, int w, int h, uint32_t col = PC_CARD) {
  lv_obj_t* c = phBox(p, x, y, w, h, col, 16);
  lv_obj_set_style_shadow_width(c, 0, 0);   // shadows need big temp RAM
  lv_obj_set_style_shadow_ofs_y(c, 3, 0);
  lv_obj_set_style_shadow_opa(c, 90, 0);
  lv_obj_set_style_shadow_color(c, K(0x000000), 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_border_color(c, K(0x232C3D), 0);
  return c;
}
static lv_obj_t* phGlass(lv_obj_t* p, int x, int y, int w, int h, int r) {
  lv_obj_t* c = phBox(p, x, y, w, h, 0xFFFFFF, r, 30);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_border_color(c, K(0xFFFFFF), 0);
  lv_obj_set_style_border_opa(c, 60, 0);
  return c;
}
static lv_obj_t* phScreen(uint32_t c1, uint32_t c2 = 0xFFFFFFFF) {
  lv_obj_t* s = lv_obj_create(NULL);
  lv_obj_remove_style_all(s);
  lv_obj_set_size(s, LV_HOR, LV_VER);
  lv_obj_set_style_bg_color(s, K(c1), 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  if (c2 != 0xFFFFFFFF) { lv_obj_set_style_bg_grad_color(s, K(c2), 0); lv_obj_set_style_bg_grad_dir(s, LV_GRAD_DIR_VER, 0); }
  lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}
static void phGo(lv_obj_t* s, lv_scr_load_anim_t a, uint32_t t = 240) { lv_scr_load_anim(s, a, t, 0, true); }

static void phAnimY(void* o, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t*)o, v, 0); }
static void phAnimO(void* o, int32_t v) { lv_obj_set_style_opa((lv_obj_t*)o, (lv_opa_t)v, 0); }
#ifndef PH_ENTER_ANIM
#define PH_ENTER_ANIM 0                                               // entrance animations cost CPU on every screen change: off by default (smoother)
#endif
static void phRise(lv_obj_t* o, uint32_t delay) {
  if (!PH_ENTER_ANIM) return;
  lv_obj_set_style_translate_y(o, 16, 0);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, o);
  lv_anim_set_time(&a, 320);
  lv_anim_set_delay(&a, delay);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&a, phAnimY); lv_anim_set_values(&a, 16, 0); lv_anim_start(&a);
}
static void phArcExec(void* o, int32_t v) { lv_arc_set_value((lv_obj_t*)o, (int16_t)v); }
static void phArcTo(lv_obj_t* arc, int v) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, arc);
  lv_anim_set_exec_cb(&a, phArcExec);
  lv_anim_set_values(&a, lv_arc_get_value(arc), v);
  lv_anim_set_time(&a, 500);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}
static void phRateShort(char* b, size_t n, float kb) { if (kb >= 1024) snprintf(b, n, "%.1fM", kb / 1024.0f); else snprintf(b, n, "%.0fK", kb); }
static uint32_t phLoadCol(float v) { return v >= 90 ? PC_RED : (v >= 75 ? PC_ORG : PC_GRN); }

// pressed-feedback style for icons / buttons
static lv_style_t phStBase, phStPress;
static void phStylesInit() {
  lv_style_init(&phStBase);
  lv_style_init(&phStPress);
  lv_style_set_translate_y(&phStPress, 2);                             // cheap pressed feedback: no zoom, no layer buffers
}
static void phPressable(lv_obj_t* o) {
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_style(o, &phStBase, 0);
  lv_obj_add_style(o, &phStPress, LV_STATE_PRESSED);
}

// =====================================================================
//  CHROME on the top layer: status bar, home pill, control center
// =====================================================================
static lv_obj_t *phStrip, *phClkL, *phHostL, *phPcDot, *phWifiB[4], *phPill, *phShade;
static lv_obj_t *shTile[4], *shTileLbl[4], *shSlider, *shStats;
static bool phShadeOn = false;

static void phChrome(bool status, bool pill) {
  if (status) lv_obj_clear_flag(phStrip, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(phStrip, LV_OBJ_FLAG_HIDDEN);
  if (pill) lv_obj_clear_flag(phPill, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(phPill, LV_OBJ_FLAG_HIDDEN);
}

static void phStatus() {
  char b[16];
  struct tm t;
  if (phNow(t)) snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min); else snprintf(b, sizeof(b), "%s", S.clk);
  phSet(phClkL, b);
  bool wc = WiFi.status() == WL_CONNECTED;
  int r = wc ? WiFi.RSSI() : -100;
  int lvl = !wc ? 0 : (r > -55 ? 4 : (r > -65 ? 3 : (r > -75 ? 2 : 1)));
  for (int i = 0; i < 4; i++) lv_obj_set_style_bg_color(phWifiB[i], i < lvl ? K(0xFFFFFF) : K(0x4B5568), 0);
  lv_obj_set_style_bg_color(phPcDot, pcOnline ? K(PC_GRN) : (wc ? K(PC_ORG) : K(PC_RED)), 0);
  if (alertActive) {                                                   // alert: blinking red bar with the alert text
    bool bl = (millis() / 500) & 1;
    lv_obj_set_style_bg_color(phStrip, K(bl ? 0xB91C1C : 0x7F1D1D), 0);
    lv_obj_set_style_bg_opa(phStrip, LV_OPA_COVER, 0);
    phSet(phHostL, alertMsg);
    lv_obj_set_style_text_color(phHostL, K(0xFFFFFF), 0);
    return;
  }
  lv_obj_set_style_bg_opa(phStrip, 0, 0);
  lv_obj_set_style_text_color(phHostL, K(PC_MUT), 0);
  phSet(phHostL, pcOnline && S.host[0] && S.host[0] != '-' ? S.host : (wc ? "PC offline" : "No Wi-Fi"));
}

// ---- control center (pull-down) ----
static void phShadeY(void* o, int32_t v) { lv_obj_set_y((lv_obj_t*)o, (lv_coord_t)v); }
static void phBuildShade();
static void phShadeFree(void*) { if (!phShadeOn && phShade) { lv_obj_del(phShade); phShade = nullptr; } }   // frees ~6 KB while closed
static void phShadeDone(lv_anim_t*) { if (!phShadeOn) lv_async_call(phShadeFree, NULL); }
static void phShadeFill() {
  if (!phShade) return;
  bool wc = WiFi.status() == WL_CONNECTED;
  lv_obj_set_style_bg_color(shTile[0], K(wc ? PC_CY : PC_CARD2), 0);
  lv_obj_set_style_bg_color(shTile[1], K(pcOnline ? PC_GRN : PC_CARD2), 0);
  char b[40];
  if (wc) snprintf(b, sizeof(b), "%d dBm", (int)WiFi.RSSI()); else strcpy(b, "Wi-Fi off");
  phSet(shTileLbl[0], b);
  phSet(shTileLbl[1], pcOnline ? "PC online" : "PC offline");
  if (!lv_obj_has_state(shSlider, LV_STATE_PRESSED)) lv_slider_set_value(shSlider, cfg.bright, LV_ANIM_OFF);
  snprintf(b, sizeof(b), "CPU %.0f%%   RAM %.0f%%   %s", S.cpu, S.ram, S.temp > 0 ? (String((int)S.temp) + " C").c_str() : "");
  phSet(shStats, b);
}
static void phShadeSet(bool on) {
  if (on == phShadeOn) return;
  if (!on && !phShade) { phShadeOn = false; return; }
  phShadeOn = on;
  if (on) { if (!phShade) phBuildShade(); phShadeFill(); }
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, phShade);
  lv_anim_set_exec_cb(&a, phShadeY);
  lv_anim_set_values(&a, lv_obj_get_y(phShade), on ? 0 : -LV_VER);
  lv_anim_set_time(&a, 260);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_ready_cb(&a, phShadeDone);
  lv_anim_start(&a);
}
static void phStatus();
static void phStripEvt(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED && alertActive) {         // tap the red bar = snooze the alert for 5 min
    alertSnoozeUntil = millis() + 300000UL; alertEval(); phStatus();
    return;
  }
  if (lv_event_get_code(e) == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_get_act()) != LV_DIR_BOTTOM) return;
  phShadeSet(true);
}
static void phShadeCloseEvt(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_get_act()) != LV_DIR_TOP) return;
  phShadeSet(false);
}
static void phShadeSlider(lv_event_t* e) {
  lv_obj_t* s = lv_event_get_target(e);
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) { cfg.bright = (uint8_t)lv_slider_get_value(s); gLastAct = millis(); applyBrightness(); }
  else if (lv_event_get_code(e) == LV_EVENT_RELEASED) saveCfg();
}
static void phShadeTile(lv_event_t* e) {
  int id = (int)(intptr_t)lv_event_get_user_data(e);
  if (id == 2) { phShadeSet(false); gAsleep = true; applyBrightness(); }          // sleep: screen off, any touch wakes
  else if (id == 3) { phShadeSet(false); phShowLock(); }
}
static void phPillEvt(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_get_act()) != LV_DIR_TOP) return;
  phHome();
}

static lv_obj_t* phShadeTileBuild(lv_obj_t* p, int i, const char* sym, const char* cap) {
  int x = 8 + i * 78;
  lv_obj_t* t = phBox(p, x, 30, 70, 70, PC_CARD, 18);
  lv_obj_set_style_border_width(t, 1, 0);
  lv_obj_set_style_border_color(t, K(0x232C3D), 0);
  lv_obj_t* c = phBox(t, 18, 8, 34, 34, PC_CARD2, 17);
  shTile[i] = c;
  lv_obj_t* g = phLbl(c, sym, &lv_font_montserrat_16, PC_TX);
  lv_obj_center(g);
  shTileLbl[i] = phLbl(t, cap, &lv_font_montserrat_12, PC_MUT);
  lv_obj_align(shTileLbl[i], LV_ALIGN_BOTTOM_MID, 0, -6);
  if (i >= 2) {
    phPressable(t);
    lv_obj_add_event_cb(t, phShadeTile, LV_EVENT_CLICKED, (void*)(intptr_t)i);
  }
  return t;
}

static void phBuildShade() {
  lv_obj_t* top = lv_layer_top();
  phShade = phBox(top, 0, -LV_VER, LV_HOR, LV_VER, 0x070A11, 0, 244);
  lv_obj_add_flag(phShade, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(phShade, phShadeCloseEvt, LV_EVENT_GESTURE, NULL);
  phShadeTileBuild(phShade, 0, LV_SYMBOL_WIFI, "Wi-Fi");
  phShadeTileBuild(phShade, 1, LV_SYMBOL_USB, "PC");
  phShadeTileBuild(phShade, 2, LV_SYMBOL_POWER, "Sleep");
  phShadeTileBuild(phShade, 3, LV_SYMBOL_EYE_CLOSE, "Lock");
  lv_obj_t* bc = phCard(phShade, 8, 108, 304, 52);
  lv_obj_t* sun = phLbl(bc, LV_SYMBOL_EYE_OPEN, &lv_font_montserrat_16, PC_YEL);
  lv_obj_align(sun, LV_ALIGN_LEFT_MID, 12, 0);
  shSlider = lv_slider_create(bc);
  lv_obj_set_size(shSlider, 236, 12);
  lv_obj_align(shSlider, LV_ALIGN_RIGHT_MID, -14, 0);
  lv_slider_set_range(shSlider, 5, 255);
  lv_slider_set_value(shSlider, cfg.bright, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(shSlider, K(PC_CARD2), LV_PART_MAIN);
  lv_obj_set_style_bg_color(shSlider, K(PC_YEL), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(shSlider, K(0xFFFFFF), LV_PART_KNOB);
  lv_obj_set_style_pad_all(shSlider, 4, LV_PART_KNOB);
  lv_obj_add_flag(shSlider, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_set_ext_click_area(shSlider, 12);
  lv_obj_add_event_cb(shSlider, phShadeSlider, LV_EVENT_ALL, NULL);
  lv_obj_t* sc = phCard(phShade, 8, 168, 304, 40);
  shStats = phLbl(sc, "-", &lv_font_montserrat_14, PC_TX);
  lv_obj_center(shStats);
  lv_obj_t* hd = phBox(phShade, 110, 214, 100, 26, 0xFFFFFF, 0, 0);
  lv_obj_add_flag(hd, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(hd, phShadeCloseEvt, LV_EVENT_CLICKED, NULL);
  lv_obj_t* hb = phBox(hd, 18, 14, 64, 4, 0xFFFFFF, 2, 150);
  (void)hb;
  lv_obj_move_to_index(phShade, 0);                                    // keep the status bar / pill above it
}

static void phBuildChrome() {
  lv_obj_t* top = lv_layer_top();

  // status bar
  phStrip = phBox(top, 0, 0, LV_HOR, 22, 0x000000, 0, 0);
  lv_obj_add_flag(phStrip, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(phStrip, 6);
  lv_obj_add_event_cb(phStrip, phStripEvt, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(phStrip, phStripEvt, LV_EVENT_GESTURE, NULL);
  phClkL = phLbl(phStrip, "--:--", &lv_font_montserrat_14, PC_TX);
  lv_obj_align(phClkL, LV_ALIGN_LEFT_MID, 12, 0);
  phPcDot = phBox(phStrip, 122, 8, 7, 7, PC_RED, 4);
  phHostL = phLbl(phStrip, "-", &lv_font_montserrat_12, PC_MUT);
  lv_obj_set_pos(phHostL, 134, 4);
  for (int i = 0; i < 4; i++) phWifiB[i] = phBox(phStrip, 290 + i * 5, 14 - (2 + i * 2), 3, 3 + i * 2, 0x4B5568, 1);
  // pill (tap or swipe up = home)
  phPill = phBox(top, (LV_HOR - 120) / 2, LV_VER - 22, 120, 22, 0x000000, 0, 0);
  lv_obj_add_flag(phPill, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(phPill, phPillEvt, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(phPill, phPillEvt, LV_EVENT_GESTURE, NULL);
  phBox(phPill, 28, 13, 64, 4, 0xFFFFFF, 2, 190);
}

// =====================================================================
//  LOCK SCREEN
// =====================================================================
static lv_obj_t *lkTime, *lkDate, *lkL1, *lkL2, *lkHint;
static void phLockUpdate() {
  struct tm t;
  char b[48];
  if (phNow(t)) {
    snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min); phSet(lkTime, b);
    strftime(b, sizeof(b), "%A, %d %B", &t); phSet(lkDate, b);
  } else { phSet(lkTime, S.clk); phSet(lkDate, "waiting for time sync"); }
  if (apMode && WiFi.status() != WL_CONNECTED) { phSet(lkL1, "Wi-Fi setup"); phSet(lkL2, "open 192.168.4.1"); }
  else if (pcOnline) {
    snprintf(b, sizeof(b), "%s", (S.host[0] && S.host[0] != '-') ? S.host : "PC");
    phSet(lkL1, b);
    snprintf(b, sizeof(b), "CPU %.0f%%   RAM %.0f%%   %s", S.cpu, S.ram, S.temp > 0 ? (String((int)S.temp) + " C").c_str() : "");
    phSet(lkL2, b);
  } else {
    phSet(lkL1, WiFi.status() == WL_CONNECTED ? "PC offline" : "No Wi-Fi");
    phSet(lkL2, WiFi.status() == WL_CONNECTED ? "waiting for the PC agent" : "connecting...");
  }
}
static void phUnlock() { if (phCur == PH_LOCK) phHome(); }
static void phLockEvt(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_get_act()) != LV_DIR_TOP) return;
  phUnlock();
}
static void phHintExec(void* o, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t*)o, v, 0); }
static lv_obj_t* phBuildLock() {
  phCur = PH_LOCK;
  lv_obj_t* s = phScreen(PC_WALL1, PC_WALL2);
  lkTime = phLbl(s, "--:--", &lv_font_montserrat_48, 0xFFFFFF);
  lv_obj_align(lkTime, LV_ALIGN_TOP_MID, 0, 34);
  lkDate = phLbl(s, "-", &lv_font_montserrat_14, 0xC7D2FE);
  lv_obj_align(lkDate, LV_ALIGN_TOP_MID, 0, 94);
  lv_obj_t* g = phGlass(s, 16, 126, 288, 58, 18);
  lv_obj_t* ic = phBox(g, 12, 13, 32, 32, PC_CY, 10);
  lv_obj_center(phLbl(ic, LV_SYMBOL_EYE_OPEN, &lv_font_montserrat_16, 0x06202B));
  lkL1 = phLbl(g, "-", &lv_font_montserrat_14, 0xFFFFFF);
  lv_obj_set_pos(lkL1, 56, 9);
  lkL2 = phLbl(g, "-", &lv_font_montserrat_12, 0xDDE3FF);
  lv_obj_set_pos(lkL2, 56, 32);
  lkHint = phLbl(s, LV_SYMBOL_UP "\nSwipe up to unlock", &lv_font_montserrat_12, 0xC7D2FE);
  lv_obj_set_style_text_align(lkHint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lkHint, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_anim_t a;                                                        // gentle bobbing hint
  lv_anim_init(&a);
  lv_anim_set_var(&a, lkHint);
  lv_anim_set_exec_cb(&a, phHintExec);
  lv_anim_set_values(&a, 0, -6);
  lv_anim_set_time(&a, 900);
  lv_anim_set_playback_time(&a, 900);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_start(&a);
  lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s, phLockEvt, LV_EVENT_GESTURE, NULL);
  lv_obj_add_event_cb(s, phLockEvt, LV_EVENT_CLICKED, NULL);
  phLockUpdate();
  phChrome(true, false);
  return s;
}
static void phShowLock() {
  phShadeSet(false);
  phLeaveCur();
  lv_obj_t* s = phBuildLock();
  phGo(s, LV_SCR_LOAD_ANIM_OVER_BOTTOM, 220);
}

// =====================================================================
//  HOME SCREEN
// =====================================================================
static lv_obj_t *hmHost, *hmState, *hmV[3];
static void phHomeUpdate() {
  char b[24];
  phSet(hmHost, (S.host[0] && S.host[0] != '-') ? S.host : "PC Monitor");
  bool wc = WiFi.status() == WL_CONNECTED;
  phSet(hmState, pcOnline ? "online" : (wc ? "offline" : "no Wi-Fi"));
  lv_obj_set_style_text_color(hmState, K(pcOnline ? PC_GRN : (wc ? PC_ORG : PC_RED)), 0);
  snprintf(b, sizeof(b), "%.0f%%", S.cpu); phSet(hmV[0], b);
  snprintf(b, sizeof(b), "%.0f%%", S.ram); phSet(hmV[1], b);
  if (S.temp > 0) snprintf(b, sizeof(b), "%.0f C", S.temp); else strcpy(b, "-");
  phSet(hmV[2], b);
}

static void phIconEvt(lv_event_t* e) { phOpen((PhScr)(intptr_t)lv_event_get_user_data(e)); }
enum { GL_SYM, GL_CLOCK, GL_LIGHT, GL_INFO };
#define PHS(v) ((v) * 44 / 48)
static lv_obj_t* phIcon(lv_obj_t* s, int col, int row, const char* name, uint32_t c1, uint32_t c2, int kind, const char* sym, PhScr tgt) {
  lv_obj_t* cell = phBox(s, col * 64, 86 + row * 68, 64, 64, 0x000000, 0, 0);
  phPressable(cell);
  lv_obj_add_event_cb(cell, phIconEvt, LV_EVENT_CLICKED, (void*)(intptr_t)tgt);
  lv_obj_t* t = phBox(cell, 10, 0, 44, 44, c1, 13);
  lv_obj_set_style_bg_grad_color(t, K(c2), 0);
  lv_obj_set_style_bg_grad_dir(t, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_shadow_width(t, 0, 0);
  if (kind == GL_SYM) lv_obj_center(phLbl(t, sym, &lv_font_montserrat_20, 0xFFFFFF));
  else if (kind == GL_CLOCK) {
    lv_obj_t* f = phBox(t, PHS(10), PHS(10), PHS(28), PHS(28), 0xFFFFFF, 14, 0);
    lv_obj_set_style_border_width(f, 3, 0); lv_obj_set_style_border_color(f, K(0xFFFFFF), 0);
    phBox(t, PHS(23), PHS(15), 3, PHS(11), 0xFFFFFF, 1);
    phBox(t, PHS(23), PHS(23), PHS(9), 3, 0xFFFFFF, 1);
  } else if (kind == GL_LIGHT) {
    lv_obj_t* f = phBox(t, PHS(8), PHS(8), PHS(32), PHS(32), 0xFFFFFF, 16, 0);
    lv_obj_set_style_border_width(f, 2, 0); lv_obj_set_style_border_color(f, K(0xFFFFFF), 0); lv_obj_set_style_border_opa(f, 140, 0);
    phBox(t, PHS(15), PHS(15), PHS(18), PHS(18), 0xFFFFFF, 9);
  } else {
    lv_obj_t* f = phBox(t, PHS(10), PHS(10), PHS(28), PHS(28), 0xFFFFFF, 14, 0);
    lv_obj_set_style_border_width(f, 3, 0); lv_obj_set_style_border_color(f, K(0xFFFFFF), 0);
    lv_obj_center(phLbl(t, "i", &lv_font_montserrat_20, 0xFFFFFF));
  }
  lv_obj_t* l = phLbl(cell, name, &lv_font_montserrat_12, 0xFFFFFF);
  lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, 0);
  return cell;
}

static lv_obj_t* phBuildHome() {
  phCur = PH_HOME;
  lv_obj_t* s = phScreen(PC_WALL1, PC_WALL2);
  lv_obj_t* g = phGlass(s, 10, 26, 300, 52, 18);
  hmHost = phLbl(g, "-", &lv_font_montserrat_14, 0xFFFFFF);
  lv_obj_set_size(hmHost, 90, 18);                                    // fixed height so long host names are cut with ".." instead of wrapping
  lv_label_set_long_mode(hmHost, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(hmHost, 14, 8);
  hmState = phLbl(g, "-", &lv_font_montserrat_12, PC_GRN);
  lv_obj_set_pos(hmState, 14, 30);
  static const char* cap[3] = {"CPU", "RAM", "TEMP"};
  for (int i = 0; i < 3; i++) {
    int x = 130 + i * 56;
    lv_obj_t* sep = phBox(g, x - 9, 10, 1, 30, 0xFFFFFF, 0, 60);
    (void)sep;
    hmV[i] = phLbl(g, "-", &lv_font_montserrat_16, 0xFFFFFF);
    lv_obj_set_pos(hmV[i], x, 8);
    lv_obj_t* c = phLbl(g, cap[i], &lv_font_montserrat_12, 0xC7D2FE);
    lv_obj_set_pos(c, x, 31);
  }
  phRise(g, 0);
  struct Ic { const char* n; uint32_t a, b; int k; const char* sym; PhScr t; };
  static const Ic ic[10] = {
    {"Monitor", 0x38BDF8, 0x2563EB, GL_SYM,   LV_SYMBOL_EYE_OPEN, PH_MON},
    {"CPU",     0xFB923C, 0xEA580C, GL_SYM,   LV_SYMBOL_CHARGE,   PH_CPU},
    {"Memory",  0xA78BFA, 0x6D28D9, GL_SYM,   LV_SYMBOL_SD_CARD,  PH_MEM},
    {"Network", 0x34D399, 0x059669, GL_SYM,   LV_SYMBOL_WIFI,     PH_NET},
    {"Tasks",   0xFBBF24, 0xD97706, GL_SYM,   LV_SYMBOL_LIST,     PH_TASKS},
    {"Remote",  0xF87171, 0xDC2626, GL_SYM,   LV_SYMBOL_KEYBOARD, PH_RMT},
    {"Clock",   0xF472B6, 0xDB2777, GL_CLOCK, "",                 PH_CLK},
    {"Gallery", 0xC084FC, 0x9333EA, GL_SYM,   LV_SYMBOL_IMAGE,    PH_GAL},
    {"Settings",0x94A3B8, 0x475569, GL_SYM,   LV_SYMBOL_SETTINGS, PH_SET},
    {"Info",    0x2DD4BF, 0x0F766E, GL_INFO,  "",                 PH_INFO},
  };
  for (int i = 0; i < 10; i++) {
    lv_obj_t* c = phIcon(s, i % 5, i / 5, ic[i].n, ic[i].a, ic[i].b, ic[i].k, ic[i].sym, ic[i].t);
    phRise(c, 60 + i * 35);
  }
  phHomeUpdate();
  phChrome(true, false);
  return s;
}

// =====================================================================
//  APP SCAFFOLD
// =====================================================================
static void phBackEvt(lv_event_t*) { phHome(); }
static lv_obj_t* phAppBase(const char* title, uint32_t accent, lv_obj_t** body) {
  lv_obj_t* s = phScreen(PC_APPBG);
  lv_obj_t* b = phBox(s, 8, 24, 30, 28, PC_CARD2, 14);
  phPressable(b);
  lv_obj_set_ext_click_area(b, 8);
  lv_obj_add_event_cb(b, phBackEvt, LV_EVENT_CLICKED, NULL);
  lv_obj_center(phLbl(b, LV_SYMBOL_LEFT, &lv_font_montserrat_14, PC_TX));
  lv_obj_t* t = phLbl(s, title, &lv_font_montserrat_16, PC_TX);
  lv_obj_set_pos(t, 48, 29);
  phBox(s, 48 + 0, 49, 22, 3, accent, 1);
  lv_obj_t* bd = phBox(s, 0, 56, LV_HOR, 170, 0x000000, 0, 0);
  *body = bd;
  phChrome(true, true);
  return s;
}
static lv_obj_t* phChip(lv_obj_t* p, int x, int y, int w, int h, const char* cap, lv_obj_t** val, uint32_t vc = PC_TX) {
  lv_obj_t* c = phCard(p, x, y, w, h);
  lv_obj_t* l = phLbl(c, cap, &lv_font_montserrat_12, PC_MUT);
  lv_obj_align(l, LV_ALIGN_TOP_LEFT, 10, 6);
  *val = phLbl(c, "-", &lv_font_montserrat_14, vc);
  lv_obj_align(*val, LV_ALIGN_BOTTOM_LEFT, 10, -6);
  return c;
}
static lv_obj_t* phChart(lv_obj_t* p, int x, int y, int w, int h, lv_chart_series_t** s1, uint32_t c1, lv_chart_series_t** s2, uint32_t c2, int n) {
  lv_obj_t* c = lv_chart_create(p);
  lv_obj_set_pos(c, x, y);
  lv_obj_set_size(c, w, h);
  lv_obj_set_style_bg_opa(c, 0, 0);
  lv_obj_set_style_border_width(c, 0, 0);
  lv_obj_set_style_pad_all(c, 0, 0);
  lv_chart_set_type(c, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(c, n);
  lv_chart_set_range(c, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_set_div_line_count(c, 3, 0);
  lv_obj_set_style_line_color(c, K(0x1F2735), LV_PART_MAIN);
  lv_obj_set_style_size(c, 0, LV_PART_INDICATOR);
  lv_obj_set_style_line_width(c, 2, LV_PART_ITEMS);
  *s1 = lv_chart_add_series(c, K(c1), LV_CHART_AXIS_PRIMARY_Y);
  *s2 = lv_chart_add_series(c, K(c2), LV_CHART_AXIS_PRIMARY_Y);
  return c;
}
static lv_obj_t* phBar(lv_obj_t* p, int w, uint32_t col) {
  lv_obj_t* b = lv_bar_create(p);
  lv_obj_set_size(b, w, 8);
  lv_bar_set_range(b, 0, 100);
  lv_obj_set_style_radius(b, 4, 0);
  lv_obj_set_style_radius(b, 4, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(b, K(PC_CARD2), LV_PART_MAIN);
  lv_obj_set_style_bg_color(b, K(col), LV_PART_INDICATOR);
  lv_obj_set_style_anim_time(b, 500, 0);
  return b;
}
static void phBarSet(lv_obj_t* b, float v) {
  lv_bar_set_value(b, constrain((int)(v + 0.5f), 0, 100), LV_ANIM_ON);
  lv_obj_set_style_bg_color(b, K(phLoadCol(v)), LV_PART_INDICATOR);
}
static void phEnter(lv_obj_t* body) {                                  // staggered entrance for the cards of an app
  uint32_t n = lv_obj_get_child_cnt(body);
  for (uint32_t i = 0; i < n; i++) phRise(lv_obj_get_child(body, i), i * 50);
}

// =====================================================================
//  APP: MONITOR (three gauges + chips)
// =====================================================================
static lv_obj_t *moArc[3], *moV[3], *moS[3], *moTemp, *moNet;
static lv_obj_t* phBuildMon() {
  lv_obj_t *bd, *s = phAppBase("Monitor", PC_CY, &bd);
  static const char* cap[3] = {"CPU", "RAM", "GPU"};
  static const uint32_t col[3] = {PC_CY, PC_VIO, PC_PNK};
  for (int i = 0; i < 3; i++) {
    lv_obj_t* c = phCard(bd, 8 + i * 104, 0, 96, 116);
    lv_obj_t* a = lv_arc_create(c);
    moArc[i] = a;
    lv_obj_set_size(a, 72, 72);
    lv_obj_align(a, LV_ALIGN_TOP_MID, 0, 8);
    lv_arc_set_rotation(a, 135);
    lv_arc_set_bg_angles(a, 0, 270);
    lv_arc_set_range(a, 0, 100);
    lv_arc_set_value(a, 0);
    lv_obj_remove_style(a, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(a, 8, 0);
    lv_obj_set_style_arc_width(a, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, K(PC_CARD2), 0);
    lv_obj_set_style_arc_color(a, K(col[i]), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
    moV[i] = phLbl(c, "-", &lv_font_montserrat_16, PC_TX);
    lv_obj_align(moV[i], LV_ALIGN_TOP_MID, 0, 33);
    lv_obj_t* l = phLbl(c, cap[i], &lv_font_montserrat_12, PC_MUT);
    lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -22);
    moS[i] = phLbl(c, "-", &lv_font_montserrat_12, PC_MUT);
    lv_obj_align(moS[i], LV_ALIGN_BOTTOM_MID, 0, -6);
  }
  phChip(bd, 8, 122, 150, 46, "TEMPERATURE", &moTemp);
  phChip(bd, 166, 122, 146, 46, "NETWORK", &moNet);
  phEnter(bd);
  return s;
}
static void phUpdMon() {
  char b[32];
  float v[3] = {S.cpu, S.ram, S.gpu >= 0 ? S.gpu : 0};
  for (int i = 0; i < 3; i++) {
    phArcTo(moArc[i], constrain((int)(v[i] + 0.5f), 0, 100));
    snprintf(b, sizeof(b), "%.0f%%", v[i]); phSet(moV[i], b);
  }
  snprintf(b, sizeof(b), "%.0f MHz", S.freq);                       phSet(moS[0], b);
  snprintf(b, sizeof(b), "%.1f/%.0f G", S.ramUsed, S.ramTotal);     phSet(moS[1], b);
  if (S.gpu >= 0) snprintf(b, sizeof(b), "%.1f/%.0f G", S.gpuMemUsed, S.gpuMemTotal); else strcpy(b, "no GPU");
  phSet(moS[2], b);
  if (S.temp > 0 && S.gpuTemp > 0) snprintf(b, sizeof(b), "C %.0f   G %.0f", S.temp, S.gpuTemp);
  else if (S.temp > 0) snprintf(b, sizeof(b), "CPU %.0f C", S.temp);
  else if (S.gpuTemp > 0) snprintf(b, sizeof(b), "GPU %.0f C", S.gpuTemp);
  else strcpy(b, "-");
  phSet(moTemp, b);
  char d1[12], u1[12];
  phRateShort(d1, sizeof(d1), S.down); phRateShort(u1, sizeof(u1), S.up);
  snprintf(b, sizeof(b), LV_SYMBOL_DOWN " %s  " LV_SYMBOL_UP " %s", d1, u1);
  phSet(moNet, b);
}

// =====================================================================
//  APP: CPU (history chart + stats)
// =====================================================================
#define PH_CH_N 60
static lv_obj_t *cpChart, *cpV[3];
static lv_chart_series_t *cpS1, *cpS2;
static lv_obj_t* phBuildCpu() {
  lv_obj_t *bd, *s = phAppBase("CPU", PC_ORG, &bd);
  lv_obj_t* c = phCard(bd, 8, 0, 304, 108);
  lv_obj_t* a = phLbl(c, "CPU", &lv_font_montserrat_12, PC_CY);  lv_obj_set_pos(a, 12, 6);
  lv_obj_t* b = phLbl(c, "GPU", &lv_font_montserrat_12, PC_VIO); lv_obj_set_pos(b, 48, 6);
  cpChart = phChart(c, 8, 22, 288, 78, &cpS1, PC_CY, &cpS2, PC_VIO, PH_CH_N);
  for (int i = 0; i < PH_CH_N; i++) {
    lv_chart_set_next_value(cpChart, cpS1, (lv_coord_t)cpuHist[HN - PH_CH_N + i]);
    lv_chart_set_next_value(cpChart, cpS2, (lv_coord_t)gpuHist[HN - PH_CH_N + i]);
  }
  static const char* cap[3] = {"FREQUENCY", "TEMP", "PROCESSES"};
  for (int i = 0; i < 3; i++) phChip(bd, 8 + i * 104, 114, 96, 54, cap[i], &cpV[i]);
  phEnter(bd);
  return s;
}
static void phUpdCpu() {
  char b[24];
  lv_chart_set_next_value(cpChart, cpS1, (lv_coord_t)constrain((int)S.cpu, 0, 100));
  lv_chart_set_next_value(cpChart, cpS2, (lv_coord_t)(S.gpu >= 0 ? constrain((int)S.gpu, 0, 100) : 0));
  snprintf(b, sizeof(b), "%.0f MHz", S.freq);                phSet(cpV[0], b);
  if (S.temp > 0) snprintf(b, sizeof(b), "%.0f C", S.temp); else strcpy(b, "-");
  phSet(cpV[1], b);
  if (S.temp > 0) lv_obj_set_style_text_color(cpV[1], K(S.temp >= 85 ? PC_RED : (S.temp >= 70 ? PC_ORG : PC_GRN)), 0);
  snprintf(b, sizeof(b), "%u", (unsigned)S.nproc);           phSet(cpV[2], b);
}

// =====================================================================
//  APP: MEMORY (RAM / swap / disk bars)
// =====================================================================
static lv_obj_t *meBar[3], *meV[3], *meSub[3], *meIo;
static lv_obj_t* phBuildMem() {
  lv_obj_t *bd, *s = phAppBase("Memory", PC_VIO, &bd);
  static const char* cap[3] = {"RAM", "SWAP", "DISK"};
  for (int i = 0; i < 3; i++) {
    lv_obj_t* c = phCard(bd, 8, i * 58, 304, 54);
    lv_obj_t* l = phLbl(c, cap[i], &lv_font_montserrat_12, PC_MUT);
    lv_obj_set_pos(l, 12, 5);
    meV[i] = phLbl(c, "-", &lv_font_montserrat_16, PC_TX);
    lv_obj_align(meV[i], LV_ALIGN_TOP_RIGHT, -12, 3);
    meBar[i] = phBar(c, 280, PC_GRN);
    lv_obj_set_pos(meBar[i], 12, 24);
    meSub[i] = phLbl(c, "-", &lv_font_montserrat_12, PC_MUT);
    lv_obj_set_pos(meSub[i], 12, 35);
  }
  meIo = phLbl(lv_obj_get_child(bd, 2), "-", &lv_font_montserrat_12, PC_CY);      // disk read / write speed, right side of the DISK row
  lv_obj_set_pos(meIo, 150, 35);
  lv_obj_set_width(meIo, 142);
  lv_obj_set_style_text_align(meIo, LV_TEXT_ALIGN_RIGHT, 0);
  phEnter(bd);
  return s;
}
static void phUpdMem() {
  char b[48];
  snprintf(b, sizeof(b), "%.0f%%", S.ram);  phSet(meV[0], b);
  snprintf(b, sizeof(b), "%.1f of %.1f GB used", S.ramUsed, S.ramTotal); phSet(meSub[0], b);
  phBarSet(meBar[0], S.ram);
  snprintf(b, sizeof(b), "%.0f%%", S.swap); phSet(meV[1], b);
  phSet(meSub[1], "page file / swap");
  phBarSet(meBar[1], S.swap);
  snprintf(b, sizeof(b), "%.0f%%", S.disk); phSet(meV[2], b);
  if (S.nDrives > 0) snprintf(b, sizeof(b), "%s %.0f/%.0f GB", S.drives[0].n, S.drives[0].u, S.drives[0].t);
  else strcpy(b, "system drive");
  phSet(meSub[2], b);
  snprintf(b, sizeof(b), "R %.1f  W %.1f MB/s", S.dr, S.dw); phSet(meIo, b);
  phBarSet(meBar[2], S.disk);
}

// =====================================================================
//  APP: NETWORK
// =====================================================================
static lv_obj_t *neChart, *neV[3], *neTot;
static lv_chart_series_t *neS1, *neS2;
static lv_obj_t* phBuildNet() {
  lv_obj_t *bd, *s = phAppBase("Network", PC_GRN, &bd);
  lv_obj_t* c = phCard(bd, 8, 0, 304, 108);
  lv_obj_t* a = phLbl(c, "DOWN", &lv_font_montserrat_12, PC_CY);  lv_obj_set_pos(a, 12, 6);
  lv_obj_t* b = phLbl(c, "UP", &lv_font_montserrat_12, PC_VIO);   lv_obj_set_pos(b, 62, 6);
  neTot = phLbl(c, "-", &lv_font_montserrat_12, PC_MUT);
  lv_obj_align(neTot, LV_ALIGN_TOP_RIGHT, -12, 6);
  neChart = phChart(c, 8, 22, 288, 78, &neS1, PC_CY, &neS2, PC_VIO, PH_CH_N);
  float pk = 200;
  for (int i = 0; i < PH_CH_N; i++) pk = max(pk, max(downHist[HN - PH_CH_N + i], upHist[HN - PH_CH_N + i]));
  phNetPeak = pk;
  lv_chart_set_range(neChart, LV_CHART_AXIS_PRIMARY_Y, 0, (lv_coord_t)pk);
  for (int i = 0; i < PH_CH_N; i++) {
    lv_chart_set_next_value(neChart, neS1, (lv_coord_t)downHist[HN - PH_CH_N + i]);
    lv_chart_set_next_value(neChart, neS2, (lv_coord_t)upHist[HN - PH_CH_N + i]);
  }
  static const char* cap[3] = {"DOWNLOAD", "UPLOAD", "PING"};
  for (int i = 0; i < 3; i++) phChip(bd, 8 + i * 104, 114, 96, 54, cap[i], &neV[i]);
  phEnter(bd);
  return s;
}
static void phUpdNet() {
  char b[40];
  phNetPeak = max(phNetPeak * 0.97f, max(max(S.down, S.up), 200.0f));
  lv_chart_set_range(neChart, LV_CHART_AXIS_PRIMARY_Y, 0, (lv_coord_t)phNetPeak);
  lv_chart_set_next_value(neChart, neS1, (lv_coord_t)S.down);
  lv_chart_set_next_value(neChart, neS2, (lv_coord_t)S.up);
  phSet(neV[0], fmtRate(S.down).c_str());
  phSet(neV[1], fmtRate(S.up).c_str());
  if (S.ping >= 0) { snprintf(b, sizeof(b), "%.0f ms", S.ping);
    lv_obj_set_style_text_color(neV[2], K(S.ping < 40 ? PC_GRN : (S.ping < 100 ? PC_ORG : PC_RED)), 0); }
  else { strcpy(b, "n/a"); lv_obj_set_style_text_color(neV[2], K(PC_MUT), 0); }
  phSet(neV[2], b);
  snprintf(b, sizeof(b), "%.1f / %.1f GB", S.rxGb, S.txGb);
  phSet(neTot, b);
}

// =====================================================================
//  APP: CLOCK (analog + digital)
// =====================================================================
static lv_obj_t *ckMeter, *ckTime, *ckSec, *ckDate, *ckSync;
static lv_meter_indicator_t *ckH, *ckM, *ckS;
static lv_obj_t* phBuildClk() {
  lv_obj_t *bd, *s = phAppBase("Clock", PC_PNK, &bd);
  lv_obj_t* m = lv_meter_create(bd);
  ckMeter = m;
  lv_obj_set_size(m, 162, 162);
  lv_obj_set_pos(m, 8, 2);
  lv_obj_set_style_bg_color(m, K(PC_CARD), 0);
  lv_obj_set_style_bg_opa(m, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(m, 4, 0);
  lv_obj_set_style_border_color(m, K(PC_CARD2), 0);
  lv_obj_set_style_text_opa(m, LV_OPA_TRANSP, LV_PART_TICKS);          // hide numbers, keep ticks
  lv_obj_set_style_bg_color(m, K(PC_TX), LV_PART_INDICATOR);
  lv_obj_set_style_pad_all(m, 4, LV_PART_INDICATOR);
  lv_meter_scale_t* sc = lv_meter_add_scale(m);
  lv_meter_set_scale_ticks(m, sc, 61, 1, 4, K(0x39445A));
  lv_meter_set_scale_major_ticks(m, sc, 5, 2, 8, K(PC_TX), 10);
  lv_meter_set_scale_range(m, sc, 0, 60, 360, 270);
  ckH = lv_meter_add_needle_line(m, sc, 5, K(PC_TX), -42);
  ckM = lv_meter_add_needle_line(m, sc, 3, K(PC_TX), -20);
  ckS = lv_meter_add_needle_line(m, sc, 2, K(PC_PNK), -8);
  ckTime = phLbl(bd, "--:--", &lv_font_montserrat_48, PC_TX);
  lv_obj_set_pos(ckTime, 178, 18);
  ckSec = phLbl(bd, "--", &lv_font_montserrat_20, PC_PNK);
  lv_obj_set_pos(ckSec, 180, 76);
  ckDate = phLbl(bd, "-", &lv_font_montserrat_14, PC_MUT);
  lv_obj_set_pos(ckDate, 180, 108);
  ckSync = phLbl(bd, "-", &lv_font_montserrat_12, PC_MUT);
  lv_obj_set_pos(ckSync, 180, 132);
  phEnter(bd);
  return s;
}
static void phUpdClk() {
  struct tm t;
  char b[40];
  bool ok = phNow(t);
  if (!ok) { memset(&t, 0, sizeof(t)); t.tm_hour = 10; t.tm_min = 10; }
  lv_meter_set_indicator_value(ckMeter, ckH, (t.tm_hour % 12) * 5 + t.tm_min / 12);
  lv_meter_set_indicator_value(ckMeter, ckM, t.tm_min);
  lv_meter_set_indicator_value(ckMeter, ckS, t.tm_sec);
  if (ok) {
    snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min); phSet(ckTime, b);
    snprintf(b, sizeof(b), "%02d", t.tm_sec);                 phSet(ckSec, b);
    strftime(b, sizeof(b), "%a, %d %b %Y", &t);               phSet(ckDate, b);
    phSet(ckSync, "synced with NTP");
  } else {
    phSet(ckTime, S.clk); phSet(ckSec, "");
    phSet(ckDate, "no time yet"); phSet(ckSync, "waiting for Wi-Fi / NTP");
  }
}

// =====================================================================
//  APP: GALLERY (videos, GIFs and photos from the SD card; tap = play full screen, tap again = back)
//  RAM: only 4 list rows exist at a time (paged), the file-name list is freed when the app closes.
// =====================================================================
#define GA_ROWS 4
static std::vector<String> gaNames;
static int gaFilter = 0, gaPage = 0;                                   // filter: 0 all, 1 videos, 2 photos + GIFs
static lv_obj_t *gaRow[GA_ROWS], *gaIco[GA_ROWS], *gaTxt[GA_ROWS], *gaChip[3], *gaChipL[3], *gaPg;
static lv_timer_t* gaT = nullptr;
static bool gaIsVideo(const String& n) { int k = sdKindOf(n); return k == 1 || k == 5; }
static void gaLoad() {
  std::vector<String> all;
  sdListNames(all);
  gaNames.clear();
  for (size_t i = 0; i < all.size() && gaNames.size() < 120; i++) {
    bool v = gaIsVideo(all[i]);
    if (gaFilter == 0 || (gaFilter == 1 && v) || (gaFilter == 2 && !v)) gaNames.push_back(all[i]);
  }
  gaNames.shrink_to_fit();
}
static void gaFill() {
  int n = (int)gaNames.size(), pages = n ? (n + GA_ROWS - 1) / GA_ROWS : 1;
  if (gaPage >= pages) gaPage = 0;
  char b[12];
  snprintf(b, sizeof(b), "%d/%d", gaPage + 1, pages);
  phSet(gaPg, b);
  for (int i = 0; i < 3; i++) {
    lv_obj_set_style_bg_color(gaChip[i], K(i == gaFilter ? PC_CY : PC_CARD2), 0);
    lv_obj_set_style_text_color(gaChipL[i], K(i == gaFilter ? 0x06202A : PC_TX), 0);
  }
  for (int i = 0; i < GA_ROWS; i++) {
    int idx = gaPage * GA_ROWS + i;
    if (idx < n) {
      int k = sdKindOf(gaNames[idx]);
      bool v = (k == 1 || k == 5);
      phSet(gaIco[i], v ? LV_SYMBOL_VIDEO : (k == 3 ? LV_SYMBOL_PLAY : LV_SYMBOL_IMAGE));
      lv_obj_set_style_text_color(gaIco[i], K(v ? PC_PNK : (k == 3 ? PC_VIO : PC_CY)), 0);
      phSet(gaTxt[i], gaNames[idx].c_str());
      lv_obj_set_style_text_color(gaTxt[i], K(PC_TX), 0);
      lv_obj_clear_flag(gaRow[i], LV_OBJ_FLAG_HIDDEN);
    } else if (i == 0) {
      phSet(gaIco[0], LV_SYMBOL_WARNING);
      lv_obj_set_style_text_color(gaIco[0], K(PC_ORG), 0);
      phSet(gaTxt[0], sdOk ? "No files in /videos /gifs /images" : "SD card not ready");
      lv_obj_set_style_text_color(gaTxt[0], K(PC_MUT), 0);
      lv_obj_clear_flag(gaRow[0], LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(gaRow[i], LV_OBJ_FLAG_HIDDEN);
  }
}
static void gaErrReset(lv_timer_t*) { gaT = nullptr; gaFill(); }
static void phMediaLeave();
static void gaPlayFailed() {
  if (curPage == P_MEDIA) phMediaLeave();                              // enterPage() had already switched to the player and drawn its own message
  lv_obj_invalidate(lv_scr_act());
  lv_obj_invalidate(lv_layer_top());
}
static void gaRowEvt(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  int idx = gaPage * GA_ROWS + i;
  if (idx >= (int)gaNames.size()) return;
  plActive = false;
  phMediaArmed = false;
  tft.waitDMA();                                                       // LVGL's last flush must be finished before the player draws
  String nm = gaNames[idx];
  if (!sdPlayFile(nm)) {                                               // show the reason in the row for a moment
    gaPlayFailed();
    phSet(gaTxt[i], lastErr.c_str());
    lv_obj_set_style_text_color(gaTxt[i], K(PC_RED), 0);
    if (gaT) lv_timer_del(gaT);
    gaT = lv_timer_create(gaErrReset, 2500, NULL);
    lv_timer_set_repeat_count(gaT, 1);
  }
}
static void gaAllEvt(lv_event_t*) {                                    // slideshow / playlist of everything on the card
  if (gaNames.empty()) return;
  plIdx = -1; plActive = true; phMediaArmed = false;
  tft.waitDMA();
  sdPlayNext();
  if (!plActive) gaPlayFailed();                                       // nothing playable
}
static void gaChipEvt(lv_event_t* e) {
  gaFilter = (int)(intptr_t)lv_event_get_user_data(e);
  gaPage = 0;
  gaLoad();
  gaFill();
}
static void gaNavEvt(lv_event_t* e) {
  int d = (int)(intptr_t)lv_event_get_user_data(e);
  int pages = gaNames.empty() ? 1 : ((int)gaNames.size() + GA_ROWS - 1) / GA_ROWS;
  gaPage = (gaPage + d + pages) % pages;
  gaFill();
}
static lv_obj_t* gaBtn(lv_obj_t* p, int x, int w, const char* t, lv_event_cb_t cb, int ud, lv_obj_t** lbl) {
  lv_obj_t* b = phBox(p, x, 2, w, 26, PC_CARD2, 13);
  phPressable(b);
  lv_obj_set_ext_click_area(b, 3);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void*)(intptr_t)ud);
  lv_obj_t* l = phLbl(b, t, &lv_font_montserrat_12, PC_TX);
  lv_obj_center(l);
  if (lbl) *lbl = l;
  return b;
}
static lv_obj_t* phBuildGal() {
  lv_obj_t *bd, *s = phAppBase("Gallery", PC_PNK, &bd);
  static const char* cn[3] = {"All", "Video", "Photo"};
  static const int cx[3] = {8, 52, 106}, cw[3] = {40, 50, 50};
  for (int i = 0; i < 3; i++) gaChip[i] = gaBtn(bd, cx[i], cw[i], cn[i], gaChipEvt, i, &gaChipL[i]);
  gaBtn(bd, 162, 62, LV_SYMBOL_PLAY " all", gaAllEvt, 0, nullptr);
  gaBtn(bd, 230, 28, LV_SYMBOL_LEFT, gaNavEvt, -1, nullptr);
  gaBtn(bd, 288, 28, LV_SYMBOL_RIGHT, gaNavEvt, 1, nullptr);
  gaPg = phLbl(bd, "1/1", &lv_font_montserrat_12, PC_MUT);
  lv_obj_set_width(gaPg, 30);
  lv_obj_set_style_text_align(gaPg, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(gaPg, 258, 8);
  for (int i = 0; i < GA_ROWS; i++) {
    gaRow[i] = phBox(bd, 8, 36 + i * 30, 304, 28, PC_CARD, 10);
    lv_obj_set_style_border_width(gaRow[i], 1, 0);
    lv_obj_set_style_border_color(gaRow[i], K(0x232C3D), 0);
    phPressable(gaRow[i]);
    lv_obj_add_event_cb(gaRow[i], gaRowEvt, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    gaIco[i] = phLbl(gaRow[i], "", &lv_font_montserrat_14, PC_CY);
    lv_obj_set_pos(gaIco[i], 10, 5);
    gaTxt[i] = phLbl(gaRow[i], "", &lv_font_montserrat_14, PC_TX);
    lv_obj_set_width(gaTxt[i], 262);
    lv_label_set_long_mode(gaTxt[i], LV_LABEL_LONG_DOT);
    lv_obj_set_pos(gaTxt[i], 34, 5);
  }
  gaPage = 0;
  gaLoad();
  gaFill();
  return s;
}
static void gaLeave() {
  if (gaT) { lv_timer_del(gaT); gaT = nullptr; }
  std::vector<String>().swap(gaNames);                                 // give the memory back
}

// =====================================================================
//  APP: SETTINGS
// =====================================================================
static lv_obj_t *seBrV, *seSlider, *seChip[5], *seSw;
static const uint8_t phIdleOpts[5] = {0, 1, 5, 10, 30};
static void phPrefSave() { Preferences p; if (p.begin("phui", false)) { p.putBool("lock", phLockOnWake); p.end(); } }
static void phPrefLoad() { Preferences p; if (p.begin("phui", true)) { phLockOnWake = p.getBool("lock", true); p.end(); } }
static void phSetSlider(lv_event_t* e) {
  lv_obj_t* s = lv_event_get_target(e);
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    int v = lv_slider_get_value(s);
    cfg.bright = (uint8_t)v; gLastAct = millis(); applyBrightness();
    char b[8]; snprintf(b, sizeof(b), "%d%%", v * 100 / 255); phSet(seBrV, b);
  } else if (lv_event_get_code(e) == LV_EVENT_RELEASED) saveCfg();
}
static void phChipStyle() {
  for (int i = 0; i < 5; i++) {
    bool on = gIdleMin == phIdleOpts[i];
    lv_obj_set_style_bg_color(seChip[i], K(on ? PC_CY : PC_CARD2), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(seChip[i], 0), K(on ? 0x06202B : PC_TX), 0);
  }
}
static void phSetChip(lv_event_t* e) {
  gIdleMin = phIdleOpts[(int)(intptr_t)lv_event_get_user_data(e)];
  idleSave(); gLastAct = millis();
  phChipStyle();
}
static void phSetSwitch(lv_event_t* e) { phLockOnWake = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED); phPrefSave(); }
static lv_obj_t* phBuildSet() {
  lv_obj_t *bd, *s = phAppBase("Settings", 0x94A3B8, &bd);
  lv_obj_t* c = phCard(bd, 8, 0, 304, 54);
  lv_obj_t* l = phLbl(c, "Brightness", &lv_font_montserrat_12, PC_MUT);
  lv_obj_set_pos(l, 12, 6);
  seBrV = phLbl(c, "-", &lv_font_montserrat_12, PC_TX);
  lv_obj_align(seBrV, LV_ALIGN_TOP_RIGHT, -12, 6);
  char b[8]; snprintf(b, sizeof(b), "%d%%", cfg.bright * 100 / 255); phSet(seBrV, b);
  seSlider = lv_slider_create(c);
  lv_obj_set_size(seSlider, 278, 10);
  lv_obj_align(seSlider, LV_ALIGN_BOTTOM_MID, 0, -12);
  lv_slider_set_range(seSlider, 5, 255);
  lv_slider_set_value(seSlider, cfg.bright, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(seSlider, K(PC_CARD2), LV_PART_MAIN);
  lv_obj_set_style_bg_color(seSlider, K(PC_YEL), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(seSlider, K(0xFFFFFF), LV_PART_KNOB);
  lv_obj_set_style_pad_all(seSlider, 4, LV_PART_KNOB);
  lv_obj_add_flag(seSlider, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_set_ext_click_area(seSlider, 12);
  lv_obj_add_event_cb(seSlider, phSetSlider, LV_EVENT_ALL, NULL);

  lv_obj_t* c2 = phCard(bd, 8, 60, 304, 62);
  lv_obj_t* l2 = phLbl(c2, "Screen off after", &lv_font_montserrat_12, PC_MUT);
  lv_obj_set_pos(l2, 12, 6);
  static const char* nm[5] = {"Never", "1 min", "5 min", "10 min", "30 min"};
  for (int i = 0; i < 5; i++) {
    lv_obj_t* ch = phBox(c2, 10 + i * 57, 26, 52, 28, PC_CARD2, 14);
    seChip[i] = ch;
    phPressable(ch);
    lv_obj_add_event_cb(ch, phSetChip, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_center(phLbl(ch, nm[i], &lv_font_montserrat_12, PC_TX));
  }
  // label colours live on the child; restyle helper only touches the button, so set text colour on the child too
  phChipStyle();

  lv_obj_t* c3 = phCard(bd, 8, 128, 304, 40);
  lv_obj_t* l3 = phLbl(c3, "Lock screen after sleep", &lv_font_montserrat_14, PC_TX);
  lv_obj_align(l3, LV_ALIGN_LEFT_MID, 12, 0);
  seSw = lv_switch_create(c3);
  lv_obj_set_size(seSw, 42, 22);
  lv_obj_align(seSw, LV_ALIGN_RIGHT_MID, -12, 0);
  lv_obj_set_style_bg_color(seSw, K(PC_CARD2), LV_PART_MAIN);
  lv_obj_set_style_bg_color(seSw, K(PC_GRN), (lv_style_selector_t)(LV_PART_INDICATOR | LV_STATE_CHECKED));
  lv_obj_set_style_bg_color(seSw, K(0xFFFFFF), LV_PART_KNOB);
  if (phLockOnWake) lv_obj_add_state(seSw, LV_STATE_CHECKED);
  lv_obj_add_event_cb(seSw, phSetSwitch, LV_EVENT_VALUE_CHANGED, NULL);
  phEnter(bd);
  return s;
}

// =====================================================================
//  APP: INFO
// =====================================================================
static lv_obj_t* inV[7];
static lv_obj_t* phBuildInfo() {
  lv_obj_t *bd, *s = phAppBase("Info", 0x2DD4BF, &bd);
  static const char* Kc[7] = {"Network", "IP address", "PC agent", "Signal", "Free memory", "Uptime", "Last reset"};
  lv_obj_t* c = phCard(bd, 8, 0, 304, 168);
  for (int i = 0; i < 7; i++) {
    lv_obj_t* k = phLbl(c, Kc[i], &lv_font_montserrat_12, PC_MUT);
    lv_obj_set_pos(k, 12, 9 + i * 22);
    inV[i] = phLbl(c, "-", &lv_font_montserrat_12, PC_TX);
    lv_obj_align(inV[i], LV_ALIGN_TOP_RIGHT, -12, 9 + i * 22);
    if (i < 6) phBox(c, 12, 9 + i * 22 + 18, 280, 1, 0x232C3D, 0);
  }
  return s;
}
static void phUpdInfo() {
  char b[56];
  if (apMode && WiFi.status() != WL_CONNECTED) snprintf(b, sizeof(b), "SETUP AP (%d clients)", (int)WiFi.softAPgetStationNum());
  else snprintf(b, sizeof(b), "%s", cfg.ssid[0] ? cfg.ssid : "-");
  phSet(inV[0], b);
  phSet(inV[1], myIp().c_str());
  snprintf(b, sizeof(b), "%s:%u  %s", cfg.pcIp[0] ? cfg.pcIp : "-", (unsigned)cfg.pcPort, pcOnline ? "online" : "offline");
  phSet(inV[2], b);
  lv_obj_set_style_text_color(inV[2], K(pcOnline ? PC_GRN : PC_ORG), 0);
  if (WiFi.status() == WL_CONNECTED) snprintf(b, sizeof(b), "%d dBm", (int)WiFi.RSSI()); else strcpy(b, "-");
  phSet(inV[3], b);
  snprintf(b, sizeof(b), "%u KB (max %u KB)", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getMaxAllocHeap() / 1024));
  phSet(inV[4], b);
  uint32_t u = millis() / 1000;
  snprintf(b, sizeof(b), "%ud %02uh %02um", (unsigned)(u / 86400), (unsigned)((u / 3600) % 24), (unsigned)((u / 60) % 60));
  phSet(inV[5], b);
  phSet(inV[6], gResetWhy);
}

// =====================================================================
//  APP: TASKS (top 6 processes by CPU, active app, battery)
// =====================================================================
static lv_obj_t *tkCap, *tkApp, *tkBat, *tkBatBar, *tkN[6], *tkM[6], *tkC[6];
static lv_obj_t* phBuildTasks() {
  lv_obj_t *bd, *s = phAppBase("Tasks", PC_ORG, &bd);
  lv_obj_t* a = phCard(bd, 8, 0, 304, 40);
  tkCap = phLbl(a, "ACTIVE APP", &lv_font_montserrat_12, PC_MUT);
  lv_obj_set_pos(tkCap, 12, 4);
  tkApp = phLbl(a, "-", &lv_font_montserrat_14, PC_TX);
  lv_obj_set_width(tkApp, 190);
  lv_label_set_long_mode(tkApp, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(tkApp, 12, 19);
  tkBat = phLbl(a, "-", &lv_font_montserrat_14, PC_TX);
  lv_obj_align(tkBat, LV_ALIGN_TOP_RIGHT, -12, 3);
  tkBatBar = phBar(a, 74, PC_GRN);
  lv_obj_align(tkBatBar, LV_ALIGN_TOP_RIGHT, -12, 25);
  lv_obj_t* c = phCard(bd, 8, 44, 304, 118);
  for (int i = 0; i < 6; i++) {
    int y = 4 + i * 19;
    tkN[i] = phLbl(c, "", &lv_font_montserrat_12, PC_TX);
    lv_obj_set_width(tkN[i], 150); lv_label_set_long_mode(tkN[i], LV_LABEL_LONG_DOT);
    lv_obj_set_pos(tkN[i], 10, y);
    tkM[i] = phLbl(c, "", &lv_font_montserrat_12, PC_MUT);
    lv_obj_set_width(tkM[i], 64); lv_obj_set_style_text_align(tkM[i], LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(tkM[i], 164, y);
    tkC[i] = phLbl(c, "", &lv_font_montserrat_12, PC_GRN);
    lv_obj_set_width(tkC[i], 58); lv_obj_set_style_text_align(tkC[i], LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(tkC[i], 234, y);
  }
  phEnter(bd);
  return s;
}
static void phUpdTasks() {
  char b[40];
  bool hasW = S.win[0], hasA = S.wapp[0];
  if (hasA && hasW) snprintf(b, sizeof(b), "ACTIVE  %s", S.wapp); else strcpy(b, "ACTIVE APP");
  phSet(tkCap, b);
  phSet(tkApp, hasW ? S.win : (hasA ? S.wapp : "-"));
  if (S.bat >= 0) {
    snprintf(b, sizeof(b), "%d%% %s", (int)S.bat, S.plug ? "AC" : "BAT");
    phSet(tkBat, b);
    lv_obj_clear_flag(tkBatBar, LV_OBJ_FLAG_HIDDEN);
    lv_bar_set_value(tkBatBar, S.bat, LV_ANIM_ON);
    lv_obj_set_style_bg_color(tkBatBar, K(S.bat < 20 ? PC_RED : (S.bat < 50 ? PC_YEL : PC_GRN)), LV_PART_INDICATOR);
  } else { phSet(tkBat, "no battery"); lv_obj_add_flag(tkBatBar, LV_OBJ_FLAG_HIDDEN); }
  for (int i = 0; i < 6; i++) {
    if (i < S.nProcs) {
      const Proc& p = S.procs[i];
      phSet(tkN[i], p.n);
      if (p.m >= 1024) snprintf(b, sizeof(b), "%.1f GB", p.m / 1024.0f); else snprintf(b, sizeof(b), "%u MB", (unsigned)p.m);
      phSet(tkM[i], b);
      snprintf(b, sizeof(b), "%.1f%%", p.c);
      phSet(tkC[i], b);
      lv_obj_set_style_text_color(tkC[i], K(phLoadCol(p.c)), 0);
    } else { phSet(tkN[i], i == 0 ? "no process data" : ""); phSet(tkM[i], ""); phSet(tkC[i], ""); }
  }
}

// =====================================================================
//  APP: REMOTE (web-panel tabs + buttons -> pc_agent.py)
//  tap = send, danger buttons need a 2nd tap (TAP AGAIN), hold = repeat, swipe / < > = other tab, toast = Sent / error
//  RAM: everything is built when the app opens and freed when it closes; flash icons (2 KB each) live only for the visible tab.
// =====================================================================
static lv_obj_t *rmGrid, *rmName, *rmEmpty, *rmToast, *rmToastL;
static lv_timer_t* rmToastT = nullptr;
static uint16_t* rmIcoBuf[RMT_PER];
static lv_img_dsc_t rmIcoDsc[RMT_PER];
static lv_obj_t* rmBtnObj[RMT_PER];
static volatile int rmSendBusy = 0, rmSendRes = -1;
static TaskHandle_t rmTask = nullptr;               // ONE send task, created when the app opens (before the icons take RAM) and reused for every tap
static char rmSendId[64], rmSendLab[13];
static int rmConfIdx = -1;
static uint32_t rmConfUntil = 0, rmLeaveAt = 0;

static void rmToastHide(lv_timer_t*) { rmToastT = nullptr; if (rmToast) lv_obj_add_flag(rmToast, LV_OBJ_FLAG_HIDDEN); }
static void rmToastShow(const char* t, uint32_t col, uint32_t ms = 1400) {
  if (!rmToast) return;
  phSet(rmToastL, t);
  lv_obj_set_style_border_color(rmToast, K(col), 0);
  lv_obj_set_style_text_color(rmToastL, K(col), 0);
  lv_obj_center(rmToastL);
  lv_obj_clear_flag(rmToast, LV_OBJ_FLAG_HIDDEN);
  if (rmToastT) lv_timer_del(rmToastT);
  rmToastT = lv_timer_create(rmToastHide, ms, NULL);
  lv_timer_set_repeat_count(rmToastT, 1);
}
static void rmTaskFn(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    rmSendRes = remoteSend(rmSendId, rmSendLab);      // blocks up to ~1.5 s on the network: that is why it has its own task
    rmSendBusy = 0;
  }
}
static bool rmTaskStart() {
  if (rmTask) return true;
  return xTaskCreatePinnedToCore(rmTaskFn, "rmsend", 6144, NULL, 1, &rmTask, 0) == pdPASS;
}
static void rmConfClear() {
  if (rmConfIdx >= 0 && rmConfIdx < RMT_PER && rmBtnObj[rmConfIdx] && rmtTabs) {
    lv_obj_t* b = rmBtnObj[rmConfIdx];
    lv_obj_set_style_border_color(b, K(PC_RED), 0);
    lv_obj_t* l = lv_obj_get_child(b, -1);
    if (l) phSet(l, rmtTab[rmtCur].b[rmConfIdx].lab);
  }
  rmConfIdx = -1;
}
static void rmTap(int i, bool repeat) {
  if (!rmtTabs || i < 0 || i >= rmtTab[rmtCur].n) return;
  const RmtBtn& a = rmtTab[rmtCur].b[i];
  if (rmSendBusy) return;
  if (a.danger && !repeat) {
    if (!(rmConfIdx == i && millis() < rmConfUntil)) {                 // dangerous: first tap only arms the button
      rmConfClear();
      rmConfIdx = i; rmConfUntil = millis() + 3000;
      lv_obj_set_style_border_color(rmBtnObj[i], K(PC_YEL), 0);
      lv_obj_t* l = lv_obj_get_child(rmBtnObj[i], -1);
      if (l) phSet(l, "TAP AGAIN");
      return;
    }
    rmConfClear();
  }
  strlcpy(rmSendId, a.id, sizeof(rmSendId));
  strlcpy(rmSendLab, a.lab, sizeof(rmSendLab));
  if (!rmTaskStart()) { rmToastShow("Low RAM: reopen Remote", PC_RED, 2200); return; }
  rmSendBusy = 1; rmSendRes = -1;
  xTaskNotifyGive(rmTask);
}
static void rmBtnEvt(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_CLICKED) rmTap(i, false);
  else if (c == LV_EVENT_LONG_PRESSED_REPEAT && rmtTabs && i < rmtTab[rmtCur].n && rmtTab[rmtCur].b[i].rep) rmTap(i, true);
}
static void rmFreeIcons() {
  for (int i = 0; i < RMT_PER; i++) if (rmIcoBuf[i]) { free(rmIcoBuf[i]); rmIcoBuf[i] = nullptr; }
}
static void rmAnimX(void* o, int32_t v) { lv_obj_set_style_translate_x((lv_obj_t*)o, v, 0); }
static void rmBuild(int dir) {                                         // dir -1 / +1 = slide direction, 0 = none
  rmConfIdx = -1;
  lv_obj_clean(rmGrid);
  rmFreeIcons();
  for (int i = 0; i < RMT_PER; i++) rmBtnObj[i] = nullptr;
  if (rmtTabs <= 0) { phSet(rmName, "REMOTE"); lv_obj_clear_flag(rmEmpty, LV_OBJ_FLAG_HIDDEN); return; }
  lv_obj_add_flag(rmEmpty, LV_OBJ_FLAG_HIDDEN);
  if (rmtCur >= rmtTabs) rmtCur = rmtTabs - 1;
  if (rmtCur < 0) rmtCur = 0;
  char hb[24];
  snprintf(hb, sizeof(hb), "%s  %d/%d", rmtTab[rmtCur].name, rmtCur + 1, rmtTabs);
  phSet(rmName, hb);
  RmtTab& T = rmtTab[rmtCur];
  for (int i = 0; i < T.n && i < RMT_PER; i++) {
    const RmtBtn& a = T.b[i];
    lv_color_t col = phC565(RMT_COL[a.col & 7]);
    lv_obj_t* b = lv_btn_create(rmGrid);
    rmBtnObj[i] = b;
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 96, 56);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_bg_color(b, K(PC_CARD2), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_border_color(b, a.danger ? K(PC_RED) : col, 0);
    lv_obj_set_style_bg_color(b, col, LV_STATE_PRESSED);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(b, 1, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_event_cb(b, rmBtnEvt, LV_EVENT_ALL, (void*)(intptr_t)i);
    bool hasIco = false;
    if (a.ico) {                                                       // 32x32 RGB565 flash icon from the web panel
      uint16_t* buf = ESP.getMaxAllocHeap() > 18432 ? (uint16_t*)malloc(ICON_BYTES) : nullptr;     // never let icons eat the last RAM
      if (buf && iconRead(a.ico, buf)) {
        for (int k = 0; k < ICON_W * ICON_W; k++) if (!buf[k]) buf[k] = 0x07E0;      // transparent -> chroma key
        rmIcoBuf[i] = buf;
        rmIcoDsc[i].header.cf = LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED;
        rmIcoDsc[i].header.always_zero = 0;
        rmIcoDsc[i].header.w = ICON_W; rmIcoDsc[i].header.h = ICON_W;
        rmIcoDsc[i].data_size = ICON_BYTES;
        rmIcoDsc[i].data = (const uint8_t*)buf;
        lv_img_set_src(lv_img_create(b), &rmIcoDsc[i]);
        hasIco = true;
      } else if (buf) free(buf);
    }
    lv_obj_t* l = phLbl(b, a.lab, hasIco ? &lv_font_montserrat_12 : &lv_font_montserrat_14, PC_TX);
    lv_obj_set_width(l, 88);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  }
  lv_obj_scroll_to_y(rmGrid, 0, LV_ANIM_OFF);
  if (dir) {
    lv_anim_t an; lv_anim_init(&an);
    lv_anim_set_var(&an, rmGrid); lv_anim_set_exec_cb(&an, rmAnimX);
    lv_anim_set_values(&an, dir * 46, 0); lv_anim_set_time(&an, 200);
    lv_anim_set_path_cb(&an, lv_anim_path_ease_out); lv_anim_start(&an);
  }
}
static void rmStep(int d) {
  if (rmtTabs <= 1) return;
  rmtCur = (rmtCur + d + rmtTabs) % rmtTabs;
  rmtConfirmSlot = -1;
  rmBuild(d);
}
static void rmNavEvt(lv_event_t* e) { rmStep((int)(intptr_t)lv_event_get_user_data(e)); }
static void rmGestEvt(lv_event_t*) {
  lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (d == LV_DIR_LEFT) rmStep(1); else if (d == LV_DIR_RIGHT) rmStep(-1);
}
static lv_obj_t* rmNavBtn(lv_obj_t* p, int x, const char* sym, int dir) {
  lv_obj_t* b = phBox(p, x, 2, 46, 28, PC_CARD2, 14);
  phPressable(b);
  lv_obj_set_ext_click_area(b, 6);
  lv_obj_add_event_cb(b, rmNavEvt, LV_EVENT_CLICKED, (void*)(intptr_t)dir);
  lv_obj_center(phLbl(b, sym, &lv_font_montserrat_16, PC_CY));
  return b;
}
static lv_obj_t* phBuildRmt() {
  lv_obj_t *bd, *s = phAppBase("Remote", PC_RED, &bd);
  rmLeaveAt = 0;
  rmFreeIcons();
  rmTaskStart();                                                       // allocate the send task FIRST, while RAM is not fragmented
  rmNavBtn(bd, 8, LV_SYMBOL_LEFT, -1);
  rmNavBtn(bd, 266, LV_SYMBOL_RIGHT, 1);
  rmName = phLbl(bd, "REMOTE", &lv_font_montserrat_14, PC_TX);
  lv_obj_set_width(rmName, 200);
  lv_obj_set_style_text_align(rmName, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(rmName, 60, 8);
  rmGrid = lv_obj_create(bd);
  lv_obj_remove_style_all(rmGrid);
  lv_obj_set_pos(rmGrid, 0, 34);
  lv_obj_set_size(rmGrid, LV_HOR, 126);
  lv_obj_set_flex_flow(rmGrid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_all(rmGrid, 6, 0);
  lv_obj_set_style_pad_row(rmGrid, 6, 0);
  lv_obj_set_style_pad_column(rmGrid, 6, 0);
  lv_obj_set_scroll_dir(rmGrid, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(rmGrid, LV_SCROLLBAR_MODE_OFF);
  rmEmpty = phLbl(bd, "No tabs yet.\nAdd them in the web panel (Remote tab).", &lv_font_montserrat_12, PC_MUT);
  lv_obj_set_style_text_align(rmEmpty, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(rmEmpty, LV_ALIGN_CENTER, 0, 10);
  lv_obj_add_flag(rmEmpty, LV_OBJ_FLAG_HIDDEN);
  rmToast = phBox(s, 50, 184, 220, 28, 0x0F172A, 14);                  // toast: on the screen, hidden until a send finishes
  lv_obj_set_style_border_width(rmToast, 2, 0);
  lv_obj_set_style_border_color(rmToast, K(PC_GRN), 0);
  rmToastL = phLbl(rmToast, "", &lv_font_montserrat_14, PC_GRN);
  lv_obj_add_flag(rmToast, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(s, rmGestEvt, LV_EVENT_GESTURE, NULL);
  rmSendRes = -1;
  rmBuild(0);
  return s;
}
static void rmLeave() {                                                // called while the old screen is still sliding out
  if (rmToastT) { lv_timer_del(rmToastT); rmToastT = nullptr; }
  rmToast = nullptr;
  rmConfIdx = -1;
  rmLeaveAt = millis() ? millis() : 1;                                 // icon buffers are freed ~0.6 s later (after the slide animation)
}
static void rmTick() {
  if (phCur == PH_RMT) {
    if (gLvRmtDirty) { gLvRmtDirty = false; rmBuild(0); }              // the web panel saved new remote tabs
    if (!rmSendBusy && rmSendRes >= 0) {                               // answer of the background send task
      int r = rmSendRes; rmSendRes = -1;
      char t[28];
      if (r == 1) { snprintf(t, sizeof(t), "Sent  %s", rmSendLab); rmToastShow(t, PC_GRN); }
      else if (r == 2) rmToastShow("Error: wrong token", PC_RED, 2200);
      else rmToastShow("Error: PC not reachable", PC_RED, 2200);
    }
    if (rmConfIdx >= 0 && millis() > rmConfUntil) rmConfClear();
  } else {
    gLvRmtDirty = false;
    if (rmTask && !rmSendBusy) { vTaskDelete(rmTask); rmTask = nullptr; }   // free the 6 KB stack when the app is closed
    if (rmLeaveAt && millis() - rmLeaveAt > 600) { rmLeaveAt = 0; rmFreeIcons(); }
  }
}

// =====================================================================
//  NAVIGATION
// =====================================================================
static void phLeaveCur() {
  if (phCur == PH_GAL) gaLeave();
  if (phCur == PH_SET) saveCfg();
  if (phCur == PH_RMT) rmLeave();
}
static bool phHeapOk() {
  lv_mem_monitor_t m;
  lv_mem_monitor(&m);                                                  // LVGL has its own fixed pool: check IT, not only the system heap
  if (m.free_biggest_size < 7000) { Serial.printf("LOW LVGL RAM: not opening app (pool free=%u biggest=%u)\n", (unsigned)m.free_size, (unsigned)m.free_biggest_size); return false; }
  return ESP.getFreeHeap() >= 16000 && ESP.getMaxAllocHeap() >= 6144;
}
static void phOpen(PhScr id) {
  if (!phHeapOk()) { Serial.printf("LOW RAM: not opening app (free=%u maxblock=%u)\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap()); return; }
  phLeaveCur();
  phCur = id;
  lv_obj_t* s = nullptr;
  switch (id) {
    case PH_MON:   s = phBuildMon();   phUpdMon(); break;
    case PH_CPU:   s = phBuildCpu();   phUpdCpu(); break;
    case PH_MEM:   s = phBuildMem();   phUpdMem(); break;
    case PH_NET:   s = phBuildNet();   phUpdNet(); break;
    case PH_TASKS: s = phBuildTasks(); phUpdTasks(); break;
    case PH_RMT:   s = phBuildRmt();   break;
    case PH_CLK:   s = phBuildClk();   phUpdClk(); break;
    case PH_GAL:   s = phBuildGal();   break;
    case PH_SET:   s = phBuildSet();   break;
    case PH_INFO:  s = phBuildInfo();  phUpdInfo(); break;
    default: break;
  }
  if (s) phGo(s, LV_SCR_LOAD_ANIM_OVER_TOP, 240);
}
static void phHome() {
  phShadeSet(false);
  if (phCur == PH_HOME) return;
  bool fromLock = (phCur == PH_LOCK);
  phLeaveCur();
  lv_obj_t* s = phBuildHome();                                         // sets phCur = PH_HOME
  phGo(s, fromLock ? LV_SCR_LOAD_ANIM_MOVE_TOP : LV_SCR_LOAD_ANIM_OUT_BOTTOM, 260);
}

// =====================================================================
//  ENTRY POINTS used by main.cpp
// =====================================================================
static void phLiveUpdate(bool data) {
  switch (phCur) {
    case PH_HOME: phHomeUpdate(); break;
    case PH_MON:  if (data) phUpdMon(); break;
    case PH_CPU:  if (data) phUpdCpu(); break;
    case PH_MEM:  if (data) phUpdMem(); break;
    case PH_NET:  if (data) phUpdNet(); break;
    case PH_TASKS: if (data) phUpdTasks(); break;
    default: break;
  }
}

// ---- hooks used by the web panel (they run on the UI task, so LVGL calls are safe) ----
static const char* phScrName() {
  switch (phCur) {
    case PH_LOCK: return "lock";   case PH_HOME: return "home";   case PH_MON: return "mon";   case PH_CPU: return "cpu";
    case PH_MEM:  return "mem";    case PH_NET:  return "net";    case PH_CLK: return "clk";   case PH_GAL: return "gal";
    case PH_SET:  return "set";    case PH_INFO: return "info";   case PH_TASKS: return "tasks"; case PH_RMT: return "rmt";
    default: return "?";
  }
}
static void phMediaLeave() {                                           // stop whatever the full-screen player shows and return to the LVGL screen
  plActive = false;
  gifStop(); clipClose(); lightStop();
  curPage = P_OVERVIEW; pageStart = millis();
  cpuFor(P_OVERVIEW);
  phMediaArmed = true;
}
static void uiStepMedia(int dir) {                                     // previous / next file on the card (swipe in the player, web panel buttons)
  std::vector<String> v;
  sdListNames(v);
  if (v.empty()) { phMediaLeave(); return; }
  int cnt = (int)v.size(), cur = -1;
  for (int i = 0; i < cnt; i++) if (v[i] == liveName) cur = i;
  int n = cur < 0 ? (dir > 0 ? 0 : cnt - 1) : (cur + dir + cnt) % cnt;
  plActive = false;
  for (int t = 0; t < cnt; t++) {                                      // skip files that cannot be played
    if (sdPlayFile(v[n])) return;
    n = (n + dir + cnt) % cnt;
  }
  phMediaLeave();
}
static bool phWebCmd(const char* cmd, const char* arg) {
  gLastAct = millis();
  if (gAsleep && strcmp(cmd, "sleep")) { gAsleep = false; applyBrightness(); }
  if (!strcmp(cmd, "wake")) return true;
  if (!strcmp(cmd, "redraw")) { lv_obj_invalidate(lv_scr_act()); lv_obj_invalidate(lv_layer_top()); return true; }
  if (!strcmp(cmd, "stop")) { if (curPage == P_MEDIA) phMediaLeave(); return true; }
  if (!strcmp(cmd, "mnext") || !strcmp(cmd, "mprev")) { if (curPage == P_MEDIA) uiStepMedia(cmd[1] == 'n' ? 1 : -1); return true; }
  if (curPage == P_MEDIA) phMediaLeave();                              // any other command first leaves the player
  if (!strcmp(cmd, "home"))  { phHome(); return true; }
  if (!strcmp(cmd, "lock"))  { if (phCur != PH_LOCK) phShowLock(); return true; }
  if (!strcmp(cmd, "sleep")) { phShadeSet(false); gAsleep = true; applyBrightness(); return true; }
  if (!strcmp(cmd, "shade")) { phShadeSet(arg[0] != '0'); return true; }
  if (!strcmp(cmd, "open")) {
    static const struct { const char* n; PhScr id; } M[] = {
      {"mon", PH_MON}, {"cpu", PH_CPU}, {"mem", PH_MEM}, {"net", PH_NET}, {"tasks", PH_TASKS},
      {"rmt", PH_RMT}, {"clk", PH_CLK}, {"gal", PH_GAL}, {"set", PH_SET}, {"info", PH_INFO}};
    for (size_t i = 0; i < sizeof(M) / sizeof(M[0]); i++) {
      if (strcmp(arg, M[i].n)) continue;
      phShadeSet(false);
      phOpen(M[i].id);
      return phCur == M[i].id;
    }
    return false;
  }
  return false;
}
static void phPump() {                                                 // called by the web server while a big upload is received
  if (curPage == P_MEDIA || !phDrv.draw_buf) return;
  phTickInc();
  lv_timer_handler();
}

static bool phPre = false;
static size_t phBufPx = 0;
// Called from setup() BEFORE WiFi starts: the LVGL pool (LV_MEM_KB, taken from the heap inside lv_init) and the draw buffers are
// reserved while the heap is still in one piece, so LVGL can never run out of memory later because WiFi / the web server grew.
#define LVUI_PREINIT 1
static void lvUiPreInit() {
  if (phPre) return;
  phPre = true;
  lv_init();
  size_t lines = 10;
  for (; lines >= 4; lines -= 2) {
    phBufPx = LV_HOR * lines;
    phBuf1 = (lv_color_t*)heap_caps_malloc(phBufPx * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (phBuf1) break;
  }
#ifndef PH_ONE_BUF   // -DPH_ONE_BUF = single draw buffer (saves ~6 KB RAM)
  if (phBuf1) phBuf2 = (lv_color_t*)heap_caps_malloc(phBufPx * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);   // optional: LVGL also runs with one buffer
#endif
  lv_mem_monitor_t m;
  lv_mem_monitor(&m);
  Serial.printf("LVGL: pool %u KB (free %u), draw buffers %u px x%d, heap=%u maxblock=%u\n", (unsigned)(m.total_size / 1024), (unsigned)m.free_size,
                (unsigned)phBufPx, phBuf2 ? 2 : (phBuf1 ? 1 : 0), (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
}

static void lvUiInit() {
  lvUiPreInit();                                                       // no-op when setup() already did it
  if (!phBuf1) { Serial.println("FATAL: no RAM for the LVGL draw buffer -> restart"); delay(300); ESP.restart(); }
  lv_disp_draw_buf_init(&phDraw, phBuf1, phBuf2, phBufPx);
  lv_disp_drv_init(&phDrv);
  phDrv.hor_res = LV_HOR; phDrv.ver_res = LV_VER;
  phDrv.flush_cb = phFlush; phDrv.draw_buf = &phDraw;
  lv_disp_drv_register(&phDrv);
  lv_indev_drv_init(&phIn);
  phIn.type = LV_INDEV_TYPE_POINTER;
  phIn.read_cb = phTouchRead;
  phIn.scroll_throw = 10;
  phIn.scroll_limit = 8;
  phIn.gesture_limit = 28;                                             // short swipes count on a small resistive panel
  phIn.gesture_min_velocity = 2;
  lv_indev_drv_register(&phIn);

  phLastTick = millis();
  uiCmdHook = phWebCmd; uiScrHook = phScrName; uiPumpHook = phPump;
  configTzTime(PH_TZ, "pool.ntp.org", "time.google.com");
  phPrefLoad();
  phStylesInit();
  phBuildChrome();
  lv_obj_t* old = lv_scr_act();
  lv_obj_t* s = phBuildLock();
  lv_scr_load(s);
  lv_obj_del(old);
  phStatus();
  Serial.printf("Phone UI ready: heap=%u maxblock=%u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
}

// Full-screen player (SD video / GIF / photo, web upload).  Swipe left / right = next / previous file, tap = back to the UI.
// The finger that started the player is ignored until it is lifted.  Exit happens on RELEASE, so no click leaks into the UI underneath.
static bool phWasMedia = false;
static void phMediaTick() {
  mediaService();
  if (curPage != P_MEDIA) return;
  static bool down = false;
  static uint32_t lastSeen = 0, t0 = 0;
  static int x0 = 0, y0 = 0, xl = 0, yl = 0;
  if (!phWasMedia) { phMediaArmed = false; down = false; lastSeen = millis(); tft.waitDMA(); }
  phWasMedia = true;
  if (lightPath.length()) lightTick();
  else if (liveType == LT_GIF) gifTick();
  else if (liveType == LT_CLIP || liveType == LT_STREAM) clipTick();
  int rx, ry;
  bool t = touchRaw(rx, ry);
  uint32_t now = millis();
  if (t) lastSeen = now;
  bool released = !t && (now - lastSeen > 50);                         // XPT2046 flickers while pressed: 50 ms without contact = finger up
  if (!phMediaArmed) {
    if (released) { phMediaArmed = true; down = false; }
  } else if (t) {
    int x, y;
    phMapTouch(rx, ry, x, y);
    if (!down) { down = true; t0 = now; x0 = x; y0 = y; }
    xl = x; yl = y;
  } else if (down && released) {
    down = false;
    int dx = xl - x0, dy = yl - y0;
    if (abs(dx) >= 50 && abs(dx) > abs(dy)) uiStepMedia(dx < 0 ? 1 : -1);
    else if (abs(dx) < 35 && abs(dy) < 35) phMediaLeave();             // tap: back to the phone UI
  }
  delay(1);
}

static void lvUiTick() {
  phTickInc();                                                         // keep LVGL time running even while a video plays (no animation jump afterwards)
  if (curPage == P_MEDIA) { phMediaTick(); return; }
  if (phWasMedia) { phWasMedia = false; lv_obj_invalidate(lv_scr_act()); lv_obj_invalidate(lv_layer_top()); }
  mediaService();

  if (newData) {
    newData = false;
    takeSnapshot();
    alertEval();                                                       // CPU / GPU temp, CPU, RAM alerts (classic loop does this, LVGL loop did not)
    phLiveUpdate(true);
    phStatus();
    if (phShadeOn) phShadeFill();
  }

  rmTick();

  static uint32_t t1 = 0;
  if (millis() - t1 > 1000) {
    t1 = millis();
    if (gIdleMin && !gAsleep && !alertActive && millis() - gLastAct > (uint32_t)gIdleMin * 60000UL) gAsleep = true;
    if (gAsleep && alertActive) gAsleep = false;                       // an active alert keeps / wakes the screen
    applyBrightness();
    if (gAsleep) { phNeedLock = phLockOnWake; if (phShadeOn) phShadeSet(false); }
    else if (phNeedLock) { phNeedLock = false; if (phCur != PH_LOCK) phShowLock(); }
    phStatus();
    lv_mem_monitor_t lvm;
    lv_mem_monitor(&lvm);
    if ((ESP.getFreeHeap() < 12000 || lvm.free_biggest_size < 4000) && phCur != PH_HOME && phCur != PH_LOCK) {   // last resort before LVGL runs out of memory
      Serial.printf("LOW RAM recovery: free=%u maxblock=%u lvgl free=%u biggest=%u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(), (unsigned)lvm.free_size, (unsigned)lvm.free_biggest_size);
      phShadeSet(false);
      phHome();
    }
    switch (phCur) {
      case PH_LOCK: phLockUpdate(); break;
      case PH_CLK:  phUpdClk(); break;
      case PH_INFO: phUpdInfo(); break;
      case PH_HOME: phHomeUpdate(); break;
      default: break;
    }
    if (phShadeOn) phShadeFill();
    static bool apShown = false;                                       // first-run WiFi setup: jump to Info (shows the AP IP)
    if (apMode && WiFi.status() != WL_CONNECTED) {
      if (!apShown && (phCur == PH_LOCK || phCur == PH_HOME)) { apShown = true; phOpen(PH_INFO); }
    } else apShown = false;
  }
  static uint32_t lastHeap = 0;
  if (millis() - lastHeap > 10000) {
    lastHeap = millis();
    lv_mem_monitor_t lvm;
    lv_mem_monitor(&lvm);
    Serial.printf("heap=%u maxblock=%u wifi=%d uiStackFree=%uB lvgl: used %u%% free=%u biggest=%u frag=%u%% scr=%s\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(), (int)WiFi.status(),
                  (unsigned)uxTaskGetStackHighWaterMark(NULL), (unsigned)lvm.used_pct, (unsigned)lvm.free_size, (unsigned)lvm.free_biggest_size, (unsigned)lvm.frag_pct, phScrName());
  }

  uint32_t wait = lv_timer_handler();
  delay(wait > 6 ? 6 : (wait ? wait : 1));
}
#endif  // USE_LVGL
