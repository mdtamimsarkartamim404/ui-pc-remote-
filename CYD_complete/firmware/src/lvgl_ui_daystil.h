// "Days til" countdown app for the CYD (320x240, no PSRAM), LVGL 8.3.  Built only with -DUSE_LVGL (env:cyd_lvgl).
// Drop-in replacement for lvgl_ui.h: main.cpp keeps calling lvUiInit() / lvUiTick().
//
//  Screens (all slide / fade into each other):
//   SPLASH   "days til" + analog clock (shows the real time)
//   EVENTS   vertical list of event cards, snap scrolling, dot indicator, "+" = new event
//   COUNT    icon + "Days til <name>!" + two big numbers (Days:Hours | Hours:Minutes | Minutes:Seconds)
//   PARTY    "<name> is here!" + ringing clock + confetti (when the countdown reaches zero)
//   SETTINGS title (on-screen keyboard), icon picker, date, time, notify, yearly, display unit, save / delete
//
//  Time comes from NTP over the WiFi that main.cpp already connects (timezone below, default Asia/Dhaka).
//  Events are stored in NVS (namespace "dtil"). Web panel / PC agent are NOT needed by this UI.
//  Bigger digits: enable LV_FONT_MONTSERRAT_28 in lv_conf.h and build with -DDT_FONT_BIG="(&lv_font_montserrat_28)".
#pragma once
#ifdef USE_LVGL
#include <lvgl.h>
#include <time.h>
#include <Preferences.h>

#define LV_HOR 320
#define LV_VER 240

#ifndef DT_TZ
#define DT_TZ "<+06>-6"                          // POSIX TZ string. Dhaka = UTC+6
#endif
#ifndef DT_FONT_BIG
#define DT_FONT_BIG (&lv_font_montserrat_20)
#endif

// ---- palette (matches the mock-up: pale sky blue, white cards, deep navy ink) ----
#define DC_INK   0x25235F
#define DC_MUT   0xB4B9C6
#define DC_TILE  0xF2F4F9
#define DC_WHITE 0xFFFFFF
#define DC_SKY1  0xC3DAF0
#define DC_SKY2  0x8DB2D6
#define DC_RED   0xE5484D
static inline lv_color_t K(uint32_t h) { return lv_color_hex(h); }

// ---------------- display + touch drivers ----------------
static lv_disp_draw_buf_t dtDraw;
static lv_disp_drv_t dtDrv;
static lv_indev_drv_t dtIn;
static lv_color_t *dtBuf1 = nullptr, *dtBuf2 = nullptr;

static void dtFlush(lv_disp_drv_t* d, const lv_area_t* a, lv_color_t* c) {
  int32_t w = a->x2 - a->x1 + 1, h = a->y2 - a->y1 + 1;
  tft.waitDMA();
  tft.pushImageDMA(a->x1, a->y1, w, h, (const lgfx::rgb565_t*)c);
  lv_disp_flush_ready(d);
}

static void dtTouchRead(lv_indev_drv_t*, lv_indev_data_t* d) {
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
      if (gAsleep) { gAsleep = false; swallow = true; applyBrightness(); }
#ifdef TOUCH_SWAP_XY
      { int t = rx; rx = ry; ry = t; }
#endif
      int x, y;
      touchToScreen(rx, ry, x, y);
#ifdef TOUCH_INV_X
      x = LV_HOR - 1 - x;
#endif
#ifdef TOUCH_INV_Y
      y = LV_VER - 1 - y;
#endif
      if (cfg.rotation == 3) { x = LV_HOR - 1 - x; y = LV_VER - 1 - y; }
      lx = constrain(x, 0, LV_HOR - 1); ly = constrain(y, 0, LV_VER - 1);
      miss = 0; pressed = true;
    } else if (pressed && ++miss >= 4) { pressed = false; swallow = false; }
  }
  d->point.x = lx; d->point.y = ly;
  d->state = (pressed && !swallow) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// =====================================================================
//  EVENT DATA (NVS)
// =====================================================================
#define DT_MAX 8
struct DtEv { char name[16]; uint8_t icon, mode, notify, rep, mo, d, h, mi; uint16_t y; };   // mode 0 auto,1 d:h,2 h:m,3 m:s ; rep = every year
static DtEv dtEv[DT_MAX];
static int dtN = 0;
static const char* const DT_SUB[6] = {"Holiday", "Vacation", "Birthday", "Gift", "Winter", "Event"};

static void dtSave() {
  Preferences p;
  if (p.begin("dtil", false)) { p.putUChar("n", (uint8_t)dtN); p.putBytes("ev", dtEv, sizeof(dtEv)); p.end(); }
}
static void dtAddDef(const char* nm, int ic, int y, int mo, int d, int rep) {
  if (dtN >= DT_MAX) return;
  DtEv& e = dtEv[dtN++];
  memset(&e, 0, sizeof(e));
  strlcpy(e.name, nm, sizeof(e.name));
  e.icon = ic; e.y = y; e.mo = mo; e.d = d; e.rep = rep; e.notify = 1;
}
static void dtLoad() {
  Preferences p;
  dtN = 0;
  memset(dtEv, 0, sizeof(dtEv));
  if (p.begin("dtil", true)) {
    int n = p.getUChar("n", 0);
    if (n > 0 && n <= DT_MAX && p.getBytesLength("ev") == sizeof(dtEv)) { p.getBytes("ev", dtEv, sizeof(dtEv)); dtN = n; }
    p.end();
  }
  if (dtN == 0) {                                  // first boot: sample events (edit / delete them in Settings)
    dtAddDef("Christmas", 0, 2026, 12, 25, 1);
    dtAddDef("New Year", 5, 2027, 1, 1, 1);
    dtAddDef("My Trip", 1, 2027, 1, 15, 0);
    dtSave();
  }
}

// =====================================================================
//  TIME
// =====================================================================
static bool dtNow(time_t& t) { t = time(nullptr); return t > 1700000000; }
static time_t dtMk(int y, const DtEv& e) {
  struct tm t = {};
  t.tm_year = y - 1900; t.tm_mon = e.mo - 1; t.tm_mday = e.d; t.tm_hour = e.h; t.tm_min = e.mi; t.tm_isdst = -1;
  return mktime(&t);
}
static time_t dtTarget(const DtEv& e, time_t now) {
  int y = e.y;
  time_t tt = dtMk(y, e);
  if (e.rep) for (int g = 0; tt < now - 86400 && g < 200; g++) tt = dtMk(++y, e);   // yearly events roll to the next date
  return tt;
}
static void dtClockTime(int& h, int& m) {
  time_t t;
  if (dtNow(t)) { struct tm tmv; localtime_r(&t, &tmv); h = tmv.tm_hour % 12; m = tmv.tm_min; }
  else { h = 10; m = 10; }
}

// =====================================================================
//  ICONS: drawn once into small ARGB canvases (no emoji fonts needed). 32x32, 3 KB each
// =====================================================================
#define DT_IC 32
#define DT_IC_N 6
static uint8_t* dtIcBuf[DT_IC_N];
static lv_img_dsc_t dtIcDsc[DT_IC_N];

static void dtR(lv_obj_t* cv, int x, int y, int w, int h, uint32_t col, int r = 0, uint32_t bc = 0, int bw = 0) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = K(col); d.bg_opa = LV_OPA_COVER; d.radius = r;
  if (bw) { d.border_color = K(bc); d.border_width = bw; d.border_opa = LV_OPA_COVER; }
  lv_canvas_draw_rect(cv, x, y, w, h, &d);
}
static void dtP(lv_obj_t* cv, const lv_point_t* p, int n, uint32_t col) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = K(col); d.bg_opa = LV_OPA_COVER;
  lv_canvas_draw_polygon(cv, p, n, &d);
}
static void dtDrawIcon(lv_obj_t* cv, int i) {
  if (i == 0) {                                                       // christmas tree
    lv_point_t a[] = {{16, 3}, {8, 13}, {24, 13}}, b[] = {{16, 8}, {6, 21}, {26, 21}}, c[] = {{16, 14}, {3, 28}, {29, 28}};
    dtR(cv, 14, 25, 4, 6, 0x8B5A2B);
    dtP(cv, c, 3, 0x2E7D32); dtP(cv, b, 3, 0x388E3C); dtP(cv, a, 3, 0x4CAF50);
    dtR(cv, 14, 0, 4, 4, 0xFFC53D, 2);
    dtR(cv, 9, 20, 3, 3, 0xE5484D, 2); dtR(cv, 20, 24, 3, 3, 0xFFC53D, 2); dtR(cv, 15, 15, 3, 3, 0xE5484D, 2);
  } else if (i == 1) {                                                // airplane (top view)
    lv_point_t w[] = {{16, 12}, {2, 21}, {2, 24}, {16, 19}, {30, 24}, {30, 21}};
    lv_point_t t[] = {{16, 24}, {9, 30}, {9, 31}, {16, 29}, {23, 31}, {23, 30}};
    dtP(cv, w, 6, 0x2D9CDB); dtP(cv, t, 6, 0x2D9CDB);
    dtR(cv, 13, 1, 6, 30, 0x4FB3E8, 3);
    dtR(cv, 14, 4, 4, 4, 0xFFFFFF, 2);
  } else if (i == 2) {                                                // cake slice
    dtR(cv, 3, 15, 26, 13, 0xFFD76B, 3);
    dtR(cv, 3, 14, 26, 5, 0xFFFFFF, 3, 0xF0E6D0, 1);
    dtR(cv, 3, 22, 26, 2, 0xE5484D);
    dtR(cv, 18, 5, 8, 8, 0xE5484D, LV_RADIUS_CIRCLE);
    dtR(cv, 21, 2, 2, 5, 0x3C8D40);
  } else if (i == 3) {                                                // gift
    dtR(cv, 5, 14, 22, 16, 0xE5484D, 3);
    dtR(cv, 3, 9, 26, 7, 0xC73A3F, 3);
    dtR(cv, 14, 9, 4, 21, 0xFFC53D);
    dtR(cv, 9, 2, 8, 8, 0xFFC53D, LV_RADIUS_CIRCLE); dtR(cv, 15, 2, 8, 8, 0xFFC53D, LV_RADIUS_CIRCLE);
    dtR(cv, 13, 6, 6, 5, 0xE0A91F, 2);
  } else if (i == 4) {                                                // snowman
    dtR(cv, 6, 14, 20, 18, 0xFFFFFF, LV_RADIUS_CIRCLE, 0xC9D6E8, 1);
    dtR(cv, 10, 2, 12, 12, 0xFFFFFF, LV_RADIUS_CIRCLE, 0xC9D6E8, 1);
    dtR(cv, 13, 6, 2, 2, 0x222222, 1); dtR(cv, 18, 6, 2, 2, 0x222222, 1);
    dtR(cv, 15, 9, 6, 2, 0xFF8A3D, 1);
    dtR(cv, 14, 19, 3, 3, 0x444444, 1); dtR(cv, 14, 25, 3, 3, 0x444444, 1);
  } else {                                                            // star
    lv_point_t s[10];
    for (int k = 0; k < 10; k++) {
      float ang = -1.5708f + k * 0.62832f, r = (k & 1) ? 6.5f : 15.0f;
      s[k].x = (lv_coord_t)(16 + r * cosf(ang)); s[k].y = (lv_coord_t)(17 + r * sinf(ang));
    }
    dtP(cv, s, 10, 0xFFC53D);
  }
}
static void dtMakeIcons() {
  lv_obj_t* cv = lv_canvas_create(lv_layer_sys());
  lv_obj_add_flag(cv, LV_OBJ_FLAG_HIDDEN);
  size_t sz = LV_IMG_BUF_SIZE_TRUE_COLOR_ALPHA(DT_IC, DT_IC);
  for (int i = 0; i < DT_IC_N; i++) {
    dtIcBuf[i] = (uint8_t*)malloc(sz);
    if (!dtIcBuf[i]) continue;
    memset(dtIcBuf[i], 0, sz);                                        // alpha 0 = transparent
    lv_canvas_set_buffer(cv, dtIcBuf[i], DT_IC, DT_IC, LV_IMG_CF_TRUE_COLOR_ALPHA);
    dtDrawIcon(cv, i);
    dtIcDsc[i].header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
    dtIcDsc[i].header.always_zero = 0;
    dtIcDsc[i].header.w = DT_IC; dtIcDsc[i].header.h = DT_IC;
    dtIcDsc[i].data_size = sz;
    dtIcDsc[i].data = dtIcBuf[i];
  }
  lv_obj_del(cv);
}
static lv_obj_t* dtIconImg(lv_obj_t* p, int idx, int zoom) {
  lv_obj_t* im = lv_img_create(p);
  if (idx >= 0 && idx < DT_IC_N && dtIcBuf[idx]) lv_img_set_src(im, &dtIcDsc[idx]);
  lv_img_set_zoom(im, zoom);                                          // 256 = 1:1
  return im;
}

// =====================================================================
//  SMALL UI HELPERS
// =====================================================================
static lv_obj_t* dtLbl(lv_obj_t* p, const char* t, const lv_font_t* f, uint32_t col) {
  lv_obj_t* l = lv_label_create(p);
  lv_label_set_text(l, t);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, K(col), 0);
  return l;
}
static void dtAnimX(void* v, int32_t x) { lv_obj_set_x((lv_obj_t*)v, x); }
static void dtAnimY(void* v, int32_t y) { lv_obj_set_y((lv_obj_t*)v, y); }
static void dtAnimOpa(void* v, int32_t o) { lv_obj_set_style_opa((lv_obj_t*)v, (lv_opa_t)o, 0); }
static void dtAnimShake(void* v, int32_t d) { lv_obj_set_x((lv_obj_t*)v, (int)(intptr_t)lv_obj_get_user_data((lv_obj_t*)v) + d); }
static void dtAnim(void* var, lv_anim_exec_xcb_t cb, int32_t from, int32_t to, uint32_t ms, uint32_t delay = 0,
                   lv_anim_path_cb_t path = lv_anim_path_ease_out) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, var);
  lv_anim_set_exec_cb(&a, cb);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_time(&a, ms);
  lv_anim_set_delay(&a, delay);
  lv_anim_set_path_cb(&a, path);
  lv_anim_start(&a);
}

static lv_obj_t* dtScr(bool sky) {                                    // new screen: pale blue gradient
  lv_obj_t* s = lv_obj_create(NULL);
  lv_obj_remove_style_all(s);
  lv_obj_set_style_bg_color(s, K(DC_SKY1), 0);
  lv_obj_set_style_bg_grad_color(s, K(DC_SKY2), 0);
  lv_obj_set_style_bg_grad_dir(s, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  (void)sky;
  return s;
}
static lv_obj_t* dtPanel(lv_obj_t* scr) {                             // the big white rounded card
  lv_obj_t* p = lv_obj_create(scr);
  lv_obj_remove_style_all(p);
  lv_obj_set_pos(p, 5, 5);
  lv_obj_set_size(p, 310, 230);
  lv_obj_set_style_bg_color(p, K(DC_WHITE), 0);
  lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(p, 22, 0);
  lv_obj_set_style_shadow_width(p, 18, 0);
  lv_obj_set_style_shadow_ofs_y(p, 4, 0);
  lv_obj_set_style_shadow_opa(p, LV_OPA_30, 0);
  lv_obj_set_style_shadow_color(p, K(0x3B5F86), 0);
  lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
  return p;
}
// round / flat icon button. circle = white disc with soft shadow
static lv_obj_t* dtBtn(lv_obj_t* p, int x, int y, int sz, const char* sym, bool circle, lv_event_cb_t cb, void* ud = nullptr, uint32_t col = DC_INK) {
  lv_obj_t* b = lv_btn_create(p);
  lv_obj_remove_style_all(b);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, sz, sz);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  if (circle) {
    lv_obj_set_style_bg_color(b, K(DC_WHITE), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(b, 10, 0);
    lv_obj_set_style_shadow_ofs_y(b, 2, 0);
    lv_obj_set_style_shadow_opa(b, LV_OPA_30, 0);
    lv_obj_set_style_shadow_color(b, K(0x3B5F86), 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, K(0xE3E8F0), 0);
  }
  lv_obj_set_style_bg_color(b, K(DC_TILE), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_set_ext_click_area(b, 6);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, sym);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, K(col), 0);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  return b;
}

// forward declarations of the navigation
static void dtOpenList(lv_scr_load_anim_t an);
static void dtOpenCount(int i, lv_scr_load_anim_t an);
static void dtOpenSettings(int i, lv_scr_load_anim_t an);
static void dtOpenParty(int i);
enum { DT_SPLASH, DT_LIST, DT_COUNT, DT_SET, DT_PARTY };
static int dtCur = DT_SPLASH;
static int dtSel = 0;                                                 // event shown on COUNT / PARTY / SETTINGS
static int dtBackTo = DT_LIST;                                        // where SETTINGS goes back to
static void dtGo(lv_obj_t* scr, lv_scr_load_anim_t an) { lv_scr_load_anim(scr, an, 300, 0, true); }

// =====================================================================
//  ANALOG CLOCK (lv_meter) with two "bells"
// =====================================================================
static lv_obj_t* dtMeter = nullptr;
static lv_meter_indicator_t *dtHr = nullptr, *dtMn = nullptr;
static int dtHrT = 50, dtMnT = 10;
static void dtSweepExec(void*, int32_t t) {
  if (!dtMeter) return;
  lv_meter_set_indicator_value(dtMeter, dtMn, dtMnT * t / 100);
  lv_meter_set_indicator_value(dtMeter, dtHr, dtHrT * t / 100);
}
static lv_obj_t* dtClock(lv_obj_t* parent, int x, int y, int sz) {
  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_pos(box, x, y);
  lv_obj_set_size(box, sz + 10, sz + 10);
  lv_obj_set_user_data(box, (void*)(intptr_t)x);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  for (int s = 0; s < 2; s++) {                                       // bells behind the dial
    lv_obj_t* b = lv_obj_create(box);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, sz / 4, sz / 4);
    lv_obj_set_pos(b, s ? sz - sz / 5 : 2, 0);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, K(0xE4EDF8), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_border_color(b, K(DC_WHITE), 0);
  }
  lv_obj_t* m = lv_meter_create(box);
  dtMeter = m;
  lv_obj_set_size(m, sz, sz);
  lv_obj_set_pos(m, 5, 8);
  lv_obj_set_style_bg_color(m, K(DC_WHITE), 0);
  lv_obj_set_style_bg_opa(m, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(m, 5, 0);
  lv_obj_set_style_border_color(m, K(0xE4EDF8), 0);
  lv_obj_set_style_text_opa(m, LV_OPA_TRANSP, LV_PART_TICKS);        // hide the numbers, keep the ticks
  lv_obj_set_style_bg_color(m, K(DC_INK), LV_PART_INDICATOR);
  lv_obj_set_style_pad_all(m, 4, LV_PART_INDICATOR);
  lv_meter_scale_t* sc = lv_meter_add_scale(m);
  lv_meter_set_scale_ticks(m, sc, 61, 1, 3, K(0xC9D3E0));
  lv_meter_set_scale_major_ticks(m, sc, 5, 2, 7, K(DC_INK), 10);
  lv_meter_set_scale_range(m, sc, 0, 60, 360, 270);
  dtHr = lv_meter_add_needle_line(m, sc, 4, K(DC_INK), -(sz * 28 / 100));
  dtMn = lv_meter_add_needle_line(m, sc, 3, K(DC_INK), -(sz * 14 / 100));
  int h, mi;
  dtClockTime(h, mi);
  dtHrT = h * 5 + mi / 12; dtMnT = mi;
  dtSweepExec(nullptr, 0);
  dtAnim(m, dtSweepExec, 0, 100, 900, 150, lv_anim_path_ease_in_out);  // hands sweep to the current time
  return box;
}

// =====================================================================
//  SPLASH
// =====================================================================
static lv_timer_t* dtSplashT = nullptr;
static void dtSplashGo(lv_timer_t*) { dtSplashT = nullptr; if (dtCur == DT_SPLASH) dtOpenList(LV_SCR_LOAD_ANIM_MOVE_LEFT); }
static void dtSplashTap(lv_event_t*) {
  if (dtSplashT) { lv_timer_del(dtSplashT); dtSplashT = nullptr; }
  if (dtCur == DT_SPLASH) dtOpenList(LV_SCR_LOAD_ANIM_MOVE_LEFT);
}
static lv_obj_t* dtBuildSplash() {
  dtCur = DT_SPLASH;
  lv_obj_t* s = dtScr(true);
  lv_obj_t* t = dtLbl(s, "days\ntil", DT_FONT_BIG, DC_WHITE);
  lv_obj_set_style_text_line_space(t, 4, 0);
  lv_obj_set_style_text_letter_space(t, 2, 0);
  lv_obj_set_pos(t, 26, 52);
  lv_obj_t* sh = lv_obj_create(s);                                    // soft ground shadow
  lv_obj_remove_style_all(sh);
  lv_obj_set_size(sh, 120, 14);
  lv_obj_set_pos(sh, 168, 188);
  lv_obj_set_style_radius(sh, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(sh, K(0x5C82AA), 0);
  lv_obj_set_style_bg_opa(sh, LV_OPA_40, 0);
  lv_obj_t* clk = dtClock(s, 160, 52, 120);
  dtAnim(clk, dtAnimY, 40, 52, 600);                                  // clock floats in
  char b[40];
  if (apMode && WiFi.status() != WL_CONNECTED) snprintf(b, sizeof(b), "WiFi setup: 192.168.4.1");
  else snprintf(b, sizeof(b), "panel: %s", myIp().c_str());
  lv_obj_t* ip = dtLbl(s, b, &lv_font_montserrat_12, DC_WHITE);
  lv_obj_set_style_text_opa(ip, LV_OPA_80, 0);
  lv_obj_align(ip, LV_ALIGN_BOTTOM_LEFT, 14, -10);
  lv_obj_add_event_cb(s, dtSplashTap, LV_EVENT_CLICKED, NULL);
  if (dtSplashT) lv_timer_del(dtSplashT);
  dtSplashT = lv_timer_create(dtSplashGo, 3000, NULL);
  lv_timer_set_repeat_count(dtSplashT, 1);
  return s;
}

// =====================================================================
//  EVENTS LIST
// =====================================================================
#define DT_CARD_H 170
#define DT_PAD    10
static lv_obj_t* dtList = nullptr;
static lv_obj_t* dtDots[DT_MAX + 1];
static int dtDotN = 0, dtDotAct = -1;

static void dtDotsSet(int act) {
  if (act == dtDotAct) return;
  dtDotAct = act;
  for (int i = 0; i < dtDotN; i++) {
    bool on = i == act;
    lv_obj_set_size(dtDots[i], on ? 10 : 6, on ? 10 : 6);
    lv_obj_set_style_bg_opa(dtDots[i], on ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dtDots[i], K(on ? DC_INK : 0xCFD4DF), 0);
    lv_obj_set_style_border_width(dtDots[i], on ? 2 : 1, 0);
  }
}
static void dtListScroll(lv_event_t*) {
  int pitch = DT_CARD_H + DT_PAD;
  int idx = (lv_obj_get_scroll_y(dtList) + pitch / 2) / pitch;
  dtDotsSet(constrain(idx, 0, dtDotN - 1));
}
static void dtBackFromList(lv_event_t*) {
  dtCur = DT_SPLASH;
  lv_obj_t* s = dtBuildSplash();
  dtGo(s, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
}
static void dtCardClick(lv_event_t* e) { dtOpenCount((int)(intptr_t)lv_event_get_user_data(e), LV_SCR_LOAD_ANIM_MOVE_LEFT); }
static void dtAddClick(lv_event_t*) {
  if (dtN >= DT_MAX) return;
  dtBackTo = DT_LIST;
  dtOpenSettings(dtN, LV_SCR_LOAD_ANIM_MOVE_LEFT);                    // index == dtN means "new"
}
static lv_obj_t* dtBuildList() {
  dtCur = DT_LIST;
  lv_obj_t* s = dtScr(false);
  lv_obj_t* p = dtPanel(s);
  dtBtn(p, 8, 8, 30, LV_SYMBOL_LEFT, false, dtBackFromList);
  lv_obj_t* t = dtLbl(p, "Events", &lv_font_montserrat_16, DC_INK);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 13);

  dtList = lv_obj_create(p);
  lv_obj_remove_style_all(dtList);
  lv_obj_set_pos(dtList, 14, 46);
  lv_obj_set_size(dtList, 236, 176);
  lv_obj_set_flex_flow(dtList, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(dtList, DT_PAD, 0);
  lv_obj_set_scroll_dir(dtList, LV_DIR_VER);
  lv_obj_set_scroll_snap_y(dtList, LV_SCROLL_SNAP_START);
  lv_obj_set_scrollbar_mode(dtList, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_event_cb(dtList, dtListScroll, LV_EVENT_SCROLL, NULL);

  if (dtN == 0) {
    lv_obj_t* e = dtLbl(p, "No events yet\ntap + to add one", &lv_font_montserrat_14, DC_MUT);
    lv_obj_set_style_text_align(e, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(e, LV_ALIGN_CENTER, -20, 6);
  }
  for (int i = 0; i < dtN; i++) {
    lv_obj_t* c = lv_obj_create(dtList);
    lv_obj_remove_style_all(c);
    lv_obj_set_size(c, 236, DT_CARD_H);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_add_event_cb(c, dtCardClick, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_t* tile = lv_obj_create(c);                                // rounded grey tile with the icon
    lv_obj_remove_style_all(tile);
    lv_obj_set_size(tile, 236, 112);
    lv_obj_set_style_radius(tile, 18, 0);
    lv_obj_set_style_bg_color(tile, K(DC_TILE), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(tile, K(0xE6EAF2), LV_STATE_PRESSED);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* im = dtIconImg(tile, dtEv[i].icon, 400);
    lv_obj_center(im);
    lv_obj_t* n = dtLbl(c, dtEv[i].name, &lv_font_montserrat_16, DC_INK);
    lv_obj_set_pos(n, 4, 122);
    lv_obj_t* sb = dtLbl(c, DT_SUB[dtEv[i].icon % 6], &lv_font_montserrat_12, DC_MUT);
    lv_obj_set_pos(sb, 4, 146);
  }
  // dot indicator on the right
  lv_obj_t* col = lv_obj_create(p);
  lv_obj_remove_style_all(col);
  lv_obj_set_size(col, 20, 150);
  lv_obj_set_pos(col, 266, 50);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(col, 10, 0);
  lv_obj_clear_flag(col, LV_OBJ_FLAG_CLICKABLE);
  dtDotN = dtN; dtDotAct = -1;
  for (int i = 0; i < dtN; i++) {
    dtDots[i] = lv_obj_create(col);
    lv_obj_remove_style_all(dtDots[i]);
    lv_obj_set_style_radius(dtDots[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dtDots[i], K(0xCFD4DF), 0);
    lv_obj_set_style_bg_opa(dtDots[i], LV_OPA_COVER, 0);
  }
  if (dtN) dtDotsSet(0);
  if (dtN < DT_MAX) dtBtn(p, 262, 184, 34, LV_SYMBOL_PLUS, true, dtAddClick);
  return s;
}
static void dtOpenList(lv_scr_load_anim_t an) { lv_obj_t* s = dtBuildList(); dtGo(s, an); }

// =====================================================================
//  COUNTDOWN
// =====================================================================
static lv_obj_t *dtCtTitle, *dtCtV1, *dtCtV2, *dtCtL1, *dtCtL2, *dtCtColon;
static int dtPartySeen = -1;
static time_t dtPartyT = 0;

static void dtRoll(lv_obj_t* l, const char* t) {                      // number changes: new digits rise in
  if (!l || !strcmp(lv_label_get_text(l), t)) return;
  lv_label_set_text(l, t);
  int y = lv_obj_get_y(l);
  dtAnim(l, dtAnimY, y + 10, y, 220);
  dtAnim(l, dtAnimOpa, 0, 255, 220, 0, lv_anim_path_linear);
}
static void dtCountRefresh() {
  if (dtCur != DT_COUNT || !dtCtV1 || dtSel >= dtN) return;
  const DtEv& e = dtEv[dtSel];
  time_t now;
  char a[12], b[12], t[40];
  if (!dtNow(now)) {                                                  // NTP not ready yet
    dtRoll(dtCtV1, "--"); dtRoll(dtCtV2, "--");
    lv_label_set_text(dtCtL1, "waiting"); lv_label_set_text(dtCtL2, "for time");
    return;
  }
  time_t tt = dtTarget(e, now);
  long diff = (long)(tt - now), s = diff > 0 ? diff : 0;
  if (diff <= 0) {
    if (diff <= -86400L && !e.rep) snprintf(t, sizeof(t), "%s\nalready passed", e.name);
    else snprintf(t, sizeof(t), "%s\nis here!", e.name);
    lv_label_set_text(dtCtTitle, t);
    if (e.notify && diff > -86400L && !(dtPartySeen == dtSel && dtPartyT == tt)) { dtPartySeen = dtSel; dtPartyT = tt; dtOpenParty(dtSel); return; }
  }
  int m = e.mode;
  if (m == 0) m = s >= 86400 ? 1 : (s >= 3600 ? 2 : 3);
  const char *l1, *l2;
  if (m == 1)      { snprintf(a, sizeof(a), "%02ld", s / 86400); snprintf(b, sizeof(b), "%02ld", (s % 86400) / 3600); l1 = "Days"; l2 = "Hours"; }
  else if (m == 2) { snprintf(a, sizeof(a), "%02ld", s / 3600);  snprintf(b, sizeof(b), "%02ld", (s % 3600) / 60);    l1 = "Hours"; l2 = "Minutes"; }
  else             { snprintf(a, sizeof(a), "%02ld", s / 60);    snprintf(b, sizeof(b), "%02ld", s % 60);             l1 = "Minutes"; l2 = "Seconds"; }
  dtRoll(dtCtV1, a); dtRoll(dtCtV2, b);
  lv_label_set_text(dtCtL1, l1); lv_label_set_text(dtCtL2, l2);
}
static void dtCountBack(lv_event_t*) { dtOpenList(LV_SCR_LOAD_ANIM_MOVE_RIGHT); }
static void dtCountSettings(lv_event_t*) { dtBackTo = DT_COUNT; dtOpenSettings(dtSel, LV_SCR_LOAD_ANIM_MOVE_LEFT); }
static lv_obj_t* dtBuildCount(int i) {
  dtCur = DT_COUNT; dtSel = i;
  const DtEv& e = dtEv[i];
  lv_obj_t* s = dtScr(false);
  lv_obj_t* p = dtPanel(s);
  dtBtn(p, 8, 8, 30, LV_SYMBOL_LEFT, false, dtCountBack);
  dtBtn(p, 268, 8, 32, LV_SYMBOL_SETTINGS, true, dtCountSettings);
  lv_obj_t* ic = dtIconImg(p, e.icon, 360);
  lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 28);
  char t[40];
  snprintf(t, sizeof(t), "Days til\n%s!", e.name);
  dtCtTitle = dtLbl(p, t, &lv_font_montserrat_16, DC_INK);
  lv_obj_set_width(dtCtTitle, 260);
  lv_obj_set_style_text_align(dtCtTitle, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(dtCtTitle, LV_ALIGN_TOP_MID, 0, 80);
  lv_obj_t* tile = lv_obj_create(p);
  lv_obj_remove_style_all(tile);
  lv_obj_set_size(tile, 250, 62);
  lv_obj_align(tile, LV_ALIGN_TOP_MID, 0, 128);
  lv_obj_set_style_radius(tile, 16, 0);
  lv_obj_set_style_bg_color(tile, K(DC_TILE), 0);
  lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
  lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
  dtCtV1 = dtLbl(tile, "--", DT_FONT_BIG, DC_INK);
  lv_obj_set_width(dtCtV1, 96); lv_obj_set_style_text_align(dtCtV1, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(dtCtV1, 10, 19);
  dtCtColon = dtLbl(tile, ":", DT_FONT_BIG, DC_INK);
  lv_obj_align(dtCtColon, LV_ALIGN_TOP_MID, 0, 19);
  dtCtV2 = dtLbl(tile, "--", DT_FONT_BIG, DC_INK);
  lv_obj_set_width(dtCtV2, 96); lv_obj_set_style_text_align(dtCtV2, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(dtCtV2, 144, 19);
  dtCtL1 = dtLbl(p, "Days", &lv_font_montserrat_12, DC_MUT);
  lv_obj_set_width(dtCtL1, 96); lv_obj_set_style_text_align(dtCtL1, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(dtCtL1, 30, 196);
  dtCtL2 = dtLbl(p, "Hours", &lv_font_montserrat_12, DC_MUT);
  lv_obj_set_width(dtCtL2, 96); lv_obj_set_style_text_align(dtCtL2, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(dtCtL2, 184, 196);
  dtCountRefresh();
  return s;
}
static void dtOpenCount(int i, lv_scr_load_anim_t an) {
  if (i < 0 || i >= dtN) { dtOpenList(an); return; }
  lv_obj_t* s = dtBuildCount(i);
  if (dtCur == DT_COUNT) dtGo(s, an);
}

// =====================================================================
//  PARTY ("<name> is here!")
// =====================================================================
static void dtPartyBack(lv_event_t*) { dtOpenList(LV_SCR_LOAD_ANIM_MOVE_RIGHT); }
static void dtPartySet(lv_event_t*) { dtBackTo = DT_LIST; dtOpenSettings(dtSel, LV_SCR_LOAD_ANIM_MOVE_LEFT); }
static void dtOpenParty(int i) {
  dtCur = DT_PARTY; dtSel = i;
  lv_obj_t* s = dtScr(true);
  static const uint32_t cc[6] = {0xE5484D, 0xFFC53D, 0x34D399, 0x22D3EE, 0xA78BFA, 0xFFFFFF};
  for (int k = 0; k < 12; k++) {                                      // confetti
    lv_obj_t* c = lv_obj_create(s);
    lv_obj_remove_style_all(c);
    lv_obj_set_size(c, 6, 6);
    lv_obj_set_pos(c, 8 + (k * 53) % 300, -10);
    lv_obj_set_style_radius(c, k % 2 ? 3 : 0, 0);
    lv_obj_set_style_bg_color(c, K(cc[k % 6]), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, c);
    lv_anim_set_exec_cb(&a, dtAnimY);
    lv_anim_set_values(&a, -10, 245);
    lv_anim_set_time(&a, 2000 + (k * 270) % 1500);
    lv_anim_set_delay(&a, (k * 160) % 1400);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
  }
  dtBtn(s, 8, 8, 30, LV_SYMBOL_LEFT, false, dtPartyBack, nullptr, DC_INK);
  dtBtn(s, 280, 8, 32, LV_SYMBOL_SETTINGS, true, dtPartySet);
  lv_obj_t* clk = dtClock(s, 105, 30, 100);
  lv_anim_t a;                                                        // the clock "rings": quick left-right shake
  lv_anim_init(&a);
  lv_anim_set_var(&a, clk);
  lv_anim_set_exec_cb(&a, dtAnimShake);
  lv_anim_set_values(&a, -3, 3);
  lv_anim_set_time(&a, 60);
  lv_anim_set_playback_time(&a, 60);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a);
  char t[40];
  snprintf(t, sizeof(t), "%s\nis here!", dtEv[i].name);
  lv_obj_t* l = dtLbl(s, t, &lv_font_montserrat_16, DC_INK);
  lv_obj_set_width(l, 280);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -26);
  dtGo(s, LV_SCR_LOAD_ANIM_FADE_ON);
}

// =====================================================================
//  SETTINGS
// =====================================================================
static lv_obj_t *dtTa, *dtKb, *dtIcBtn[DT_IC_N], *dtDdD, *dtDdM, *dtDdY, *dtDdH, *dtDdMi, *dtCbN, *dtCbR, *dtDdMode;
static int dtEdIdx = 0, dtEdIcon = 0;
static char dtOptD[100], dtOptY[100], dtOptH[80], dtOptMi[190];
#define DT_Y0 2026
#define DT_YN 16

static void dtIcPick(lv_event_t* e) {
  dtEdIcon = (int)(intptr_t)lv_event_get_user_data(e);
  for (int i = 0; i < DT_IC_N; i++) {
    bool on = i == dtEdIcon;
    lv_obj_set_style_bg_color(dtIcBtn[i], K(DC_TILE), 0);
    lv_obj_set_style_bg_opa(dtIcBtn[i], on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dtIcBtn[i], on ? 2 : 0, 0);
    lv_obj_set_style_border_color(dtIcBtn[i], K(DC_INK), 0);
  }
}
static void dtTaClick(lv_event_t*) {
  lv_obj_clear_flag(dtKb, LV_OBJ_FLAG_HIDDEN);
  lv_keyboard_set_textarea(dtKb, dtTa);
  lv_obj_add_state(dtTa, LV_STATE_FOCUSED);
}
static void dtKbEvent(lv_event_t* e) {
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_READY || c == LV_EVENT_CANCEL) { lv_obj_add_flag(dtKb, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_state(dtTa, LV_STATE_FOCUSED); }
}
static void dtDdOpen(lv_event_t* e) {                                  // style the popup list when it opens
  lv_obj_t* l = lv_dropdown_get_list(lv_event_get_target(e));
  if (!l) return;
  lv_obj_set_style_bg_color(l, K(DC_WHITE), 0);
  lv_obj_set_style_text_color(l, K(DC_INK), 0);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
  lv_obj_set_style_border_color(l, K(0xD6DCE8), 0);
  lv_obj_set_style_bg_color(l, K(DC_TILE), LV_PART_SELECTED | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(l, K(DC_INK), LV_PART_SELECTED | LV_STATE_CHECKED);
}
static lv_obj_t* dtDd(lv_obj_t* p, const char* opts, int w, int sel) {
  lv_obj_t* d = lv_dropdown_create(p);
  lv_dropdown_set_options(d, opts);
  lv_dropdown_set_selected(d, sel);
  lv_obj_set_size(d, w, 30);
  lv_obj_set_style_bg_color(d, K(DC_TILE), 0);
  lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(d, 0, 0);
  lv_obj_set_style_radius(d, 10, 0);
  lv_obj_set_style_text_color(d, K(DC_INK), 0);
  lv_obj_set_style_text_font(d, &lv_font_montserrat_12, 0);
  lv_obj_set_style_pad_all(d, 6, 0);
  lv_obj_set_style_shadow_width(d, 0, 0);
  lv_obj_add_event_cb(d, dtDdOpen, LV_EVENT_READY, NULL);
  return d;
}
static lv_obj_t* dtRow(lv_obj_t* p, int h) {
  lv_obj_t* r = lv_obj_create(p);
  lv_obj_remove_style_all(r);
  lv_obj_set_size(r, 286, h);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, 6, 0);
  lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  return r;
}
static lv_obj_t* dtCheck(lv_obj_t* p, const char* txt, bool on) {
  lv_obj_t* c = lv_checkbox_create(p);
  lv_checkbox_set_text(c, txt);
  lv_obj_set_style_text_color(c, K(DC_INK), 0);
  lv_obj_set_style_text_font(c, &lv_font_montserrat_12, 0);
  lv_obj_set_style_bg_color(c, K(DC_INK), LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_set_style_border_color(c, K(DC_INK), LV_PART_INDICATOR);
  lv_obj_set_style_border_width(c, 2, LV_PART_INDICATOR);
  lv_obj_set_style_radius(c, 5, LV_PART_INDICATOR);
  lv_obj_set_style_text_color(c, K(DC_WHITE), LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_set_style_pad_column(c, 6, 0);
  if (on) lv_obj_add_state(c, LV_STATE_CHECKED);
  return c;
}
static void dtSetBack(lv_event_t*) {
  if (dtBackTo == DT_COUNT && dtEdIdx < dtN) dtOpenCount(dtEdIdx, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
  else dtOpenList(LV_SCR_LOAD_ANIM_MOVE_RIGHT);
}
static void dtSetSave(lv_event_t*) {
  DtEv e;
  memset(&e, 0, sizeof(e));
  const char* nm = lv_textarea_get_text(dtTa);
  strlcpy(e.name, (nm && nm[0]) ? nm : "Event", sizeof(e.name));
  e.icon = dtEdIcon;
  e.d = lv_dropdown_get_selected(dtDdD) + 1;
  e.mo = lv_dropdown_get_selected(dtDdM) + 1;
  e.y = DT_Y0 + lv_dropdown_get_selected(dtDdY);
  e.h = lv_dropdown_get_selected(dtDdH);
  e.mi = lv_dropdown_get_selected(dtDdMi);
  e.notify = lv_obj_has_state(dtCbN, LV_STATE_CHECKED);
  e.rep = lv_obj_has_state(dtCbR, LV_STATE_CHECKED);
  e.mode = lv_dropdown_get_selected(dtDdMode);
  int i = dtEdIdx;
  if (i >= dtN) { if (dtN >= DT_MAX) return; i = dtN++; }
  dtEv[i] = e;
  dtSave();
  dtPartySeen = -1;
  dtOpenCount(i, LV_SCR_LOAD_ANIM_MOVE_LEFT);
}
static void dtSetDelete(lv_event_t*) {
  if (dtEdIdx < dtN) {
    for (int i = dtEdIdx; i < dtN - 1; i++) dtEv[i] = dtEv[i + 1];
    dtN--;
    dtSave();
  }
  dtPartySeen = -1;
  dtOpenList(LV_SCR_LOAD_ANIM_MOVE_RIGHT);
}
static lv_obj_t* dtTxtBtn(lv_obj_t* p, int w, const char* txt, uint32_t bg, uint32_t fg, bool outline, lv_event_cb_t cb) {
  lv_obj_t* b = lv_btn_create(p);
  lv_obj_remove_style_all(b);
  lv_obj_set_size(b, w, 34);
  lv_obj_set_style_radius(b, 12, 0);
  lv_obj_set_style_bg_color(b, K(bg), 0);
  lv_obj_set_style_bg_opa(b, outline ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(b, outline ? 2 : 0, 0);
  lv_obj_set_style_border_color(b, K(fg), 0);
  lv_obj_set_style_opa(b, LV_OPA_70, LV_STATE_PRESSED);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, K(fg), 0);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
  return b;
}
static lv_obj_t* dtBuildSettings(int idx) {
  dtCur = DT_SET; dtEdIdx = idx;
  bool isNew = idx >= dtN;
  DtEv e;
  if (!isNew) e = dtEv[idx];
  else {                                                               // new event: 30 days from now at 00:00
    memset(&e, 0, sizeof(e));
    e.icon = 5; e.notify = 1; e.y = 2026; e.mo = 12; e.d = 25;
    time_t t;
    if (dtNow(t)) { t += 30L * 86400; struct tm tmv; localtime_r(&t, &tmv); e.y = tmv.tm_year + 1900; e.mo = tmv.tm_mon + 1; e.d = tmv.tm_mday; }
  }
  dtEdIcon = e.icon % DT_IC_N;
  // option strings
  { char* q = dtOptD; for (int i = 1; i <= 31; i++) q += sprintf(q, i < 31 ? "%d\n" : "%d", i); }
  { char* q = dtOptY; for (int i = 0; i < DT_YN; i++) q += sprintf(q, i < DT_YN - 1 ? "%d\n" : "%d", DT_Y0 + i); }
  { char* q = dtOptH; for (int i = 0; i < 24; i++) q += sprintf(q, i < 23 ? "%02d\n" : "%02d", i); }
  { char* q = dtOptMi; for (int i = 0; i < 60; i++) q += sprintf(q, i < 59 ? "%02d\n" : "%02d", i); }

  lv_obj_t* s = dtScr(false);
  lv_obj_t* p = dtPanel(s);
  dtBtn(p, 8, 8, 30, LV_SYMBOL_LEFT, false, dtSetBack);
  lv_obj_t* t = dtLbl(p, isNew ? "New event" : "Settings", &lv_font_montserrat_16, DC_INK);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 13);

  lv_obj_t* sc = lv_obj_create(p);                                     // scrolling form
  lv_obj_remove_style_all(sc);
  lv_obj_set_pos(sc, 10, 44);
  lv_obj_set_size(sc, 292, 180);
  lv_obj_set_flex_flow(sc, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(sc, 8, 0);
  lv_obj_set_style_pad_bottom(sc, 8, 0);
  lv_obj_set_scroll_dir(sc, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(sc, LV_SCROLLBAR_MODE_OFF);

  dtTa = lv_textarea_create(sc);
  lv_textarea_set_one_line(dtTa, true);
  lv_textarea_set_max_length(dtTa, 15);
  lv_textarea_set_placeholder_text(dtTa, "Countdown Title");
  if (!isNew) lv_textarea_set_text(dtTa, e.name);
  lv_obj_set_size(dtTa, 286, 34);
  lv_obj_set_style_bg_color(dtTa, K(DC_TILE), 0);
  lv_obj_set_style_bg_opa(dtTa, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(dtTa, 0, 0);
  lv_obj_set_style_radius(dtTa, 10, 0);
  lv_obj_set_style_text_color(dtTa, K(DC_INK), 0);
  lv_obj_set_style_text_font(dtTa, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(dtTa, K(DC_MUT), LV_PART_TEXTAREA_PLACEHOLDER);
  lv_obj_add_event_cb(dtTa, dtTaClick, LV_EVENT_CLICKED, NULL);

  lv_obj_t* ir = dtRow(sc, 40);                                        // icon picker
  lv_obj_set_flex_align(ir, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  for (int i = 0; i < DT_IC_N; i++) {
    lv_obj_t* b = lv_btn_create(ir);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 40, 40);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_t* im = dtIconImg(b, i, 256);
    lv_obj_center(im);
    lv_obj_add_event_cb(b, dtIcPick, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    dtIcBtn[i] = b;
  }
  lv_event_t fake; (void)fake;
  for (int i = 0; i < DT_IC_N; i++) {
    bool on = i == dtEdIcon;
    lv_obj_set_style_bg_color(dtIcBtn[i], K(DC_TILE), 0);
    lv_obj_set_style_bg_opa(dtIcBtn[i], on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dtIcBtn[i], on ? 2 : 0, 0);
    lv_obj_set_style_border_color(dtIcBtn[i], K(DC_INK), 0);
  }

  lv_obj_t* r1 = dtRow(sc, 32);                                        // date
  lv_obj_t* l1 = dtLbl(r1, "Date", &lv_font_montserrat_12, DC_MUT);
  lv_obj_set_width(l1, 36);
  dtDdD = dtDd(r1, dtOptD, 62, constrain(e.d - 1, 0, 30));
  dtDdM = dtDd(r1, "Jan\nFeb\nMar\nApr\nMay\nJun\nJul\nAug\nSep\nOct\nNov\nDec", 74, constrain(e.mo - 1, 0, 11));
  dtDdY = dtDd(r1, dtOptY, 84, constrain((int)e.y - DT_Y0, 0, DT_YN - 1));

  lv_obj_t* r2 = dtRow(sc, 32);                                        // time of day
  lv_obj_t* l2 = dtLbl(r2, "Time", &lv_font_montserrat_12, DC_MUT);
  lv_obj_set_width(l2, 36);
  dtDdH = dtDd(r2, dtOptH, 62, constrain((int)e.h, 0, 23));
  lv_obj_t* cl = dtLbl(r2, ":", &lv_font_montserrat_14, DC_INK); (void)cl;
  dtDdMi = dtDd(r2, dtOptMi, 62, constrain((int)e.mi, 0, 59));

  lv_obj_t* r3 = dtRow(sc, 32);                                        // notify / yearly
  dtCbN = dtCheck(r3, "Notify me", e.notify);
  lv_obj_set_width(dtCbN, 120);
  dtCbR = dtCheck(r3, "Every year", e.rep);

  lv_obj_t* r4 = dtRow(sc, 32);                                        // display unit
  lv_obj_t* l4 = dtLbl(r4, "Show", &lv_font_montserrat_12, DC_MUT);
  lv_obj_set_width(l4, 36);
  dtDdMode = dtDd(r4, "Auto\nDays : Hours\nHours : Min\nMin : Sec", 150, constrain((int)e.mode, 0, 3));

  lv_obj_t* r5 = dtRow(sc, 40);
  lv_obj_set_flex_align(r5, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  dtTxtBtn(r5, isNew ? 286 : 138, "Save", DC_INK, DC_WHITE, false, dtSetSave);
  if (!isNew) dtTxtBtn(r5, 138, "Delete", DC_WHITE, DC_RED, true, dtSetDelete);

  dtKb = lv_keyboard_create(s);                                        // on-screen keyboard, hidden until the title is tapped
  lv_obj_set_size(dtKb, LV_HOR, 124);
  lv_obj_align(dtKb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_text_font(dtKb, &lv_font_montserrat_14, LV_PART_ITEMS);
  lv_obj_add_flag(dtKb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(dtKb, dtKbEvent, LV_EVENT_ALL, NULL);
  return s;
}
static void dtOpenSettings(int i, lv_scr_load_anim_t an) { lv_obj_t* s = dtBuildSettings(i); dtGo(s, an); }

// =====================================================================
//  ENTRY POINTS used by main.cpp
// =====================================================================
static void lvUiInit() {
  lv_init();
  size_t lines = 24, px = LV_HOR * lines;
  dtBuf1 = (lv_color_t*)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  dtBuf2 = (lv_color_t*)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  if (!dtBuf1 || !dtBuf2) {                                            // low RAM: smaller render blocks
    if (dtBuf1) free(dtBuf1);
    if (dtBuf2) free(dtBuf2);
    lines = 8; px = LV_HOR * lines;
    dtBuf1 = (lv_color_t*)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    dtBuf2 = (lv_color_t*)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  }
  Serial.printf("LVGL: draw buffers %u lines x2 (%s), heap=%u\n", (unsigned)lines, (dtBuf1 && dtBuf2) ? "ok" : "FAILED", (unsigned)ESP.getFreeHeap());
  lv_disp_draw_buf_init(&dtDraw, dtBuf1, dtBuf2, px);
  lv_disp_drv_init(&dtDrv);
  dtDrv.hor_res = LV_HOR; dtDrv.ver_res = LV_VER;
  dtDrv.flush_cb = dtFlush; dtDrv.draw_buf = &dtDraw;
  lv_disp_drv_register(&dtDrv);
  lv_indev_drv_init(&dtIn);
  dtIn.type = LV_INDEV_TYPE_POINTER;
  dtIn.read_cb = dtTouchRead;
  dtIn.scroll_throw = 10;
  dtIn.scroll_limit = 8;
  lv_indev_drv_register(&dtIn);

  configTzTime(DT_TZ, "pool.ntp.org", "time.google.com");              // clock for the countdowns (needs WiFi)
  dtLoad();
  dtMakeIcons();
  lv_obj_t* old = lv_scr_act();
  lv_obj_t* s = dtBuildSplash();
  lv_scr_load(s);
  lv_obj_del(old);
  Serial.printf("Days-til UI ready: %d events, heap=%u maxblock=%u\n", dtN, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
}

// video / GIF / image from the web panel: legacy full-screen player, tap = back to the UI
static bool dtWasMedia = false;
static void dtMediaTick() {
  mediaService();
  if (curPage != P_MEDIA) return;
  dtWasMedia = true;
  if (lightPath.length()) lightTick();
  else if (liveType == LT_GIF) gifTick();
  else if (liveType == LT_CLIP || liveType == LT_STREAM) clipTick();
  static uint32_t tapAt = 0;
  int rx, ry;
  if (touchRaw(rx, ry)) {
    if (!tapAt) tapAt = millis();
    if (millis() - tapAt > 250) { tapAt = 0; plActive = false; gifStop(); clipClose(); lightStop(); curPage = P_OVERVIEW; pageStart = millis(); }
  } else tapAt = 0;
  delay(1);
}

static void lvUiTick() {
  if (curPage == P_MEDIA) { dtMediaTick(); return; }
  if (dtWasMedia) { dtWasMedia = false; lv_obj_invalidate(lv_scr_act()); }
  mediaService();
  if (newData) { newData = false; takeSnapshot(); }                    // keeps the PC-stats snapshot fresh (not shown here)

  static uint32_t t1 = 0;
  if (millis() - t1 > 1000) {
    t1 = millis();
    if (gIdleMin && !gAsleep && millis() - gLastAct > (uint32_t)gIdleMin * 60000UL) gAsleep = true;
    applyBrightness();
    dtCountRefresh();                                                  // countdown ticks once per second
  }
  static uint32_t lastHeap = 0;
  if (millis() - lastHeap > 10000) { lastHeap = millis(); Serial.printf("heap=%u maxblock=%u wifi=%d\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(), (int)WiFi.status()); }

  uint32_t wait = lv_timer_handler();
  delay(wait > 6 ? 6 : (wait ? wait : 1));
}
#endif  // USE_LVGL
