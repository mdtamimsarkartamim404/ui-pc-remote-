// "DECK" UI for the 320x240 CYD (LVGL 8.3): colourful phone-style launcher + Stream Deck keys. No monitoring pages.
//
//  HOME          10 big app-icon keys (5x2) = the buttons of the current PROFILE (= web panel "Remote" tab). Every key is a vivid
//                gradient tile (its colour from the web panel) with the 32x32 flash icon or a big monogram letter. The wallpaper
//                takes the colour of the profile. Top bar: profile pill (tap = picker / Back in a folder), page dots, Wi-Fi + clock
//                (tap = Control centre). Dock: Reel | Wi-Fi | Control | Standby | Settings.
//                Swipe left / right = next / previous profile (in a folder: right = back). Swipe DOWN = Control centre.
//  CONTROL       volume - / + / mute (hold = repeat), brightness, hotspot on/off, Wi-Fi, standby, screen off.
//  PROFILES      every web panel tab is a profile. AUTO PROFILE: active PC window / app name contains a profile name (>= 3 letters).
//  FOLDERS       key action  goto:NAME  opens profile NAME as a folder.      MACROS  macro:a|b|c  (badge on the key)
//  MEDIA KEYS    media:all = SD slideshow,  media:clip.mjv = play that file.
//  REEL          SD videos / GIFs / photos. tap = play full screen, tap again = back.
//  WI-FI         scan, pick a network, on-screen keyboard for the password, hotspot on/off (the firmware hotspot PC-Monitor-Setup).
//  STANDBY       big clock after N idle seconds (optional SD slideshow).
//
// RAM: all looks use SHARED styles (a key costs ~4 small objects), the permanent send task is gone (created on demand, deleted when idle),
// flash icons are only loaded while enough heap is free (the rest get a monogram tile), and screens are swapped through an empty screen
// when the LVGL pool is too small to hold two screens at once. That keeps WiFi + web panel alive.
#pragma once
#ifdef USE_LVGL
#include <lvgl.h>
#include <time.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <vector>
#include <Preferences.h>

#define LV_HOR 320
#define LV_VER 240

#ifndef DK_TZ
#define DK_TZ "<+06>-6"                          // POSIX TZ string. Dhaka = UTC+6
#endif
#ifndef DK_ICON_MIN_HEAP
#ifndef DK_ICON_MIN_HEAP
#define DK_ICON_MIN_HEAP 28000                  // -DDK_ICON_MIN_HEAP=99999999 = never keep icons in RAM (monogram letters only): saves up to 20 KB
#endif                   // flash icons (2 KB each) are only loaded while at least this much heap is free
#endif

// ---------------- palette (vivid, phone-like) ----------------
#define DC_TX   0xFFFFFF
#define DC_MUT  0xA9B4D6
#define DC_DIM  0x6E7AA6
#define DC_CARD 0x172044
#define DC_CARDB 0x2C3B7A
#define DC_CY   0x22D3EE
#define DC_GRN  0x34D399
#define DC_RED  0xFB7185
#define DC_YEL  0xFACC15
#define DC_ORG  0xFB923C
#define DC_PNK  0xF472B6
#define DC_VIO  0xA78BFA
#define DC_BLU  0x4F8CFF
#define DC_INK  0x0B1226

static inline lv_color_t dkK(uint32_t h) { return lv_color_hex(h); }
static inline void dkSet(lv_obj_t* l, const char* t) {      // skip identical text: no needless invalidation
  const char* c = lv_label_get_text(l);
  if (c && !strcmp(c, t)) return;
  lv_label_set_text(l, t);
}
static lv_color_t dkC565(uint16_t c) { return lv_color_make(((c >> 11) & 31) * 255 / 31, ((c >> 5) & 63) * 255 / 63, (c & 31) * 255 / 31); }

// =====================================================================
//  display + touch drivers (same as the phone UI)
// =====================================================================
static lv_disp_draw_buf_t dkDraw;
static lv_disp_drv_t dkDrv;
static lv_indev_drv_t dkIn;
static lv_color_t *dkBuf1 = nullptr, *dkBuf2 = nullptr;
static bool dkSwallowNext = false;
static bool dkMediaArmed = true;                 // media player: wait until the finger that started it is lifted

static void dkFlush(lv_disp_drv_t* d, const lv_area_t* a, lv_color_t* c) {
  int32_t w = a->x2 - a->x1 + 1, h = a->y2 - a->y1 + 1;
  tft.waitDMA();
  tft.pushImageDMA(a->x1, a->y1, w, h, (const lgfx::rgb565_t*)c);
  lv_disp_flush_ready(d);
}
static void dkMapTouch(int rx, int ry, int& x, int& y) {
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
static void dkTouchRead(lv_indev_drv_t*, lv_indev_data_t* d) {
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
      if (dkSwallowNext) { swallow = true; dkSwallowNext = false; }
      if (gAsleep) { gAsleep = false; swallow = true; applyBrightness(); }
      int x, y;
      dkMapTouch(rx, ry, x, y);
      lx = x; ly = y;
      miss = 0; pressed = true;
    } else if (pressed && ++miss >= 4) { pressed = false; swallow = false; }
  }
  d->point.x = lx; d->point.y = ly;
  d->state = (pressed && !swallow) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// =====================================================================
//  helpers + SHARED styles
// =====================================================================
enum DkScr { DK_NONE, DK_DECK, DK_CTRL, DK_REEL, DK_SET, DK_STBY, DK_WIFI };
static DkScr dkCur = DK_NONE;

static uint32_t dkLastTick = 0;
static inline void dkTickInc() { uint32_t n = millis(); if (n != dkLastTick) { lv_tick_inc(n - dkLastTick); dkLastTick = n; } }

static bool dkNow(struct tm& t) {
  time_t n = time(nullptr);
  if (n < 1700000000) return false;              // NTP not synced yet
  localtime_r(&n, &t);
  return true;
}

// time text that follows the web-panel settings: 24 h "14:05" / "14:05:09", 12 h "2:05 PM" / "2:05:09" (AM/PM is shown separately where there is no room)
static void dkFmtTime(char* b, size_t n, const struct tm& t, bool allowSec, bool ampm) {
  int h = t.tm_hour;
  bool pm = h >= 12;
  if (clock12) { h %= 12; if (!h) h = 12; }
  int k = snprintf(b, n, clock12 ? "%d:%02d" : "%02d:%02d", h, t.tm_min);
  if (allowSec && clockSec && k > 0 && (size_t)k < n) k += snprintf(b + k, n - k, ":%02d", t.tm_sec);
  if (clock12 && ampm && k > 0 && (size_t)k < n) snprintf(b + k, n - k, " %s", pm ? "PM" : "AM");
}

static lv_style_t stTile, stKeyPr, stLbl, stMono, stGhost, stChip, stChipPr, stCard, stDockBar, stDockBtn, stDockPr;
static void dkStylesInit() {
  lv_style_init(&stTile);                        // vivid icon tile (colours are set per tile)
  lv_style_set_radius(&stTile, 15);
  lv_style_set_bg_opa(&stTile, LV_OPA_COVER);
  lv_style_set_bg_grad_dir(&stTile, LV_GRAD_DIR_VER);
  lv_style_set_border_width(&stTile, 0);
  lv_style_init(&stKeyPr);                       // pressed key: soft white glow behind icon + label
  lv_style_set_radius(&stKeyPr, 16);
  lv_style_set_bg_color(&stKeyPr, lv_color_white());
  lv_style_set_bg_opa(&stKeyPr, 60);
  lv_style_init(&stLbl);
  lv_style_set_text_font(&stLbl, &lv_font_montserrat_12);
  lv_style_set_text_color(&stLbl, lv_color_white());
  lv_style_set_text_align(&stLbl, LV_TEXT_ALIGN_CENTER);
  lv_style_init(&stMono);
  lv_style_set_text_font(&stMono, &lv_font_montserrat_28);
  lv_style_set_text_color(&stMono, lv_color_white());
  lv_style_init(&stGhost);                       // switched-off key slot
  lv_style_set_radius(&stGhost, 15);
  lv_style_set_bg_opa(&stGhost, 0);
  lv_style_set_border_width(&stGhost, 1);
  lv_style_set_border_color(&stGhost, dkK(0x34407A));
  lv_style_set_border_opa(&stGhost, 140);
  lv_style_init(&stChip);
  lv_style_set_radius(&stChip, LV_RADIUS_CIRCLE);
  lv_style_set_bg_color(&stChip, dkK(0x25306B));
  lv_style_set_bg_opa(&stChip, LV_OPA_COVER);
  lv_style_set_border_width(&stChip, 1);
  lv_style_set_border_color(&stChip, dkK(0x3F4E96));
  lv_style_init(&stChipPr);
  lv_style_set_bg_color(&stChipPr, dkK(0x4558B8));
  lv_style_init(&stCard);
  lv_style_set_radius(&stCard, 14);
  lv_style_set_bg_color(&stCard, dkK(DC_CARD));
  lv_style_set_bg_opa(&stCard, 225);
  lv_style_set_border_width(&stCard, 1);
  lv_style_set_border_color(&stCard, dkK(DC_CARDB));
  lv_style_init(&stDockBar);
  lv_style_set_radius(&stDockBar, 20);
  lv_style_set_bg_color(&stDockBar, lv_color_white());
  lv_style_set_bg_opa(&stDockBar, 40);
  lv_style_set_border_width(&stDockBar, 1);
  lv_style_set_border_color(&stDockBar, lv_color_white());
  lv_style_set_border_opa(&stDockBar, 60);
  lv_style_init(&stDockBtn);
  lv_style_set_radius(&stDockBtn, 11);
  lv_style_set_bg_opa(&stDockBtn, LV_OPA_COVER);
  lv_style_set_bg_grad_dir(&stDockBtn, LV_GRAD_DIR_VER);
  lv_style_init(&stDockPr);
  lv_style_set_translate_y(&stDockPr, 2);
}

static lv_obj_t* dkBoxC(lv_obj_t* p, int x, int y, int w, int h, lv_color_t col, int r, int opa = 255) {
  lv_obj_t* o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, col, 0);
  lv_obj_set_style_bg_opa(o, opa, 0);
  lv_obj_set_style_radius(o, r, 0);
  lv_obj_clear_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  return o;
}
static lv_obj_t* dkBox(lv_obj_t* p, int x, int y, int w, int h, uint32_t col, int r, int opa = 255) { return dkBoxC(p, x, y, w, h, dkK(col), r, opa); }
static lv_obj_t* dkLbl(lv_obj_t* p, const char* t, const lv_font_t* f, uint32_t col) {
  lv_obj_t* l = lv_label_create(p);
  dkSet(l, t);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, dkK(col), 0);
  return l;
}
static lv_obj_t* dkCard(lv_obj_t* p, int x, int y, int w, int h) {
  lv_obj_t* o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_add_style(o, &stCard, 0);
  lv_obj_clear_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  return o;
}
static lv_obj_t* dkScreen(uint32_t c1, uint32_t c2 = 0xFFFFFFFF) {
  lv_obj_t* s = lv_obj_create(NULL);
  lv_obj_remove_style_all(s);
  lv_obj_set_size(s, LV_HOR, LV_VER);
  lv_obj_set_style_bg_color(s, dkK(c1), 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  if (c2 != 0xFFFFFFFF) { lv_obj_set_style_bg_grad_color(s, dkK(c2), 0); lv_obj_set_style_bg_grad_dir(s, LV_GRAD_DIR_VER, 0); }
  lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}
static lv_obj_t* dkPageScreen() { return dkScreen(0x2A1F66, 0x0A1030); }      // violet -> navy wallpaper of the sub screens
static void dkGo(lv_obj_t* s, lv_scr_load_anim_t a, uint32_t t = 220) { lv_scr_load_anim(s, a, t, 0, true); }

static void dkTileColors(lv_obj_t* t, lv_color_t col) {                // light top -> deeper bottom
  lv_obj_set_style_bg_color(t, lv_color_mix(lv_color_white(), col, 70), 0);
  lv_obj_set_style_bg_grad_color(t, lv_color_mix(lv_color_black(), col, 70), 0);
}

static void dkAnimY(void* o, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t*)o, v, 0); }
static void dkAnimX(void* o, int32_t v) { lv_obj_set_style_translate_x((lv_obj_t*)o, v, 0); }
static void dkRise(lv_obj_t* o, uint32_t delay) {                      // keys slide up one after another (translate only: no layer buffers)
  lv_obj_set_style_translate_y(o, 14, 0);
  lv_anim_t a; lv_anim_init(&a);
  lv_anim_set_var(&a, o); lv_anim_set_exec_cb(&a, dkAnimY);
  lv_anim_set_values(&a, 14, 0); lv_anim_set_time(&a, 220); lv_anim_set_delay(&a, delay);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}
static void dkPressable(lv_obj_t* o) {
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_style(o, &stChipPr, LV_STATE_PRESSED);
}
static lv_obj_t* dkChipBtn(lv_obj_t* p, int x, int y, int w, int h, const char* t, lv_event_cb_t cb, int ud, lv_obj_t** lbl,
                           const lv_font_t* f = &lv_font_montserrat_12) {
  lv_obj_t* b = lv_obj_create(p);
  lv_obj_remove_style_all(b);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_add_style(b, &stChip, 0);
  lv_obj_add_style(b, &stChipPr, LV_STATE_PRESSED);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_ext_click_area(b, 4);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void*)(intptr_t)ud);
  lv_obj_t* l = dkLbl(b, t, f, DC_TX);
  lv_obj_center(l);
  if (lbl) *lbl = l;
  return b;
}

// =====================================================================
//  settings stored in NVS "deck"
// =====================================================================
static uint16_t dkStbySecs = 60;                 // idle seconds until the standby clock (0 = never)
static bool dkAuto = true;                       // auto profile by active PC window
static bool dkAmbient = false;                   // SD media slideshow while in standby
static void dkPrefSave() {
  Preferences p;
  if (p.begin("deck", false)) { p.putUShort("stby", dkStbySecs); p.putBool("auto", dkAuto); p.putBool("amb", dkAmbient); p.end(); }
}
static void dkPrefLoad() {
  Preferences p;
  if (p.begin("deck", true)) { dkStbySecs = p.getUShort("stby", 60); dkAuto = p.getBool("auto", true); dkAmbient = p.getBool("amb", false); p.end(); }
}

// ---- screens (forward) ----
static void dkShowDeck();
static void dkShowCtrl();
static void dkShowReel();
static void dkShowSet();
static void dkShowStby(bool idle);
static void dkShowWifi();

// =====================================================================
//  full-screen media player glue (video / GIF / photo from the SD card)
// =====================================================================
static bool dkStrip = false;                                          // lock-screen control strip is visible (picture + sound paused)
static uint32_t dkStripAt = 0;
static bool dkAmbientRun = false;                                      // the player was started by the lock screen: tap = control strip instead of leaving
static void dkMediaLeave() {                                           // stop whatever the player shows and return to the LVGL screen
  plActive = false;
  dkStrip = false; dkAmbientRun = false;
  gifStop(); clipClose(); lightStop();
  curPage = P_OVERVIEW; pageStart = millis();
  cpuFor(P_OVERVIEW);
  dkMediaArmed = true;
}
static void dkPlayFailed() {
  if (curPage == P_MEDIA) dkMediaLeave();                              // enterPage() had already switched to the player
  lv_obj_invalidate(lv_scr_act());
  lv_obj_invalidate(lv_layer_top());
}
static void dkStepMedia(int dir) {                                     // previous / next file on the card (swipe in the player, web panel)
  std::vector<String> v;
  sdListNames(v);
  if (v.empty()) { dkMediaLeave(); return; }
  int cnt = (int)v.size(), cur = -1;
  for (int i = 0; i < cnt; i++) if (v[i] == liveName) cur = i;
  int n = cur < 0 ? (dir > 0 ? 0 : cnt - 1) : (cur + dir + cnt) % cnt;
  plActive = false;
  for (int t = 0; t < cnt; t++) {                                      // skip files that cannot be played
    if (sdPlayFile(v[n])) return;
    n = (n + dir + cnt) % cnt;
  }
  dkMediaLeave();
}
static bool dkPlayAll() {                                              // slideshow / playlist of everything on the card
  if (!sdOk) return false;
  plIdx = -1; plActive = true; dkMediaArmed = false;
  tft.waitDMA();
  sdPlayNext();
  if (!plActive) { dkPlayFailed(); return false; }
  return true;
}

// =====================================================================
//  toast (replaces the page dots for a moment)
// =====================================================================
static lv_obj_t *dkToast = nullptr, *dkToastL = nullptr;
static lv_timer_t* dkToastT = nullptr;
static void dkToastHide(lv_timer_t*) { dkToastT = nullptr; if (dkToast) lv_obj_add_flag(dkToast, LV_OBJ_FLAG_HIDDEN); }
static void dkToastShow(const char* t, uint32_t col, uint32_t ms = 1400) {
  if (!dkToast) return;
  dkSet(dkToastL, t);
  lv_obj_set_style_border_color(dkToast, dkK(col), 0);
  lv_obj_set_style_text_color(dkToastL, dkK(col), 0);
  lv_obj_clear_flag(dkToast, LV_OBJ_FLAG_HIDDEN);
  if (dkToastT) lv_timer_del(dkToastT);
  dkToastT = lv_timer_create(dkToastHide, ms, NULL);
  lv_timer_set_repeat_count(dkToastT, 1);
}

// =====================================================================
//  HOME screen: state
// =====================================================================
#define DK_KW 58                                 // key container
#define DK_KH 74
#define DK_KTILE 54
static lv_obj_t *dkScr = nullptr, *dkGrid = nullptr, *dkPill = nullptr, *dkPillL = nullptr, *dkDots = nullptr, *dkWifiL = nullptr, *dkClkL = nullptr, *dkPick = nullptr;
static lv_obj_t *dkKeyObj[RMT_PER], *dkKeyLbl[RMT_PER];                // dkKeyObj = the coloured tile of each key
static uint16_t *dkIcoBuf[RMT_PER], *dkIcoOld[RMT_PER];
static lv_img_dsc_t dkIcoDsc[RMT_PER];
static int dkSp = 0, dkStk[4];                   // folder stack (profile indexes)
static int dkConfIdx = -1;
static uint32_t dkConfUntil = 0, dkLeaveAt = 0;
static int dkPressX = 0, dkPressY = 0;
static lv_timer_t* dkFlashT = nullptr;
static int dkFlashIdx = -1;
static char dkLastApp[24] = "";

static volatile int dkSendBusy = 0, dkSendRes = -1;
static volatile int dkSendIdx = -1;
static volatile bool dkSendQuick = false;
static TaskHandle_t dkTask = nullptr;            // created on the first send, deleted again after a few idle seconds (saves a 6 KB stack)
static uint32_t dkTaskLast = 0;
static char dkSendId[64], dkSendLab[13];

static lv_color_t dkAccent() {                   // profile colour = colour of its first key
  if (rmtTabs > 0 && rmtCur < rmtTabs && rmtTab[rmtCur].n > 0) return dkC565(RMT_COL[rmtTab[rmtCur].b[0].col & 7]);
  return dkK(DC_BLU);
}

static void dkFreeIcons() {
  for (int i = 0; i < RMT_PER; i++) if (dkIcoBuf[i]) { free(dkIcoBuf[i]); dkIcoBuf[i] = nullptr; }
}
static void dkFreeOld() {
  for (int i = 0; i < RMT_PER; i++) if (dkIcoOld[i]) { free(dkIcoOld[i]); dkIcoOld[i] = nullptr; }
}

// ---- send task (network can block ~1.5 s, so it has its own task) ----
static void dkTaskFn(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    dkSendRes = remoteSend(dkSendId, dkSendLab);
    dkSendBusy = 0;
  }
}
static bool dkTaskStart() {
  if (dkTask) return true;
  return xTaskCreatePinnedToCore(dkTaskFn, "dksend", 6144, NULL, 1, &dkTask, 0) == pdPASS;
}
static bool dkSendStart(const char* id, const char* lab, int idx, bool quick) {
  if (dkSendBusy) return false;
  strlcpy(dkSendId, id, sizeof(dkSendId));
  strlcpy(dkSendLab, lab, sizeof(dkSendLab));
  if (!dkTaskStart()) { dkToastShow("Low RAM, try again", DC_RED, 2200); return false; }
  dkTaskLast = millis();
  dkSendIdx = idx; dkSendQuick = quick; dkSendRes = -1; dkSendBusy = 1;
  xTaskNotifyGive(dkTask);
  return true;
}

// ---- key border / flash (on the tile) ----
static void dkKeyBase(int i) {
  if (i < 0 || i >= RMT_PER || !dkKeyObj[i] || rmtTabs <= 0) return;
  bool d = rmtTab[rmtCur].b[i].danger;
  lv_obj_set_style_border_width(dkKeyObj[i], d ? 2 : 0, 0);
  lv_obj_set_style_border_color(dkKeyObj[i], dkK(DC_RED), 0);
}
static void dkFlashEnd(lv_timer_t*) {
  dkFlashT = nullptr;
  if (dkFlashIdx >= 0 && dkCur == DK_DECK) dkKeyBase(dkFlashIdx);
  dkFlashIdx = -1;
}
static void dkFlash(int i, uint32_t col) {
  if (i < 0 || i >= RMT_PER || !dkKeyObj[i]) return;
  if (dkFlashT) { lv_timer_del(dkFlashT); dkFlashT = nullptr; }
  if (dkFlashIdx >= 0 && dkFlashIdx != i) dkKeyBase(dkFlashIdx);
  dkFlashIdx = i;
  lv_obj_set_style_border_width(dkKeyObj[i], 4, 0);
  lv_obj_set_style_border_color(dkKeyObj[i], dkK(col), 0);
  dkFlashT = lv_timer_create(dkFlashEnd, 450, NULL);
  lv_timer_set_repeat_count(dkFlashT, 1);
}
static void dkConfClear() {
  if (dkConfIdx >= 0 && dkConfIdx < RMT_PER && dkKeyObj[dkConfIdx] && rmtTabs > 0) {
    dkKeyBase(dkConfIdx);
    if (dkKeyLbl[dkConfIdx]) dkSet(dkKeyLbl[dkConfIdx], rmtTab[rmtCur].b[dkConfIdx].lab);
  }
  dkConfIdx = -1;
}

// =====================================================================
//  profiles / folders / key taps
// =====================================================================
static void dkBuildKeys(int dir);
static void dkTopUpdate();
static void dkSetProfile(int t, int dir) {
  if (rmtTabs <= 0) return;
  rmtCur = ((t % rmtTabs) + rmtTabs) % rmtTabs;
  rmtConfirmSlot = -1;
  dkBuildKeys(dir);
  dkTopUpdate();
}
static void dkStep(int d) { if (rmtTabs <= 1) return; dkSp = 0; dkSetProfile(rmtCur + d, d); }
static void dkGoto(const char* name) {                                // folder = open another profile, remember where we came from
  for (int t = 0; t < rmtTabs; t++) {
    if (strcasecmp(rmtTab[t].name, name)) continue;
    if (t == rmtCur) return;
    if (dkSp < 4) dkStk[dkSp++] = rmtCur;
    dkSetProfile(t, 1);
    return;
  }
  char m[32]; snprintf(m, sizeof(m), "No profile %s", name);
  dkToastShow(m, DC_RED, 1800);
}
static void dkBack() {
  if (dkSp <= 0) return;
  int t = dkStk[--dkSp];
  dkSetProfile(t, -1);
}

static void dkMediaKey(const char* arg) {
  if (!sdOk) { dkToastShow("SD card not ready", DC_ORG, 1800); return; }
  if (!strcasecmp(arg, "all")) { if (!dkPlayAll()) dkToastShow("No media on SD card", DC_RED, 1800); return; }
  std::vector<String> v;
  sdListNames(v);
  for (size_t i = 0; i < v.size(); i++) {
    if (!v[i].equalsIgnoreCase(arg)) continue;
    plActive = false; dkMediaArmed = false;
    tft.waitDMA();
    if (!sdPlayFile(v[i])) { dkPlayFailed(); dkToastShow(lastErr.c_str(), DC_RED, 2200); }
    return;
  }
  dkToastShow("File not found", DC_RED, 1800);
}

static void dkTap(int i, bool repeat) {
  if (rmtTabs <= 0 || i < 0 || i >= rmtTab[rmtCur].n) return;
  const RmtBtn& a = rmtTab[rmtCur].b[i];
  if (!strncmp(a.id, "goto:", 5)) { if (!repeat) dkGoto(a.id + 5); return; }
  if (!strncmp(a.id, "media:", 6)) { if (!repeat) dkMediaKey(a.id + 6); return; }
  if (dkSendBusy) return;
  if (a.danger && !repeat) {                                           // dangerous: first tap only arms the key
    if (!(dkConfIdx == i && millis() < dkConfUntil)) {
      dkConfClear();
      dkConfIdx = i; dkConfUntil = millis() + 3000;
      lv_obj_set_style_border_color(dkKeyObj[i], dkK(DC_YEL), 0);
      lv_obj_set_style_border_width(dkKeyObj[i], 3, 0);
      if (dkKeyLbl[i]) dkSet(dkKeyLbl[i], "SURE?");
      return;
    }
    dkConfClear();
  }
  dkSendStart(a.id, a.lab, i, false);
}
static void dkKeyEvt(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  lv_event_code_t c = lv_event_get_code(e);
  lv_indev_t* in = lv_indev_get_act();
  lv_point_t p = {0, 0};
  if (in) lv_indev_get_point(in, &p);
  if (c == LV_EVENT_PRESSED) { dkPressX = p.x; dkPressY = p.y; }
  else if (c == LV_EVENT_CLICKED) { if (abs(p.x - dkPressX) < 24 && abs(p.y - dkPressY) < 24) dkTap(i, false); }   // a swipe is not a tap
  else if (c == LV_EVENT_LONG_PRESSED_REPEAT) { if (rmtTabs > 0 && i < rmtTab[rmtCur].n && rmtTab[rmtCur].b[i].rep) dkTap(i, true); }
}

static void dkBuildKeys(int dir) {                                     // dir -1 / +1 = slide direction, 0 = keys rise one by one
  if (dkFlashT) { lv_timer_del(dkFlashT); dkFlashT = nullptr; }
  dkFlashIdx = -1; dkConfIdx = -1;
  lv_obj_clean(dkGrid);
  dkFreeIcons();
  for (int i = 0; i < RMT_PER; i++) { dkKeyObj[i] = nullptr; dkKeyLbl[i] = nullptr; }
  lv_obj_set_style_translate_x(dkGrid, 0, 0);
  if (rmtTabs <= 0) {
    lv_obj_t* l = dkLbl(dkGrid, "No profiles yet.\nAdd them in the web panel (Remote tab).", &lv_font_montserrat_14, DC_MUT);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, 0);
    return;
  }
  if (rmtCur >= rmtTabs) rmtCur = rmtTabs - 1;
  if (rmtCur < 0) rmtCur = 0;
  RmtTab& T = rmtTab[rmtCur];
  for (int i = 0; i < RMT_PER; i++) {
    int x = 6 + (i % 5) * 62, y = 2 + (i / 5) * 82;
    if (i >= T.n) {                                                    // empty slot
      lv_obj_t* g = lv_obj_create(dkGrid);
      lv_obj_remove_style_all(g);
      lv_obj_set_pos(g, x + 2, y + 2);
      lv_obj_set_size(g, DK_KTILE, DK_KTILE);
      lv_obj_add_style(g, &stGhost, 0);
      lv_obj_clear_flag(g, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
      continue;
    }
    const RmtBtn& a = T.b[i];
    lv_color_t col = dkC565(RMT_COL[a.col & 7]);
    lv_obj_t* b = lv_btn_create(dkGrid);                               // whole key (icon + name) is the touch target
    lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, DK_KW, DK_KH);
    lv_obj_add_style(b, &stKeyPr, LV_STATE_PRESSED);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_ext_click_area(b, 2);
    lv_obj_add_event_cb(b, dkKeyEvt, LV_EVENT_ALL, (void*)(intptr_t)i);

    lv_obj_t* t = lv_obj_create(b);                                    // vivid gradient tile
    dkKeyObj[i] = t;
    lv_obj_remove_style_all(t);
    lv_obj_set_pos(t, 2, 2);
    lv_obj_set_size(t, DK_KTILE, DK_KTILE);
    lv_obj_add_style(t, &stTile, 0);
    dkTileColors(t, col);
    lv_obj_set_style_border_color(t, dkK(DC_RED), 0);
    lv_obj_set_style_border_width(t, a.danger ? 2 : 0, 0);
    lv_obj_clear_flag(t, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));

    bool hasIco = false;
    if (a.ico && ESP.getFreeHeap() > DK_ICON_MIN_HEAP) {               // 32x32 RGB565 flash icon from the web panel (only while heap is plentiful)
      uint16_t* buf = ESP.getMaxAllocHeap() > 8192 ? (uint16_t*)malloc(ICON_BYTES) : nullptr;
      if (buf && iconRead(a.ico, buf)) {
        for (int k = 0; k < ICON_W * ICON_W; k++) if (!buf[k]) buf[k] = 0x07E0;                  // transparent -> chroma key
        dkIcoBuf[i] = buf;
        dkIcoDsc[i].header.cf = LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED;
        dkIcoDsc[i].header.always_zero = 0;
        dkIcoDsc[i].header.w = ICON_W; dkIcoDsc[i].header.h = ICON_W;
        dkIcoDsc[i].data_size = ICON_BYTES;
        dkIcoDsc[i].data = (const uint8_t*)buf;
        lv_obj_t* im = lv_img_create(t);
        lv_img_set_src(im, &dkIcoDsc[i]);
        lv_obj_center(im);
        hasIco = true;
      } else if (buf) free(buf);
    }
    if (!hasIco) {                                                     // no icon: big monogram letter
      char m[2] = {0, 0};
      unsigned char ch = (unsigned char)a.lab[0];
      m[0] = (ch >= 33 && ch < 127) ? (char)toupper(ch) : '#';
      lv_obj_t* ml = lv_label_create(t);
      lv_obj_add_style(ml, &stMono, 0);
      dkSet(ml, m);
      lv_obj_center(ml);
    }
    const char* badge = nullptr;                                       // what kind of key is this?
    if (!strncmp(a.id, "macro:", 6)) badge = LV_SYMBOL_NEXT;
    else if (!strncmp(a.id, "goto:", 5)) badge = LV_SYMBOL_DIRECTORY;
    else if (!strncmp(a.id, "media:", 6)) badge = LV_SYMBOL_VIDEO;
    if (badge) { lv_obj_t* bl = lv_label_create(t); lv_obj_add_style(bl, &stLbl, 0); dkSet(bl, badge); lv_obj_set_pos(bl, 38, 3); }

    lv_obj_t* l = lv_label_create(b);                                  // name under the tile, like a phone home screen
    dkKeyLbl[i] = l;
    lv_obj_add_style(l, &stLbl, 0);
    dkSet(l, a.lab);
    lv_obj_set_width(l, DK_KW);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(l, 0, 58);
    if (!dir) dkRise(b, 20 + i * 24);
  }
  lv_obj_scroll_to_y(dkGrid, 0, LV_ANIM_OFF);
  if (dir) {
    lv_anim_t an; lv_anim_init(&an);
    lv_anim_set_var(&an, dkGrid); lv_anim_set_exec_cb(&an, dkAnimX);
    lv_anim_set_values(&an, dir * 46, 0); lv_anim_set_time(&an, 200);
    lv_anim_set_path_cb(&an, lv_anim_path_ease_out); lv_anim_start(&an);
  }
}

// =====================================================================
//  top bar, dock, picker
// =====================================================================
static void dkStatus() {                                               // clock + Wi-Fi state (once a second)
  if (dkCur != DK_DECK || !dkClkL) return;
  struct tm t;
  char b[16];
  if (dkNow(t)) dkFmtTime(b, sizeof(b), t, !clock12, true); else strcpy(b, "--:--");   // 24 h: HH:MM[:SS]   12 h: h:MM AM/PM
  dkSet(dkClkL, b);
  bool wc = WiFi.status() == WL_CONNECTED;
  lv_obj_set_style_text_color(dkWifiL, dkK(pcOnline ? DC_GRN : (wc ? DC_YEL : DC_RED)), 0);
}
static void dkTopUpdate() {
  if (!dkPillL) return;
  lv_color_t ac = dkAccent();
  char b[40];
  const char* nm = rmtTabs > 0 ? rmtTab[rmtCur].name : "NO PROFILE";
  if (dkSp > 0) snprintf(b, sizeof(b), LV_SYMBOL_LEFT "  %s", nm);
  else snprintf(b, sizeof(b), "%s  " LV_SYMBOL_DOWN, nm);
  dkSet(dkPillL, b);
  lv_obj_set_style_bg_color(dkPill, ac, 0);
  lv_obj_set_style_bg_color(dkScr, lv_color_mix(ac, dkK(0x0A1030), 105), 0);      // the wallpaper takes the profile colour
  lv_obj_clean(dkDots);
  int n = rmtTabs > 12 ? 12 : rmtTabs;
  if (n > 1) {
    int dw = n > 8 ? 4 : 6, ow = n > 8 ? 12 : 16, gp = n > 8 ? 3 : 5;       // smaller dots when there are many profiles (12 must fit in 120 px)
    int total = (n - 1) * dw + ow + (n - 1) * gp, x = (100 - total) / 2;
    for (int t = 0; t < n; t++) {
      bool on = t == rmtCur;
      int w = on ? ow : dw;
      dkBoxC(dkDots, x, 0, w, 6, on ? lv_color_white() : dkK(0x6A76B0), 3);
      x += w + gp;
    }
  }
  dkStatus();
}
static void dkPickClose() { if (dkPick) { lv_obj_del(dkPick); dkPick = nullptr; } }
static void dkPickEvt(lv_event_t* e) {
  int t = (int)(intptr_t)lv_event_get_user_data(e);
  dkPickClose();
  if (t >= 0 && t < rmtTabs && t != rmtCur) { dkSp = 0; dkSetProfile(t, t > rmtCur ? 1 : -1); }
}
static void dkPickOpen() {                                             // profile picker: dim the home screen, a coloured chip for every profile
  if (dkPick || rmtTabs < 1) return;
  dkPick = dkBox(dkScr, 0, 0, LV_HOR, LV_VER, 0x000000, 0, 170);
  lv_obj_add_flag(dkPick, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(dkPick, dkPickEvt, LV_EVENT_CLICKED, (void*)(intptr_t)-1);
  int pcount = rmtTabs < 12 ? rmtTabs : 12, prow = (pcount + 3) / 4;
  int ph = 34 + prow * 48;
  lv_obj_t* pn = dkCard(dkPick, 10, (LV_VER - ph) / 2, 300, ph);
  lv_obj_t* tl = dkLbl(pn, "PROFILES", &lv_font_montserrat_12, DC_MUT);
  lv_obj_set_pos(tl, 14, 8);
  for (int t = 0; t < pcount; t++) {
    int x = 8 + (t % 4) * 72, y = 28 + (t / 4) * 48;
    lv_color_t col = rmtTab[t].n > 0 ? dkC565(RMT_COL[rmtTab[t].b[0].col & 7]) : dkK(DC_BLU);
    lv_obj_t* c = lv_obj_create(pn);
    lv_obj_remove_style_all(c);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, 68, 42);
    lv_obj_add_style(c, &stTile, 0);
    dkTileColors(c, col);
    lv_obj_set_style_radius(c, 12, 0);
    if (t == rmtCur) { lv_obj_set_style_border_width(c, 3, 0); lv_obj_set_style_border_color(c, lv_color_white(), 0); }
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(c, dkPickEvt, LV_EVENT_CLICKED, (void*)(intptr_t)t);
    lv_obj_t* l = lv_label_create(c);
    lv_obj_add_style(l, &stLbl, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    dkSet(l, rmtTab[t].name);
    lv_obj_set_width(l, 62);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, 0);
  }
}
static void dkPillEvt(lv_event_t*) { if (dkSp > 0) dkBack(); else dkPickOpen(); }
static void dkStatusEvt(lv_event_t*) { dkShowCtrl(); }
static void dkGestEvt(lv_event_t*) {
  if (dkPick) return;
  lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (d == LV_DIR_LEFT) dkStep(1);
  else if (d == LV_DIR_RIGHT) { if (dkSp > 0) dkBack(); else dkStep(-1); }
  else if (d == LV_DIR_BOTTOM) dkShowCtrl();                           // pull down = Control centre
}
static void dkDockEvt(lv_event_t* e) {
  switch ((int)(intptr_t)lv_event_get_user_data(e)) {
    case 0: dkShowReel(); break;
    case 1: dkShowWifi(); break;
    case 2: dkShowCtrl(); break;
    case 3: dkShowStby(false); break;
    case 4: dkShowSet(); break;
  }
}
static lv_obj_t* dkBuildDeck() {
  lv_obj_t* s = dkScreen(0x2A1F66, 0x070B1E);
  dkScr = s;
  // ---- top bar ----
  dkPill = lv_obj_create(s);
  lv_obj_remove_style_all(dkPill);
  lv_obj_set_pos(dkPill, 8, 2);
  lv_obj_set_size(dkPill, 112, 22);
  lv_obj_set_style_radius(dkPill, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(dkPill, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(dkPill, dkK(DC_BLU), 0);
  lv_obj_clear_flag(dkPill, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_style(dkPill, &stDockPr, LV_STATE_PRESSED);
  lv_obj_set_ext_click_area(dkPill, 4);
  lv_obj_add_event_cb(dkPill, dkPillEvt, LV_EVENT_CLICKED, NULL);
  dkPillL = dkLbl(dkPill, "", &lv_font_montserrat_14, DC_INK);
  lv_obj_set_width(dkPillL, 104);
  lv_label_set_long_mode(dkPillL, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(dkPillL, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(dkPillL, LV_ALIGN_CENTER, 0, 0);
  dkDots = dkBox(s, 126, 9, 100, 8, 0x000000, 0, 0);
  lv_obj_t* sb = dkBox(s, 228, 0, 92, 26, 0x000000, 0, 0);            // Wi-Fi glyph + clock: tap = Control centre
  lv_obj_add_flag(sb, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(sb, dkStatusEvt, LV_EVENT_CLICKED, NULL);
  dkWifiL = dkLbl(sb, LV_SYMBOL_WIFI, &lv_font_montserrat_14, DC_YEL);
  lv_obj_set_pos(dkWifiL, 4, 4);
  dkClkL = dkLbl(sb, "--:--", &lv_font_montserrat_14, DC_TX);
  lv_obj_set_pos(dkClkL, 26, 4);

  // ---- keys ----
  dkGrid = lv_obj_create(s);
  lv_obj_remove_style_all(dkGrid);
  lv_obj_set_pos(dkGrid, 0, 28);
  lv_obj_set_size(dkGrid, LV_HOR, 164);
  lv_obj_clear_flag(dkGrid, LV_OBJ_FLAG_SCROLLABLE);

  // ---- dock: five coloured app icons on a glass bar ----
  lv_obj_t* bar = lv_obj_create(s);
  lv_obj_remove_style_all(bar);
  lv_obj_set_pos(bar, 8, 196);
  lv_obj_set_size(bar, 304, 42);
  lv_obj_add_style(bar, &stDockBar, 0);
  lv_obj_clear_flag(bar, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  static const char* const ic[5] = {LV_SYMBOL_VIDEO, LV_SYMBOL_WIFI, LV_SYMBOL_VOLUME_MAX, LV_SYMBOL_EYE_OPEN, LV_SYMBOL_SETTINGS};
  static const uint32_t icc[5] = {DC_PNK, DC_GRN, DC_BLU, DC_YEL, DC_VIO};
  for (int i = 0; i < 5; i++) {
    lv_obj_t* b = lv_obj_create(s);
    lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, 28 + i * 57, 199);
    lv_obj_set_size(b, 36, 36);
    lv_obj_add_style(b, &stDockBtn, 0);
    lv_obj_add_style(b, &stDockPr, LV_STATE_PRESSED);
    dkTileColors(b, dkK(icc[i]));
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_ext_click_area(b, 8);
    lv_obj_add_event_cb(b, dkDockEvt, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_t* l = dkLbl(b, ic[i], &lv_font_montserrat_16, DC_TX);
    lv_obj_center(l);
  }
  lv_obj_add_event_cb(s, dkGestEvt, LV_EVENT_GESTURE, NULL);

  // ---- toast (on top of the page dots) ----
  dkToast = dkBox(s, 124, 2, 122, 22, 0x0B1222, 11);
  lv_obj_set_style_border_width(dkToast, 2, 0);
  lv_obj_set_style_border_color(dkToast, dkK(DC_GRN), 0);
  dkToastL = dkLbl(dkToast, "", &lv_font_montserrat_12, DC_GRN);
  lv_obj_set_width(dkToastL, 116);
  lv_label_set_long_mode(dkToastL, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(dkToastL, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(dkToastL, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag(dkToast, LV_OBJ_FLAG_HIDDEN);

  dkBuildKeys(0);
  dkTopUpdate();
  return s;
}

// =====================================================================
//  shared bits of the sub screens
// =====================================================================
static void dkBackEvt(lv_event_t* e) { if ((int)(intptr_t)lv_event_get_user_data(e) == 1) dkShowSet(); else dkShowDeck(); }
static lv_obj_t* dkHeader(lv_obj_t* s, const char* title, uint32_t accent, int backTo = 0) {
  dkChipBtn(s, 8, 3, 48, 22, LV_SYMBOL_LEFT, dkBackEvt, backTo, nullptr, &lv_font_montserrat_14);
  lv_obj_t* t = dkLbl(s, title, &lv_font_montserrat_16, accent);
  lv_obj_set_pos(t, 64, 5);
  return t;
}
// coloured tile with a symbol and a caption (Control centre)
static lv_obj_t* dkTile(lv_obj_t* p, int x, int y, int w, int h, uint32_t c, const char* sym, const lv_font_t* sf, const char* cap, lv_event_cb_t cb, int ud) {
  lv_obj_t* t = lv_obj_create(p);
  lv_obj_remove_style_all(t);
  lv_obj_set_pos(t, x, y);
  lv_obj_set_size(t, w, h);
  lv_obj_add_style(t, &stTile, 0);
  lv_obj_add_style(t, &stDockPr, LV_STATE_PRESSED);
  dkTileColors(t, dkK(c));
  lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(t, cb, LV_EVENT_ALL, (void*)(intptr_t)ud);
  lv_obj_t* a = dkLbl(t, sym, sf, DC_TX);
  lv_obj_align(a, LV_ALIGN_CENTER, 0, -8);
  lv_obj_t* l = lv_label_create(t);
  lv_obj_add_style(l, &stLbl, 0);
  dkSet(l, cap);
  lv_obj_set_width(l, w);
  lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -5);
  return t;
}

// =====================================================================
//  CONTROL centre: volume, brightness, hotspot, wi-fi, standby, screen off
// =====================================================================
static lv_obj_t *ctMsg = nullptr, *ctHotT = nullptr;
static lv_obj_t* seBrV = nullptr;
static void dkSetSlider(lv_event_t* e) {
  lv_obj_t* s = lv_event_get_target(e);
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    int v = lv_slider_get_value(s);
    cfg.bright = (uint8_t)v; gLastAct = millis(); lastBl = 0; applyBrightness();
    char b[8]; snprintf(b, sizeof(b), "%d%%", v * 100 / 255);
    if (seBrV) dkSet(seBrV, b);
  } else if (lv_event_get_code(e) == LV_EVENT_RELEASED) saveCfg();
}
static lv_obj_t* dkSlider(lv_obj_t* p, int x, int y, int w) {
  lv_obj_t* sl = lv_slider_create(p);
  lv_obj_set_size(sl, w, 10);
  lv_obj_set_pos(sl, x, y);
  lv_slider_set_range(sl, 5, 255);
  lv_slider_set_value(sl, cfg.bright, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(sl, dkK(0x34407A), LV_PART_MAIN);
  lv_obj_set_style_bg_color(sl, dkK(DC_YEL), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(sl, dkK(DC_TX), LV_PART_KNOB);
  lv_obj_set_ext_click_area(sl, 10);
  lv_obj_add_event_cb(sl, dkSetSlider, LV_EVENT_ALL, NULL);
  return sl;
}
static void dkHotToggle() {                                            // the firmware hotspot PC-Monitor-Setup (192.168.4.1)
  if (!apMode) startAP();
  else if (WiFi.status() == WL_CONNECTED) stopAP();
  else if (ctMsg) { dkSet(ctMsg, "Connect to a Wi-Fi first"); lv_obj_set_style_text_color(ctMsg, dkK(DC_ORG), 0); }
}
static void dkCtHotDraw() { if (ctHotT) dkTileColors(ctHotT, dkK(apMode ? DC_GRN : 0x4A5588)); }
static void dkCtEvt(lv_event_t* e) {
  lv_event_code_t c = lv_event_get_code(e);
  int ud = (int)(intptr_t)lv_event_get_user_data(e);
  if (ud <= 2) {                                                       // 0 vol-, 1 vol+, 2 mute (hold = repeat, not for mute)
    if (c != LV_EVENT_CLICKED && !(c == LV_EVENT_LONG_PRESSED_REPEAT && ud < 2)) return;
    static const char* const id[3] = {"voldown", "volup", "mute"};
    static const char* const lb[3] = {"VOL -", "VOL +", "MUTE"};
    dkSendStart(id[ud], lb[ud], -1, true);
    return;
  }
  if (c != LV_EVENT_CLICKED) return;
  if (ud == 3) { dkHotToggle(); dkCtHotDraw(); }
  else if (ud == 4) dkShowWifi();
  else if (ud == 5) dkShowStby(false);
  else if (ud == 6) { gAsleep = true; applyBrightness(); }
}
static void dkCtGestEvt(lv_event_t*) { if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_TOP) dkShowDeck(); }
static lv_obj_t* dkBuildCtrl() {
  lv_obj_t* s = dkPageScreen();
  dkHeader(s, "Control", DC_BLU);
  dkTile(s, 8, 32, 92, 58, 0x3B82F6, LV_SYMBOL_VOLUME_MID, &lv_font_montserrat_28, "VOL -", dkCtEvt, 0);
  dkTile(s, 106, 32, 92, 58, 0x22D3EE, LV_SYMBOL_VOLUME_MAX, &lv_font_montserrat_28, "VOL +", dkCtEvt, 1);
  dkTile(s, 204, 32, 108, 58, 0xF43F5E, LV_SYMBOL_MUTE, &lv_font_montserrat_28, "MUTE", dkCtEvt, 2);
  lv_obj_t* c = dkCard(s, 8, 96, 304, 46);
  lv_obj_t* l = dkLbl(c, LV_SYMBOL_EYE_OPEN, &lv_font_montserrat_20, DC_YEL);
  lv_obj_set_pos(l, 12, 10);
  dkSlider(c, 50, 18, 204);
  char b[8]; snprintf(b, sizeof(b), "%d%%", cfg.bright * 100 / 255);
  seBrV = dkLbl(c, b, &lv_font_montserrat_14, DC_TX);
  lv_obj_set_pos(seBrV, 266, 13);
  ctHotT = dkTile(s, 8, 148, 72, 62, 0x4A5588, LV_SYMBOL_GPS, &lv_font_montserrat_20, "Hotspot", dkCtEvt, 3);
  dkTile(s, 85, 148, 72, 62, 0x10B981, LV_SYMBOL_WIFI, &lv_font_montserrat_20, "Wi-Fi", dkCtEvt, 4);
  dkTile(s, 162, 148, 72, 62, 0xF59E0B, LV_SYMBOL_EYE_OPEN, &lv_font_montserrat_20, "Standby", dkCtEvt, 5);
  dkTile(s, 239, 148, 72, 62, 0x8B5CF6, LV_SYMBOL_POWER, &lv_font_montserrat_20, "Screen off", dkCtEvt, 6);
  ctMsg = dkLbl(s, "Swipe up to close", &lv_font_montserrat_12, DC_DIM);
  lv_obj_set_pos(ctMsg, 12, 218);
  lv_obj_add_event_cb(s, dkCtGestEvt, LV_EVENT_GESTURE, NULL);
  dkCtHotDraw();
  return s;
}

// =====================================================================
//  REEL: SD videos / GIFs / photos.  tap = play full screen, tap again = back
//  RAM: only 5 rows exist, the name list is freed when the screen closes.
// =====================================================================
#define DK_ROWS 5
static std::vector<String> dkNames;
static int dkFilter = 0, dkPage = 0;                                   // filter: 0 all, 1 videos, 2 photos + GIFs
static lv_obj_t *dkRow[DK_ROWS], *dkRowTag[DK_ROWS], *dkRowTagL[DK_ROWS], *dkRowTxt[DK_ROWS], *dkChip[3], *dkChipL[3], *dkPg;
static lv_timer_t* dkErrT = nullptr;

static void dkReelLoad() {
  std::vector<String> all;
  sdListNames(all);
  dkNames.clear();
  for (size_t i = 0; i < all.size() && dkNames.size() < 120; i++) {
    int k = sdKindOf(all[i]);
    bool v = (k == 1 || k == 5);
    if (dkFilter == 0 || (dkFilter == 1 && v) || (dkFilter == 2 && !v)) dkNames.push_back(all[i]);
  }
  dkNames.shrink_to_fit();
}
static void dkReelFill() {
  int n = (int)dkNames.size(), pages = n ? (n + DK_ROWS - 1) / DK_ROWS : 1;
  if (dkPage >= pages) dkPage = 0;
  char b[12];
  snprintf(b, sizeof(b), "%d/%d", dkPage + 1, pages);
  dkSet(dkPg, b);
  for (int i = 0; i < 3; i++) {
    lv_obj_set_style_bg_color(dkChip[i], dkK(i == dkFilter ? DC_PNK : 0x25306B), 0);
    lv_obj_set_style_text_color(dkChipL[i], dkK(i == dkFilter ? 0x2A0A1A : DC_TX), 0);
  }
  for (int i = 0; i < DK_ROWS; i++) {
    int idx = dkPage * DK_ROWS + i;
    if (idx < n) {
      int k = sdKindOf(dkNames[idx]);
      bool v = (k == 1 || k == 5);
      dkSet(dkRowTagL[i], v ? "VID" : (k == 3 ? "GIF" : "IMG"));
      lv_obj_set_style_bg_color(dkRowTag[i], dkK(v ? DC_PNK : (k == 3 ? DC_VIO : DC_CY)), 0);
      dkSet(dkRowTxt[i], dkNames[idx].c_str());
      lv_obj_set_style_text_color(dkRowTxt[i], dkK(DC_TX), 0);
      lv_obj_clear_flag(dkRow[i], LV_OBJ_FLAG_HIDDEN);
    } else if (i == 0) {
      dkSet(dkRowTagL[0], "!");
      lv_obj_set_style_bg_color(dkRowTag[0], dkK(DC_ORG), 0);
      dkSet(dkRowTxt[0], sdOk ? "No files in /videos /gifs /images" : "SD card not ready");
      lv_obj_set_style_text_color(dkRowTxt[0], dkK(DC_MUT), 0);
      lv_obj_clear_flag(dkRow[0], LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(dkRow[i], LV_OBJ_FLAG_HIDDEN);
  }
}
static void dkErrReset(lv_timer_t*) { dkErrT = nullptr; if (dkCur == DK_REEL) dkReelFill(); }
// A tap must NOT start the player inside the LVGL event: the picture would be drawn first and LVGL would then repaint the pressed row on top of it.
// So the tap only queues the file; dkRunPending() starts it ~40 ms later, after LVGL has finished its refresh (lvUiTick stops calling LVGL once the player runs).
static int dkPendIdx = -1;
static bool dkPendAll = false;
static bool dkPendAmb = false;                                         // lock screen: start the slideshow after the open animation
static String dkPendName;
static uint32_t dkPendWhen = 0;
static void dkRowEvt(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  int idx = dkPage * DK_ROWS + i;
  if (idx >= (int)dkNames.size() || dkPendIdx >= 0 || dkPendAll) return;
  dkPendIdx = i; dkPendName = dkNames[idx]; dkPendWhen = millis() + 40;
}
static void dkRunPending() {
  if (dkPendAmb) {                                                     // lock screen slideshow: the standby screen must be completely drawn first
    dkPendAmb = false;
    if (dkCur != DK_STBY || !sdOk) return;
    dkAmbientRun = true;
    tft.waitDMA();
    tft.fillScreen(TFT_BLACK);                                         // no leftovers of the clock screen around a letter-boxed picture
    dkPlayAll();
    return;
  }
  if (dkPendAll) {
    dkPendAll = false;
    dkAmbientRun = false;
    plActive = false; dkMediaArmed = false;
    tft.waitDMA();
    if (!dkNames.empty()) dkPlayAll();
    return;
  }
  int i = dkPendIdx;
  dkPendIdx = -1;
  if (i < 0) return;
  dkAmbientRun = false;
  String nm = dkPendName;
  plActive = false;
  dkMediaArmed = false;
  tft.waitDMA();                                                       // LVGL's last flush must be finished before the player draws
  if (!sdPlayFile(nm)) {                                               // show the reason in the row for a moment
    dkPlayFailed();
    if (dkCur != DK_REEL) return;                                        // the list is gone (user left the Reel): nothing to colour
    dkSet(dkRowTxt[i], lastErr.c_str());
    lv_obj_set_style_text_color(dkRowTxt[i], dkK(DC_RED), 0);
    if (dkErrT) lv_timer_del(dkErrT);
    dkErrT = lv_timer_create(dkErrReset, 2500, NULL);
    lv_timer_set_repeat_count(dkErrT, 1);
  }
}
static void dkReelAllEvt(lv_event_t*) { if (!dkNames.empty() && dkPendIdx < 0 && !dkPendAll && !dkPendAmb) { dkPendAll = true; dkPendWhen = millis() + 40; } }
static void dkReelChipEvt(lv_event_t* e) { dkFilter = (int)(intptr_t)lv_event_get_user_data(e); dkPage = 0; dkReelLoad(); dkReelFill(); }
static void dkReelNavEvt(lv_event_t* e) {
  int d = (int)(intptr_t)lv_event_get_user_data(e);
  int pages = dkNames.empty() ? 1 : ((int)dkNames.size() + DK_ROWS - 1) / DK_ROWS;
  dkPage = (dkPage + d + pages) % pages;
  dkReelFill();
}
static lv_obj_t* dkBuildReel() {
  lv_obj_t* s = dkPageScreen();
  dkHeader(s, "Reel", DC_PNK);
  static const char* cn[3] = {"All", "Video", "Photo"};
  static const int cx[3] = {150, 198, 256}, cw[3] = {44, 54, 54};
  for (int i = 0; i < 3; i++) dkChip[i] = dkChipBtn(s, cx[i], 3, cw[i], 22, cn[i], dkReelChipEvt, i, &dkChipL[i]);
  for (int i = 0; i < DK_ROWS; i++) {
    dkRow[i] = dkCard(s, 8, 32 + i * 33, 304, 29);
    dkPressable(dkRow[i]);
    lv_obj_add_event_cb(dkRow[i], dkRowEvt, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    dkRowTag[i] = dkBox(dkRow[i], 6, 5, 38, 18, DC_CY, 9);               // film "tag": VID / GIF / IMG
    dkRowTagL[i] = dkLbl(dkRowTag[i], "", &lv_font_montserrat_12, DC_INK);
    lv_obj_set_width(dkRowTagL[i], 38);
    lv_obj_set_style_text_align(dkRowTagL[i], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(dkRowTagL[i], LV_ALIGN_CENTER, 0, 0);
    dkRowTxt[i] = dkLbl(dkRow[i], "", &lv_font_montserrat_14, DC_TX);
    lv_obj_set_width(dkRowTxt[i], 244);
    lv_label_set_long_mode(dkRowTxt[i], LV_LABEL_LONG_DOT);
    lv_obj_set_pos(dkRowTxt[i], 52, 5);
  }
  dkChipBtn(s, 8, 202, 56, 30, LV_SYMBOL_LEFT, dkReelNavEvt, -1, nullptr, &lv_font_montserrat_14);
  dkPg = dkLbl(s, "1/1", &lv_font_montserrat_12, DC_MUT);
  lv_obj_set_width(dkPg, 44);
  lv_obj_set_style_text_align(dkPg, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(dkPg, 68, 210);
  dkChipBtn(s, 116, 202, 56, 30, LV_SYMBOL_RIGHT, dkReelNavEvt, 1, nullptr, &lv_font_montserrat_14);
  lv_obj_t* all = dkChipBtn(s, 180, 202, 132, 30, LV_SYMBOL_PLAY "  Play all", dkReelAllEvt, 0, nullptr, &lv_font_montserrat_14);
  lv_obj_set_style_bg_color(all, dkK(DC_PNK), 0);
  lv_obj_set_style_text_color(lv_obj_get_child(all, 0), dkK(0x2A0A1A), 0);
  dkPage = 0;
  dkReelLoad();
  dkReelFill();
  return s;
}

// =====================================================================
//  SETTINGS
// =====================================================================
static const uint16_t dkStbyOpts[4] = {0, 30, 60, 300};
static const uint8_t dkIdleOpts[4] = {0, 5, 10, 30};
static lv_obj_t *seStby[4], *seIdle[4], *seNet;
static void dkChipsStyle(lv_obj_t** c, int n, int sel) {
  for (int i = 0; i < n; i++) {
    lv_obj_set_style_bg_color(c[i], dkK(i == sel ? DC_CY : 0x25306B), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(c[i], 0), dkK(i == sel ? 0x06202B : DC_TX), 0);
  }
}
static void dkSetStyleAll() {
  int a = 0, b = 0;
  for (int i = 0; i < 4; i++) { if (dkStbySecs == dkStbyOpts[i]) a = i; if (gIdleMin == dkIdleOpts[i]) b = i; }
  dkChipsStyle(seStby, 4, a);
  dkChipsStyle(seIdle, 4, b);
}
static void dkSetStbyEvt(lv_event_t* e) { dkStbySecs = dkStbyOpts[(int)(intptr_t)lv_event_get_user_data(e)]; dkPrefSave(); gLastAct = millis(); dkSetStyleAll(); }
static void dkSetIdleEvt(lv_event_t* e) { gIdleMin = dkIdleOpts[(int)(intptr_t)lv_event_get_user_data(e)]; idleSave(); gLastAct = millis(); dkSetStyleAll(); }
static void dkSetSwEvt(lv_event_t* e) {
  bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  if ((int)(intptr_t)lv_event_get_user_data(e) == 0) dkAuto = on; else dkAmbient = on;
  dkPrefSave();
}
static lv_obj_t* dkSwitch(lv_obj_t* p, int x, int y, bool on, int ud, lv_event_cb_t cb = dkSetSwEvt) {
  lv_obj_t* sw = lv_switch_create(p);
  lv_obj_set_size(sw, 44, 22);
  lv_obj_set_pos(sw, x, y);
  lv_obj_set_style_bg_color(sw, dkK(0x34407A), LV_PART_MAIN);
  lv_obj_set_style_bg_color(sw, dkK(DC_GRN), (lv_style_selector_t)(LV_PART_INDICATOR | LV_STATE_CHECKED));
  lv_obj_set_style_bg_color(sw, dkK(DC_TX), LV_PART_KNOB);
  if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)ud);
  return sw;
}
static void dkSetNet() {
  char b[80];
  if (WiFi.status() == WL_CONNECTED) snprintf(b, sizeof(b), LV_SYMBOL_WIFI "  %s   %s", WiFi.SSID().c_str(), myIp().c_str());
  else if (apMode) snprintf(b, sizeof(b), LV_SYMBOL_WARNING "  Hotspot %s  %s", AP_NAME, myIp().c_str());
  else snprintf(b, sizeof(b), LV_SYMBOL_WARNING "  Not connected - tap to set up Wi-Fi");
  dkSet(seNet, b);
}
static void dkSetWifiEvt(lv_event_t*) { dkShowWifi(); }
static lv_obj_t* dkBuildSet() {
  lv_obj_t* s = dkPageScreen();
  dkHeader(s, "Settings", DC_VIO);
  lv_obj_t* l;
  l = dkLbl(s, "Brightness", &lv_font_montserrat_14, DC_TX); lv_obj_set_pos(l, 12, 38);
  lv_obj_t* sl = dkSlider(s, 112, 44, 160);
  (void)sl;
  char b[8]; snprintf(b, sizeof(b), "%d%%", cfg.bright * 100 / 255);
  seBrV = dkLbl(s, b, &lv_font_montserrat_12, DC_MUT); lv_obj_set_pos(seBrV, 282, 40);

  static const char* const sn[4] = {"Off", "30 s", "1 min", "5 min"};
  static const char* const in[4] = {"Off", "5 min", "10 min", "30 min"};
  l = dkLbl(s, "Standby clock", &lv_font_montserrat_14, DC_TX); lv_obj_set_pos(l, 12, 72);
  for (int i = 0; i < 4; i++) seStby[i] = dkChipBtn(s, 118 + i * 49, 70, 45, 24, sn[i], dkSetStbyEvt, i, nullptr);
  l = dkLbl(s, "Screen off", &lv_font_montserrat_14, DC_TX); lv_obj_set_pos(l, 12, 104);
  for (int i = 0; i < 4; i++) seIdle[i] = dkChipBtn(s, 118 + i * 49, 102, 45, 24, in[i], dkSetIdleEvt, i, nullptr);
  l = dkLbl(s, "Auto profile (active app)", &lv_font_montserrat_14, DC_TX); lv_obj_set_pos(l, 12, 138);
  dkSwitch(s, 264, 135, dkAuto, 0);
  l = dkLbl(s, "Standby = SD slideshow", &lv_font_montserrat_14, DC_TX); lv_obj_set_pos(l, 12, 168);
  dkSwitch(s, 264, 165, dkAmbient, 1);
  lv_obj_t* nb = dkCard(s, 8, 194, 304, 24);
  dkPressable(nb);
  lv_obj_add_event_cb(nb, dkSetWifiEvt, LV_EVENT_CLICKED, NULL);
  seNet = dkLbl(nb, "", &lv_font_montserrat_12, DC_TX);
  lv_obj_set_width(seNet, 266);
  lv_label_set_long_mode(seNet, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(seNet, 10, 4);
  l = dkLbl(nb, LV_SYMBOL_RIGHT, &lv_font_montserrat_12, DC_MUT);
  lv_obj_set_pos(l, 284, 4);
  l = dkLbl(s, "Key actions:  goto:PROFILE   media:all   media:file.mjv", &lv_font_montserrat_12, DC_DIM);
  lv_obj_set_pos(l, 12, 221);
  dkSetStyleAll();
  dkSetNet();
  return s;
}

// =====================================================================
//  WI-FI: scan, switch network (on-screen keyboard), hotspot on / off
//  The firmware already runs the open hotspot "PC-Monitor-Setup" (192.168.4.1) + captive portal; this screen drives the same functions.
// =====================================================================
#define DK_WROWS 3
struct DkNet { char s[33]; int8_t r; bool e; };
static DkNet wfNet[24];
static int wfN = 0, wfPage = 0;
static bool wfScanning = false;
static uint8_t wfPhase = 0;                                            // 1 = connecting
static char wfSel[33], wfMsg[48] = "";
static uint32_t wfMsgUntil = 0, wfMsgCol = DC_MUT, wfLastUi = 0;
static lv_obj_t *wfScr = nullptr, *wfStatA = nullptr, *wfStatB = nullptr, *wfPg = nullptr, *wfHot = nullptr, *wfScanL = nullptr;
static lv_obj_t *wfRow[DK_WROWS], *wfRowT[DK_WROWS], *wfRowI[DK_WROWS], *wfRowE[DK_WROWS];
static lv_obj_t *wfBox = nullptr, *wfTa = nullptr, *wfKb = nullptr;

static void dkWfMsg(const char* t, uint32_t col, uint32_t ms = 2500) { strlcpy(wfMsg, t, sizeof(wfMsg)); wfMsgCol = col; wfMsgUntil = millis() + ms; wfLastUi = 0; }
static void dkWfFill() {
  if (!wfPg) return;
  int pages = wfN ? (wfN + DK_WROWS - 1) / DK_WROWS : 1;
  if (wfPage >= pages) wfPage = 0;
  char b[12]; snprintf(b, sizeof(b), "%d/%d", wfPage + 1, pages);
  dkSet(wfPg, b);
  for (int i = 0; i < DK_WROWS; i++) {
    int idx = wfPage * DK_WROWS + i;
    if (idx < wfN) {
      const DkNet& n = wfNet[idx];
      dkSet(wfRowT[i], n.s);
      dkSet(wfRowE[i], n.e ? "WPA" : "open");
      lv_obj_set_style_text_color(wfRowI[i], dkK(n.r > -60 ? DC_GRN : (n.r > -75 ? DC_YEL : DC_ORG)), 0);
      lv_obj_set_style_text_color(wfRowT[i], dkK(!strcmp(n.s, cfg.ssid) && WiFi.status() == WL_CONNECTED ? DC_CY : DC_TX), 0);
      lv_obj_clear_flag(wfRow[i], LV_OBJ_FLAG_HIDDEN);
    } else if (i == 0) {
      dkSet(wfRowT[0], wfScanning ? "Scanning..." : "No networks - tap Scan");
      dkSet(wfRowE[0], "");
      lv_obj_set_style_text_color(wfRowI[0], dkK(DC_MUT), 0);
      lv_obj_set_style_text_color(wfRowT[0], dkK(DC_MUT), 0);
      lv_obj_clear_flag(wfRow[0], LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(wfRow[i], LV_OBJ_FLAG_HIDDEN);
  }
}
static void dkWfScanStart() {
  WiFi.scanDelete();
  WiFi.scanNetworks(true);
  wfScanning = true;
  if (wfScanL) dkSet(wfScanL, "...");
  dkWfFill();
}
static void dkWfCollect() {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  wfScanning = false;
  wfN = 0;
  for (int i = 0; i < n && wfN < 24; i++) {
    String ss = WiFi.SSID(i);
    if (!ss.length() || ss.length() > 32) continue;
    int8_t r = (int8_t)constrain(WiFi.RSSI(i), -127, 0);
    int dup = -1;
    for (int k = 0; k < wfN; k++) if (!strcmp(wfNet[k].s, ss.c_str())) { dup = k; break; }
    if (dup >= 0) { if (r > wfNet[dup].r) wfNet[dup].r = r; continue; }
    strlcpy(wfNet[wfN].s, ss.c_str(), sizeof(wfNet[0].s));
    wfNet[wfN].r = r;
    wfNet[wfN].e = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    wfN++;
  }
  WiFi.scanDelete();
  for (int i = 1; i < wfN; i++) {                                      // strongest first
    DkNet t = wfNet[i];
    int j = i - 1;
    while (j >= 0 && wfNet[j].r < t.r) { wfNet[j + 1] = wfNet[j]; j--; }
    wfNet[j + 1] = t;
  }
  wfPage = 0;
  if (wfScanL) dkSet(wfScanL, "Scan");
  dkWfFill();
}
static void dkWfConnect(const char* ssid, const char* pass) {          // starts the hotspot too (if off) so the web panel stays reachable
  beginWifiTry(ssid, pass);
  wfPhase = 1;
  char m[48]; snprintf(m, sizeof(m), "Connecting to %s ...", ssid);
  dkWfMsg(m, DC_YEL, 20000);
}
static void dkWfDlgClose() { if (wfBox) { lv_obj_del(wfBox); wfBox = wfTa = wfKb = nullptr; } }
static void dkWfKbEvt(lv_event_t* e) {
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_READY) {
    char p[65];
    strlcpy(p, lv_textarea_get_text(wfTa), sizeof(p));
    dkWfDlgClose();
    dkWfConnect(wfSel, p);
  } else if (c == LV_EVENT_CANCEL) dkWfDlgClose();
}
static void dkWfCancelEvt(lv_event_t*) { dkWfDlgClose(); }
static void dkWfEyeEvt(lv_event_t*) { if (wfTa) lv_textarea_set_password_mode(wfTa, !lv_textarea_get_password_mode(wfTa)); }
static void dkWfDialog(const char* ssid) {
  lv_mem_monitor_t m;
  lv_mem_monitor(&m);
  if (m.free_biggest_size < 6000) { dkWfMsg("Low RAM: cannot open keyboard", DC_RED); return; }
  wfBox = dkBox(wfScr, 0, 0, LV_HOR, LV_VER, 0x0A1030, 0);
  lv_obj_add_flag(wfBox, LV_OBJ_FLAG_CLICKABLE);
  char t[64]; snprintf(t, sizeof(t), "Password for %s", ssid);
  lv_obj_t* tl = dkLbl(wfBox, t, &lv_font_montserrat_14, DC_TX);
  lv_obj_set_width(tl, 250);
  lv_label_set_long_mode(tl, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(tl, 10, 6);
  dkChipBtn(wfBox, 276, 3, 36, 22, LV_SYMBOL_CLOSE, dkWfCancelEvt, 0, nullptr, &lv_font_montserrat_14);
  wfTa = lv_textarea_create(wfBox);
  lv_obj_set_size(wfTa, 246, 34);
  lv_obj_set_pos(wfTa, 8, 28);
  lv_textarea_set_one_line(wfTa, true);
  lv_textarea_set_password_mode(wfTa, true);
  lv_textarea_set_max_length(wfTa, 63);
  lv_textarea_set_placeholder_text(wfTa, "Password");
  lv_obj_set_style_bg_color(wfTa, dkK(DC_CARD), 0);
  lv_obj_set_style_border_color(wfTa, dkK(DC_CY), 0);
  lv_obj_set_style_border_width(wfTa, 1, 0);
  lv_obj_set_style_radius(wfTa, 10, 0);
  lv_obj_set_style_text_color(wfTa, dkK(DC_TX), 0);
  lv_obj_set_style_text_font(wfTa, &lv_font_montserrat_14, 0);
  dkChipBtn(wfBox, 260, 28, 52, 34, LV_SYMBOL_EYE_OPEN, dkWfEyeEvt, 0, nullptr, &lv_font_montserrat_16);
  wfKb = lv_keyboard_create(wfBox);
  lv_obj_set_size(wfKb, LV_HOR, 172);
  lv_obj_align(wfKb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(wfKb, wfTa);
  lv_obj_set_style_bg_color(wfKb, dkK(0x0A1030), 0);
  lv_obj_set_style_border_width(wfKb, 0, 0);
  lv_obj_set_style_pad_all(wfKb, 3, 0);
  lv_obj_set_style_pad_row(wfKb, 3, 0);
  lv_obj_set_style_pad_column(wfKb, 3, 0);
  lv_obj_set_style_bg_color(wfKb, dkK(0x25306B), LV_PART_ITEMS);
  lv_obj_set_style_text_color(wfKb, dkK(DC_TX), LV_PART_ITEMS);
  lv_obj_set_style_radius(wfKb, 8, LV_PART_ITEMS);
  lv_obj_set_style_border_width(wfKb, 0, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(wfKb, dkK(DC_CY), (lv_style_selector_t)(LV_PART_ITEMS | LV_STATE_PRESSED));
  lv_obj_set_style_text_color(wfKb, dkK(0x06202B), (lv_style_selector_t)(LV_PART_ITEMS | LV_STATE_PRESSED));
  lv_obj_set_style_bg_color(wfKb, dkK(DC_VIO), (lv_style_selector_t)(LV_PART_ITEMS | LV_STATE_CHECKED));
  lv_obj_add_event_cb(wfKb, dkWfKbEvt, LV_EVENT_ALL, NULL);
}
static void dkWfRowEvt(lv_event_t* e) {
  if (wfBox || wfPhase) return;
  int idx = wfPage * DK_WROWS + (int)(intptr_t)lv_event_get_user_data(e);
  if (idx >= wfN) return;
  strlcpy(wfSel, wfNet[idx].s, sizeof(wfSel));
  bool conn = WiFi.status() == WL_CONNECTED;
  bool saved = !strcmp(wfSel, cfg.ssid);
  if (saved && conn) { dkWfMsg("Already connected", DC_GRN); return; }
  if (!wfNet[idx].e) dkWfConnect(wfSel, "");
  else if (saved && cfg.pass[0]) dkWfConnect(wfSel, cfg.pass);       // known network: reuse the saved password
  else dkWfDialog(wfSel);
}
static void dkWfNavEvt(lv_event_t* e) {
  int d = (int)(intptr_t)lv_event_get_user_data(e);
  int pages = wfN ? (wfN + DK_WROWS - 1) / DK_WROWS : 1;
  wfPage = (wfPage + d + pages) % pages;
  dkWfFill();
}
static void dkWfScanEvt(lv_event_t*) { if (!wfScanning && !wfPhase && !wfBox) dkWfScanStart(); }
static void dkWfHotEvt(lv_event_t* e) {
  lv_obj_t* sw = lv_event_get_target(e);
  bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
  if (on) { startAP(); dkWfMsg("Hotspot on - 192.168.4.1", DC_GRN, 3000); }
  else if (WiFi.status() == WL_CONNECTED) { stopAP(); dkWfMsg("Hotspot off", DC_MUT, 2000); }
  else { lv_obj_add_state(sw, LV_STATE_CHECKED); dkWfMsg("Connect to a Wi-Fi first", DC_ORG, 2500); }   // never switch off the only way in
}
static lv_obj_t* dkBuildWifi() {
  lv_obj_t* s = dkPageScreen();
  wfScr = s;
  dkHeader(s, "Wi-Fi", DC_GRN, 1);
  dkChipBtn(s, 244, 3, 68, 22, "Scan", dkWfScanEvt, 0, &wfScanL);
  lv_obj_t* st = dkCard(s, 8, 30, 304, 44);
  wfStatA = dkLbl(st, "", &lv_font_montserrat_14, DC_TX);
  lv_obj_set_width(wfStatA, 282); lv_label_set_long_mode(wfStatA, LV_LABEL_LONG_DOT); lv_obj_set_pos(wfStatA, 12, 5);
  wfStatB = dkLbl(st, "", &lv_font_montserrat_12, DC_MUT);
  lv_obj_set_width(wfStatB, 282); lv_label_set_long_mode(wfStatB, LV_LABEL_LONG_DOT); lv_obj_set_pos(wfStatB, 12, 25);
  lv_obj_t* hs = dkCard(s, 8, 80, 304, 36);
  lv_obj_t* l = dkLbl(hs, LV_SYMBOL_GPS "  Hotspot", &lv_font_montserrat_14, DC_TX);
  lv_obj_set_pos(l, 12, 2);
  l = dkLbl(hs, AP_NAME "   192.168.4.1   open", &lv_font_montserrat_12, DC_MUT);
  lv_obj_set_pos(l, 12, 19);
  wfHot = dkSwitch(hs, 250, 7, apMode, 0, dkWfHotEvt);
  for (int i = 0; i < DK_WROWS; i++) {
    wfRow[i] = dkCard(s, 8, 122 + i * 30, 304, 26);
    dkPressable(wfRow[i]);
    lv_obj_add_event_cb(wfRow[i], dkWfRowEvt, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    wfRowI[i] = dkLbl(wfRow[i], LV_SYMBOL_WIFI, &lv_font_montserrat_14, DC_GRN);
    lv_obj_set_pos(wfRowI[i], 10, 4);
    wfRowT[i] = dkLbl(wfRow[i], "", &lv_font_montserrat_14, DC_TX);
    lv_obj_set_width(wfRowT[i], 210); lv_label_set_long_mode(wfRowT[i], LV_LABEL_LONG_DOT); lv_obj_set_pos(wfRowT[i], 36, 4);
    wfRowE[i] = dkLbl(wfRow[i], "", &lv_font_montserrat_12, DC_MUT);
    lv_obj_set_width(wfRowE[i], 46); lv_obj_set_style_text_align(wfRowE[i], LV_TEXT_ALIGN_RIGHT, 0); lv_obj_set_pos(wfRowE[i], 250, 6);
  }
  dkChipBtn(s, 8, 212, 56, 26, LV_SYMBOL_LEFT, dkWfNavEvt, -1, nullptr, &lv_font_montserrat_14);
  wfPg = dkLbl(s, "1/1", &lv_font_montserrat_12, DC_MUT);
  lv_obj_set_width(wfPg, 44); lv_obj_set_style_text_align(wfPg, LV_TEXT_ALIGN_CENTER, 0); lv_obj_set_pos(wfPg, 68, 219);
  dkChipBtn(s, 116, 212, 56, 26, LV_SYMBOL_RIGHT, dkWfNavEvt, 1, nullptr, &lv_font_montserrat_14);
  l = dkLbl(s, "Tap a network to connect", &lv_font_montserrat_12, DC_DIM);
  lv_obj_set_pos(l, 184, 219);
  wfPhase = 0; wfMsg[0] = 0; wfLastUi = 0;
  dkWfFill();
  if (WiFi.softAPgetStationNum() == 0) dkWfScanStart();                // a scan makes the hotspot hop channels: skip it while a phone is on the hotspot
  return s;
}
static void dkWfTick() {
  if (wfScanning) dkWfCollect();
  if (wfPhase) {
    if (wifiTry == WS_OK) { wfPhase = 0; dkWfMsg("Connected!", DC_GRN, 3000); dkWfFill(); }
    else if (wifiTry == WS_FAIL) { wfPhase = 0; dkWfMsg("Failed - check the password", DC_RED, 3500); }
  }
  uint32_t n = millis();
  if (n - wfLastUi < 500) return;
  wfLastUi = n;
  bool c = WiFi.status() == WL_CONNECTED;
  char a[48], b[72];
  if (c) {
    snprintf(a, sizeof(a), LV_SYMBOL_WIFI "  %s", WiFi.SSID().c_str());
    snprintf(b, sizeof(b), "%s    %d dBm", WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
  } else {
    strcpy(a, LV_SYMBOL_WARNING "  Not connected");
    snprintf(b, sizeof(b), apMode ? "Join %s, open 192.168.4.1" : "Pick a network below", AP_NAME);
  }
  dkSet(wfStatA, a);
  lv_obj_set_style_text_color(wfStatA, dkK(c ? DC_GRN : DC_ORG), 0);
  if (wfMsg[0] && n < wfMsgUntil) { dkSet(wfStatB, wfMsg); lv_obj_set_style_text_color(wfStatB, dkK(wfMsgCol), 0); }
  else { wfMsg[0] = 0; dkSet(wfStatB, b); lv_obj_set_style_text_color(wfStatB, dkK(DC_MUT), 0); }
  if (wfHot && lv_obj_has_state(wfHot, LV_STATE_CHECKED) != apMode) { if (apMode) lv_obj_add_state(wfHot, LV_STATE_CHECKED); else lv_obj_clear_state(wfHot, LV_STATE_CHECKED); }
}

// =====================================================================
//  STANDBY clock (optional SD slideshow on top). Tap = home.
// =====================================================================
static lv_obj_t *sbTime = nullptr, *sbDate = nullptr, *sbInfo = nullptr;
static void dkStbyEvt(lv_event_t*) { dkShowDeck(); }
static void dkStbyUpdate() {
  if (dkCur != DK_STBY || !sbTime) return;
  struct tm t;
  char b[32];
  if (dkNow(t)) {
    dkFmtTime(b, sizeof(b), t, true, false); dkSet(sbTime, b);                 // big clock: h:MM:SS / HH:MM:SS (per web panel)
    char dt[32];
    strftime(dt, sizeof(dt), "%a, %d %b %Y", &t);
    if (clock12) snprintf(b, sizeof(b), "%s   %s", t.tm_hour >= 12 ? "PM" : "AM", dt); else snprintf(b, sizeof(b), "%s", dt);
    dkSet(sbDate, b);
  } else { dkSet(sbTime, "--:--"); dkSet(sbDate, "waiting for time sync"); }
  char ip[48];
  snprintf(ip, sizeof(ip), "%s%s", myIp().c_str(), pcOnline ? "   PC online" : "");
  dkSet(sbInfo, ip);
}
static lv_obj_t* dkBuildStby() {
  lv_obj_t* s = dkScreen(0x2A1F66, 0x000000);
  lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s, dkStbyEvt, LV_EVENT_CLICKED, NULL);
  sbTime = dkLbl(s, "--:--", &lv_font_montserrat_48, DC_TX);
  lv_obj_set_width(sbTime, 300);
  lv_obj_set_style_text_align(sbTime, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(sbTime, LV_ALIGN_CENTER, 0, -24);
  dkBoxC(s, 130, 128, 60, 3, dkAccent(), 1);
  sbDate = dkLbl(s, "", &lv_font_montserrat_16, DC_PNK);
  lv_obj_set_width(sbDate, 300);
  lv_obj_set_style_text_align(sbDate, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(sbDate, LV_ALIGN_CENTER, 0, 28);
  sbInfo = dkLbl(s, "", &lv_font_montserrat_12, DC_DIM);
  lv_obj_set_width(sbInfo, 300);
  lv_obj_set_style_text_align(sbInfo, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(sbInfo, LV_ALIGN_BOTTOM_MID, 0, -12);
  return s;
}

// =====================================================================
//  NAVIGATION
// =====================================================================
static void dkLeave() {                                                // called before the next screen is built
  if (dkFlashT) { lv_timer_del(dkFlashT); dkFlashT = nullptr; }
  if (dkToastT) { lv_timer_del(dkToastT); dkToastT = nullptr; }
  if (dkErrT) { lv_timer_del(dkErrT); dkErrT = nullptr; }
  dkToast = dkToastL = dkPick = nullptr;
  dkFlashIdx = -1; dkConfIdx = -1;
  if (dkCur == DK_DECK) {
    dkFreeOld();                                                       // the old screen may still be sliding out: free its icons a bit later
    for (int i = 0; i < RMT_PER; i++) { dkIcoOld[i] = dkIcoBuf[i]; dkIcoBuf[i] = nullptr; dkKeyObj[i] = nullptr; dkKeyLbl[i] = nullptr; }
    dkLeaveAt = millis() ? millis() : 1;
    dkGrid = dkPill = dkPillL = dkDots = dkWifiL = dkClkL = nullptr;
  }
  if (dkCur == DK_CTRL) { ctMsg = ctHotT = seBrV = nullptr; }
  if (dkCur == DK_REEL) std::vector<String>().swap(dkNames);          // give the memory back
  if (dkCur == DK_SET) { saveCfg(); dkPrefSave(); seBrV = nullptr; }
  if (dkCur == DK_STBY) sbTime = sbDate = sbInfo = nullptr;
  if (dkCur == DK_WIFI) { wfBox = wfTa = wfKb = nullptr; wfStatA = wfStatB = wfPg = wfHot = wfScanL = nullptr; wfPhase = 0; }
}
static lv_obj_t* dkBuild(DkScr s) {
  switch (s) {
    case DK_CTRL: return dkBuildCtrl();
    case DK_REEL: return dkBuildReel();
    case DK_SET:  return dkBuildSet();
    case DK_STBY: return dkBuildStby();
    case DK_WIFI: return dkBuildWifi();
    default:      return dkBuildDeck();
  }
}
static const char* dkScrName() {
  switch (dkCur) {
    case DK_DECK: return "rmt";    case DK_REEL: return "gal";      // same ids as the web panel app buttons
    case DK_SET:  return "set";    case DK_STBY: return "clk";
    case DK_WIFI: return "wifi";   case DK_CTRL: return "ctrl";
    default: return "?";
  }
}
// Open a screen. Normally the new screen slides over the old one (both exist for ~0.2 s). When the LVGL pool is too small to hold two
// screens the old one is deleted first (no animation): a screen that cannot be built silently was the old "buttons do nothing" problem.
static void dkOpen(DkScr to, lv_scr_load_anim_t anim, uint32_t t = 220) {
  if (to == dkCur) return;
  if (ESP.getFreeHeap() < 9000) { Serial.printf("ui: refused %d, heap=%u\n", (int)to, (unsigned)ESP.getFreeHeap()); return; }
  dkLeave();
  lv_obj_t* old = lv_scr_act();
  lv_mem_monitor_t m;
  lv_mem_monitor(&m);
  bool tight = m.free_biggest_size < 16000;
  lv_obj_t* tmp = nullptr;
  if (tight) { tmp = dkScreen(0x000000); lv_scr_load(tmp); lv_obj_del(old); dkFreeOld(); }
  dkCur = to;
  lv_obj_t* s = dkBuild(to);
  if (tight) { lv_scr_load(s); lv_obj_del(tmp); } else dkGo(s, anim, t);
  lv_mem_monitor(&m);
  Serial.printf("ui: %s%s  heap=%u  lvgl free=%u biggest=%u\n", dkScrName(), tight ? " (swap)" : "", (unsigned)ESP.getFreeHeap(), (unsigned)m.free_size, (unsigned)m.free_biggest_size);
}
static void dkShowDeck() { DkScr f = dkCur; dkOpen(DK_DECK, f == DK_STBY ? LV_SCR_LOAD_ANIM_OUT_BOTTOM : (f == DK_CTRL ? LV_SCR_LOAD_ANIM_OUT_TOP : LV_SCR_LOAD_ANIM_OVER_RIGHT), 240); }
static void dkShowCtrl() { dkOpen(DK_CTRL, LV_SCR_LOAD_ANIM_OVER_BOTTOM, 240); }
static void dkShowReel() { dkOpen(DK_REEL, LV_SCR_LOAD_ANIM_OVER_LEFT); }
static void dkShowSet()  { dkOpen(DK_SET, LV_SCR_LOAD_ANIM_OVER_LEFT); }
static void dkShowWifi() { dkOpen(DK_WIFI, LV_SCR_LOAD_ANIM_OVER_LEFT); }
static void dkShowStby(bool idle) {
  if (dkCur == DK_STBY) return;
  dkOpen(DK_STBY, LV_SCR_LOAD_ANIM_OVER_TOP, 240);
  if (dkCur != DK_STBY) return;
  dkStbyUpdate();
  if (idle && dkAmbient && sdOk) { dkPendAmb = true; dkPendWhen = millis() + 320; }   // ambient photo frame / video: starts after the 240 ms open animation (otherwise LVGL leftovers show around it)
}

// =====================================================================
//  ENTRY POINTS used by main.cpp
// =====================================================================
static bool dkWebCmd(const char* cmd, const char* arg) {               // web panel buttons (runs on the UI task, LVGL calls are safe)
  gLastAct = millis();
  if (gAsleep && strcmp(cmd, "sleep")) { gAsleep = false; applyBrightness(); }
  if (!strcmp(cmd, "wake")) return true;
  if (!strcmp(cmd, "redraw")) { lv_obj_invalidate(lv_scr_act()); lv_obj_invalidate(lv_layer_top()); return true; }
  if (!strcmp(cmd, "stop")) { if (curPage == P_MEDIA) dkMediaLeave(); return true; }
  if (!strcmp(cmd, "mnext") || !strcmp(cmd, "mprev")) { if (curPage == P_MEDIA) dkStepMedia(cmd[1] == 'n' ? 1 : -1); return true; }
  if (curPage == P_MEDIA) dkMediaLeave();                              // any other command first leaves the player
  if (!strcmp(cmd, "home")) { dkShowDeck(); return true; }
  if (!strcmp(cmd, "lock")) { dkShowStby(true); return true; }          // lock = standby screen (+ slideshow/video when enabled)
  if (!strcmp(cmd, "shade")) { dkShowCtrl(); return dkCur == DK_CTRL; }
  if (!strcmp(cmd, "sleep")) { gAsleep = true; applyBrightness(); return true; }
  if (!strcmp(cmd, "open")) {
    if (!strcmp(arg, "rmt") || !strcmp(arg, "deck")) { dkShowDeck(); return dkCur == DK_DECK; }
    if (!strcmp(arg, "gal") || !strcmp(arg, "reel"))  { dkShowReel(); return dkCur == DK_REEL; }
    if (!strcmp(arg, "set")) { dkShowSet(); return dkCur == DK_SET; }
    if (!strcmp(arg, "wifi") || !strcmp(arg, "info")) { dkShowWifi(); return dkCur == DK_WIFI; }
    if (!strcmp(arg, "ctrl")) { dkShowCtrl(); return dkCur == DK_CTRL; }
    if (!strcmp(arg, "clk")) { dkShowStby(false); return dkCur == DK_STBY; }
    return false;
  }
  return false;
}
static void dkPump() {                                                 // called by the web server while a big upload is received
  if (curPage == P_MEDIA || !dkDrv.draw_buf) return;
  dkTickInc();
  lv_timer_handler();
}

static bool dkPre = false;
static size_t dkBufPx = 0;
// Called from setup() BEFORE WiFi starts: the LVGL pool (LV_MEM_KB, taken from the heap inside lv_init) and the draw buffers are
// reserved while the heap is still in one piece, so LVGL can never run out of memory later because WiFi / the web server grew.
#define LVUI_PREINIT 1
static void lvUiPreInit() {
  if (dkPre) return;
  dkPre = true;
  lv_init();
  size_t lines = 10;
  for (; lines >= 4; lines -= 2) {
    dkBufPx = LV_HOR * lines;
    dkBuf1 = (lv_color_t*)heap_caps_malloc(dkBufPx * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (dkBuf1) break;
  }
#ifndef PH_ONE_BUF   // -DPH_ONE_BUF = single draw buffer (saves ~6 KB RAM), the env cyd_lvgl already sets it
  if (dkBuf1) dkBuf2 = (lv_color_t*)heap_caps_malloc(dkBufPx * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
#endif
  lv_mem_monitor_t m;
  lv_mem_monitor(&m);
  Serial.printf("LVGL: pool %u KB (free %u), draw buffers %u px x%d, heap=%u maxblock=%u\n", (unsigned)(m.total_size / 1024), (unsigned)m.free_size,
                (unsigned)dkBufPx, dkBuf2 ? 2 : (dkBuf1 ? 1 : 0), (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
}

static void lvUiInit() {
  lvUiPreInit();                                                       // no-op when setup() already did it
  if (!dkBuf1) { Serial.println("FATAL: no RAM for the LVGL draw buffer -> restart"); delay(300); ESP.restart(); }
  lv_disp_draw_buf_init(&dkDraw, dkBuf1, dkBuf2, dkBufPx);
  lv_disp_drv_init(&dkDrv);
  dkDrv.hor_res = LV_HOR; dkDrv.ver_res = LV_VER;
  dkDrv.flush_cb = dkFlush; dkDrv.draw_buf = &dkDraw;
  lv_disp_drv_register(&dkDrv);
  lv_indev_drv_init(&dkIn);
  dkIn.type = LV_INDEV_TYPE_POINTER;
  dkIn.read_cb = dkTouchRead;
  dkIn.scroll_throw = 10;
  dkIn.scroll_limit = 8;
  dkIn.gesture_limit = 28;                                             // short swipes count on a small resistive panel
  dkIn.gesture_min_velocity = 2;
  lv_indev_drv_register(&dkIn);

  dkLastTick = millis();
  uiCmdHook = dkWebCmd; uiScrHook = dkScrName; uiPumpHook = dkPump;
  configTzTime(DK_TZ, "pool.ntp.org", "time.google.com");
  dkPrefLoad();
  dkStylesInit();
  gLastAct = millis();
  lv_obj_t* old = lv_scr_act();
  dkCur = DK_DECK;
  lv_obj_t* s = dkBuildDeck();
  lv_scr_load(s);
  lv_obj_del(old);
  Serial.printf("Deck UI ready: heap=%u maxblock=%u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
}

// ---- auto profile: the active PC window / app name contains a profile name ----
static void dkAutoProfile() {
  if (!dkAuto || dkCur != DK_DECK || rmtTabs < 2 || dkPick) return;
  const char* app = S.wapp[0] ? S.wapp : S.win;
  if (!app[0] || !strcmp(app, dkLastApp)) return;
  strlcpy(dkLastApp, app, sizeof(dkLastApp));
  char hay[96];
  snprintf(hay, sizeof(hay), "%s %s", S.wapp, S.win);
  for (char* p = hay; *p; p++) *p = (char)tolower((unsigned char)*p);
  for (int t = 0; t < rmtTabs; t++) {
    char n[12];
    strlcpy(n, rmtTab[t].name, sizeof(n));
    if (strlen(n) < 3) continue;
    for (char* p = n; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (!strstr(hay, n)) continue;
    if (t != rmtCur) {
      dkSp = 0;
      dkSetProfile(t, t > rmtCur ? 1 : -1);
      char m[28]; snprintf(m, sizeof(m), "Auto: %s", rmtTab[t].name);
      dkToastShow(m, DC_CY);
    }
    return;
  }
}

// ---- per-loop work ----
static void dkTick() {
  if (dkCur == DK_WIFI) dkWfTick();
  if (!dkSendBusy && dkSendRes >= 0) {                                 // answer of the background send task
    int r = dkSendRes; dkSendRes = -1;
    dkTaskLast = millis();
    if (dkCur == DK_DECK) {
      if (!dkSendQuick) dkFlash(dkSendIdx, r == 1 ? DC_GRN : DC_RED);
      if (r == 2) dkToastShow("Error: wrong token", DC_RED, 2200);
      else if (r != 1) dkToastShow("PC not reachable", DC_RED, 2200);
    } else if (dkCur == DK_CTRL && ctMsg) {
      dkSet(ctMsg, r == 1 ? "Sent" : (r == 2 ? "Wrong token" : "PC not reachable"));
      lv_obj_set_style_text_color(ctMsg, dkK(r == 1 ? DC_GRN : DC_RED), 0);
    }
  }
  if (dkTask && !dkSendBusy && millis() - dkTaskLast > 6000) { vTaskDelete(dkTask); dkTask = nullptr; }   // free the 6 KB stack while idle
  if (dkCur == DK_DECK) {
    if (gLvRmtDirty) { gLvRmtDirty = false; dkSp = 0; dkBuildKeys(0); dkTopUpdate(); }     // the web panel saved new profiles
    if (dkConfIdx >= 0 && millis() > dkConfUntil) dkConfClear();
  } else gLvRmtDirty = false;
  if (dkLeaveAt && millis() - dkLeaveAt > 600) { dkLeaveAt = 0; dkFreeOld(); }
}

// Full-screen player (SD video / GIF / photo). Swipe left / right = next / previous file, tap = back to the UI.
// The finger that started the player is ignored until it is lifted. Exit happens on RELEASE, so no click leaks into the UI underneath.
static bool dkWasMedia = false;
// ---- lock-screen control strip: drawn straight on the panel over the (frozen) picture. Large buttons: back | play | next | vol- | vol+ | unlock ----
static void dkStripDraw() {
  tft.waitDMA();
  const uint16_t BAR = 0x10A2, BTN = 0x3A09, RED = 0xB0A3;
  static const char* lab[6] = {"<<", ">", ">>", "-", "+", "X"};
  tft.fillRect(0, 248, 480, 72, BAR);                                  // logical 480x320 coordinates: this is the bottom 54 px of the real panel
  char b[24];
  snprintf(b, sizeof(b), "Volume %d%%", (int)audioVolGet());
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, BAR);
  tft.drawString(b, 240, 258, 2);
  for (int i = 0; i < 6; i++) {
    tft.fillRect(i * 80 + 4, 272, 72, 42, i == 5 ? RED : BTN);
    tft.drawRect(i * 80 + 4, 272, 72, 42, TFT_LIGHTGREY);
    tft.setTextColor(TFT_WHITE, i == 5 ? RED : BTN);
    tft.drawString(lab[i], i * 80 + 40, 293, 4);
  }
}
static void dkStripShow() {
  dkStrip = true; dkStripAt = millis();
  lightPause(true);
  dkStripDraw();
}
static void dkStripHide() {                                            // video: the next frame wipes the strip. Everything else is played again from its start
  dkStrip = false;
  lightPause(false);
  if (liveType != LT_CLIP || !lightKey) { String n = liveName; if (n.length()) sdPlayFile(n); }
}
static void dkStripTap(int x, int y) {
  dkStripAt = millis();
  if (y < 196) { dkStripHide(); return; }                              // tap on the picture: continue
  int i = constrain(x * 6 / 320, 0, 5);
  switch (i) {
    case 0: dkStrip = false; dkStepMedia(-1); break;
    case 1: dkStripHide(); break;
    case 2: dkStrip = false; dkStepMedia(1); break;
    case 3: audioVolSet((int)audioVolGet() - 10); dkStripDraw(); break;
    case 4: audioVolSet((int)audioVolGet() + 10); dkStripDraw(); break;
    case 5: dkMediaLeave(); break;                                     // unlock: back to the clock screen
  }
}

// ---- physical BOOT button (GPIO0): short = next picture / lock <-> unlock, hold 1 s = leave the player ----
static void dkBootBtn() {
  static uint32_t t0 = 0;
  static bool longDone = false;
  bool dn = digitalRead(BTN_PIN) == LOW;
  uint32_t now = millis();
  if (dn) {
    if (!t0) { t0 = now; longDone = false; }
    else if (!longDone && now - t0 > 1000 && now - t0 < 8000) {
      longDone = true;
      gLastAct = now; gAsleep = false;
      if (curPage == P_MEDIA) dkMediaLeave();
    }
  } else if (t0) {
    uint32_t held = now - t0;
    t0 = 0;
    if (longDone || held < 40) return;
    gLastAct = now;
    if (gAsleep) { gAsleep = false; return; }                          // first press only wakes the screen
    if (curPage == P_MEDIA) { dkStrip = false; dkStepMedia(1); }
    else if (dkCur == DK_STBY) dkShowDeck();
    else if (dkCur == DK_DECK) dkShowStby(true);
  }
}

static void dkMediaTick() {
  if (dkStrip) { stillUntil = stillUntil ? millis() + 4000 : 0; if (millis() - dkStripAt > 6000) dkStripHide(); }   // strip open: nothing advances
  mediaService();
  if (curPage != P_MEDIA) return;
  static bool down = false;
  static uint32_t lastSeen = 0;
  static int x0 = 0, y0 = 0, xl = 0, yl = 0;
  if (!dkWasMedia) { dkMediaArmed = false; down = false; lastSeen = millis(); tft.waitDMA(); }
  dkWasMedia = true;
  if (lightPath.length()) lightTick();
  else if (liveType == LT_GIF) gifTick();
  else if (liveType == LT_CLIP || liveType == LT_STREAM) clipTick();
  int rx, ry;
  bool t = touchRaw(rx, ry);
  uint32_t now = millis();
  if (t) lastSeen = now;
  bool released = !t && (now - lastSeen > 50);                         // XPT2046 flickers while pressed: 50 ms without contact = finger up
  if (!dkMediaArmed) {
    if (released) { dkMediaArmed = true; down = false; }
  } else if (t) {
    int x, y;
    dkMapTouch(rx, ry, x, y);
    if (!down) { down = true; x0 = x; y0 = y; }
    xl = x; yl = y;
  } else if (down && released) {
    down = false;
    int dx = xl - x0, dy = yl - y0;
    if (abs(dx) >= 50 && abs(dx) > abs(dy)) dkStepMedia(dx < 0 ? 1 : -1);
    else if (abs(dx) < 35 && abs(dy) < 35) {
      if (dkAmbientRun) { if (dkStrip) dkStripTap(xl, yl); else dkStripShow(); }   // lock screen: tap = control strip (so the buttons stay reachable)
      else dkMediaLeave();                                             // tap: back to the UI
    }
  }
  delay(1);
}

static void lvUiTick() {
  dkTickInc();
  dkBootBtn();
  gFastPoll = dkAuto && dkCur == DK_DECK && !gAsleep && curPage != P_MEDIA;   // the PC is polled every 3 s only while the home screen needs the active window (auto profile)
  if ((dkPendIdx >= 0 || dkPendAll || dkPendAmb) && (int32_t)(millis() - dkPendWhen) >= 0) dkRunPending();   // queued by a Reel tap (see dkRowEvt)
  if (curPage == P_MEDIA) { dkMediaTick(); return; }
  if (dkWasMedia) { dkWasMedia = false; lv_obj_invalidate(lv_scr_act()); lv_obj_invalidate(lv_layer_top()); }
  mediaService();

  if (newData) {                                                       // poll data still arrives (needed for AUTO PROFILE); no monitor screens
    newData = false;
    takeSnapshot();
    dkAutoProfile();
  }
  dkTick();

  static uint32_t t1 = 0;
  if (millis() - t1 > 1000) {
    t1 = millis();
    if (gIdleMin && !gAsleep && millis() - gLastAct > (uint32_t)gIdleMin * 60000UL) gAsleep = true;
    applyBrightness();
    if (dkStbySecs && !gAsleep && dkCur == DK_DECK && !dkPick && millis() - gLastAct > (uint32_t)dkStbySecs * 1000UL) dkShowStby(true);
    dkStatus();
    dkStbyUpdate();
    if (dkCur == DK_SET && seNet) dkSetNet();
    if (dkCur == DK_CTRL) dkCtHotDraw();
    lv_mem_monitor_t lvm;
    lv_mem_monitor(&lvm);
    if ((ESP.getFreeHeap() < 12000 || lvm.free_biggest_size < 4000) && dkCur != DK_DECK && dkCur != DK_STBY) {   // last resort before LVGL runs out of memory
      Serial.printf("LOW RAM recovery: free=%u maxblock=%u lvgl free=%u biggest=%u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(), (unsigned)lvm.free_size, (unsigned)lvm.free_biggest_size);
      dkShowDeck();
    }
    static bool apShown = false;                                       // first-run WiFi setup: jump to the Wi-Fi screen
    if (apMode && WiFi.status() != WL_CONNECTED) {
      if (!apShown && dkCur == DK_DECK) { apShown = true; dkShowWifi(); }
    } else apShown = false;
  }
  static uint32_t lastHeap = 0;
  if (millis() - lastHeap > 10000) {
    lastHeap = millis();
    lv_mem_monitor_t lvm;
    lv_mem_monitor(&lvm);
    Serial.printf("heap=%u maxblock=%u wifi=%d uiStackFree=%uB lvgl: used %u%% free=%u biggest=%u frag=%u%% scr=%s\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(), (int)WiFi.status(),
                  (unsigned)uxTaskGetStackHighWaterMark(NULL), (unsigned)lvm.used_pct, (unsigned)lvm.free_size, (unsigned)lvm.free_biggest_size, (unsigned)lvm.frag_pct, dkScrName());
  }

  uint32_t wait = lv_timer_handler();
  delay(wait > 6 ? 6 : (wait ? wait : 1));
}
#endif  // USE_LVGL
