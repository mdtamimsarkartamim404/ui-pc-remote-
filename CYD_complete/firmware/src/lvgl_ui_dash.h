// LVGL 8.3 card dashboard for the CYD (320x240, no PSRAM). Built only with -DUSE_LVGL (env:cyd_lvgl).
// Drop-in replacement for lvgl_ui.h: main.cpp calls lvUiInit() / lvUiTick() exactly like before.
//
//  Tabs (tap a chip OR swipe left/right):  OVERVIEW | CPU | MEMORY | NETWORK | SYSTEM
//  Every tab change: slide animation of the page + staggered "rise and fade in" of the cards.
//  Media (video / GIF / image from the web panel) still plays full screen, a tap brings the UI back.
#pragma once
#ifdef USE_LVGL
#include <lvgl.h>

#define LV_HOR 320
#define LV_VER 240
#define TAB_N  5

// ---- palette (dark slate + cyan/violet accents) ----
#define C_BG    0x0A0E14
#define C_CARD  0x121923
#define C_CARD2 0x182130
#define C_LINE  0x243044
#define C_TX    0xEAF0F7
#define C_MUT   0x8896A8
#define C_CY    0x22D3EE
#define C_VIO   0xA78BFA
#define C_GRN   0x34D399
#define C_ORG   0xFBBF24
#define C_RED   0xF87171
#define C_TRK   0x1E2733

static inline lv_color_t K(uint32_t h) { return lv_color_hex(h); }

// ---------------- display + touch drivers ----------------
static lv_disp_draw_buf_t dDraw;
static lv_disp_drv_t dDrv;
static lv_indev_drv_t dIn;
static lv_color_t *dBuf1 = nullptr, *dBuf2 = nullptr;

static void dFlush(lv_disp_drv_t* d, const lv_area_t* a, lv_color_t* c) {
  int32_t w = a->x2 - a->x1 + 1, h = a->y2 - a->y1 + 1;
  tft.waitDMA();
  tft.pushImageDMA(a->x1, a->y1, w, h, (const lgfx::rgb565_t*)c);
  lv_disp_flush_ready(d);
}

static void dTouchRead(lv_indev_drv_t*, lv_indev_data_t* d) {
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

// ---------------- styles ----------------
static lv_style_t sCard, sChip, sChipOn;

static void dStyles() {
  lv_style_init(&sCard);                       // rounded card with a soft shadow
  lv_style_set_bg_color(&sCard, K(C_CARD));
  lv_style_set_bg_opa(&sCard, LV_OPA_COVER);
  lv_style_set_radius(&sCard, 14);
  lv_style_set_border_color(&sCard, K(C_LINE));
  lv_style_set_border_width(&sCard, 1);
  lv_style_set_pad_all(&sCard, 0);
  lv_style_set_shadow_width(&sCard, 14);
  lv_style_set_shadow_ofs_y(&sCard, 3);
  lv_style_set_shadow_opa(&sCard, LV_OPA_50);
  lv_style_set_shadow_color(&sCard, lv_color_black());

  lv_style_init(&sChip);
  lv_style_set_bg_color(&sChip, K(C_CARD));
  lv_style_set_bg_opa(&sChip, LV_OPA_COVER);
  lv_style_set_radius(&sChip, 13);
  lv_style_set_border_width(&sChip, 0);
  lv_style_set_shadow_width(&sChip, 0);
  lv_style_set_text_color(&sChip, K(C_MUT));

  lv_style_init(&sChipOn);
  lv_style_set_bg_color(&sChipOn, K(C_CY));
  lv_style_set_text_color(&sChipOn, K(C_BG));
}

static lv_obj_t* dLbl(lv_obj_t* p, const char* t, const lv_font_t* f, uint32_t col) {
  lv_obj_t* l = lv_label_create(p);
  lv_label_set_text(l, t);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, K(col), 0);
  return l;
}

// ---------------- animation helpers ----------------
static void aX(void* v, int32_t x) { lv_obj_set_x((lv_obj_t*)v, x); }
static void aY(void* v, int32_t y) { lv_obj_set_y((lv_obj_t*)v, y); }
static void aOpa(void* v, int32_t o) { lv_obj_set_style_opa((lv_obj_t*)v, (lv_opa_t)o, 0); }
static void aPulse(void* v, int32_t o) { lv_obj_set_style_bg_opa((lv_obj_t*)v, (lv_opa_t)o, 0); }
static void dAnim(void* var, lv_anim_exec_xcb_t cb, int32_t from, int32_t to, uint32_t ms,
                  lv_anim_path_cb_t path = lv_anim_path_ease_out, uint32_t delay = 0) {
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

// ---------------- screen skeleton + card registry ----------------
static lv_obj_t *dTv = nullptr, *dTiles[TAB_N], *dChips[TAB_N];
static lv_obj_t *dTitle, *dClock, *dDot;
static int dActTile = 0;

#define MAX_CARDS 8
static lv_obj_t* dCards[TAB_N][MAX_CARDS];
static int dCardN[TAB_N];

static lv_obj_t* dCard(int tab, int x, int y, int w, int h) {
  lv_obj_t* o = lv_obj_create(dTiles[tab]);
  lv_obj_remove_style_all(o);
  lv_obj_add_style(o, &sCard, 0);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_user_data(o, (void*)(intptr_t)y);          // remember the resting y for the entrance animation
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  if (dCardN[tab] < MAX_CARDS) dCards[tab][dCardN[tab]++] = o;
  return o;
}

// staggered rise + fade of every card on a tab (this is the "tab transition")
static void dEnter(int tab) {
  for (int i = 0; i < dCardN[tab]; i++) {
    lv_obj_t* c = dCards[tab][i];
    int y0 = (int)(intptr_t)lv_obj_get_user_data(c);
    lv_anim_del(c, aY);
    lv_anim_del(c, aOpa);
    lv_obj_set_style_opa(c, LV_OPA_TRANSP, 0);
    dAnim(c, aY, y0 + 16, y0, 280, lv_anim_path_ease_out, 40 + i * 45);
    dAnim(c, aOpa, 0, 255, 260, lv_anim_path_linear, 40 + i * 45);
  }
}

static void dSetChip(int act) {
  for (int i = 0; i < TAB_N; i++) {
    if (i == act) lv_obj_add_style(dChips[i], &sChipOn, 0);
    else lv_obj_remove_style(dChips[i], &sChipOn, 0);
    lv_obj_invalidate(dChips[i]);
  }
}

static void dTabChanged(lv_event_t*) {
  lv_obj_t* act = lv_tileview_get_tile_act(dTv);
  for (int i = 0; i < TAB_N; i++)
    if (dTiles[i] == act && dActTile != i) { dActTile = i; dSetChip(i); dEnter(i); }
}
static void dChipEvent(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i != dActTile) lv_obj_set_tile_id(dTv, i, 0, LV_ANIM_ON);   // smooth horizontal slide
}

// ---------------- gauge (arc) with animated value ----------------
static void arcExec(void* var, int32_t v) {
  lv_obj_t* arc = (lv_obj_t*)var;
  lv_arc_set_value(arc, v);
  lv_obj_t* l = (lv_obj_t*)lv_obj_get_user_data(arc);
  if (l) lv_label_set_text_fmt(l, "%d%%", (int)v);
  lv_obj_set_style_arc_color(arc, v >= 90 ? K(C_RED) : (v >= 70 ? K(C_ORG) : K(C_CY)), LV_PART_INDICATOR);
}
static void arcTo(lv_obj_t* arc, int to) { dAnim(arc, arcExec, lv_arc_get_value(arc), to, 650); }

static lv_obj_t* dGauge(int tab, int x, int y, int w, int h, const char* cap, lv_obj_t** sub) {
  lv_obj_t* c = dCard(tab, x, y, w, h);
  lv_obj_t* arc = lv_arc_create(c);
  lv_obj_set_size(arc, 78, 78);
  lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 6);
  lv_arc_set_rotation(arc, 135);
  lv_arc_set_bg_angles(arc, 0, 270);
  lv_arc_set_range(arc, 0, 100);
  lv_arc_set_value(arc, 0);
  lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, K(C_TRK), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, K(C_CY), LV_PART_INDICATOR);
  lv_obj_t* v = dLbl(arc, "0%", &lv_font_montserrat_16, C_TX);
  lv_obj_align(v, LV_ALIGN_CENTER, 0, -4);
  lv_obj_t* cl = dLbl(arc, cap, &lv_font_montserrat_12, C_MUT);
  lv_obj_align(cl, LV_ALIGN_CENTER, 0, 12);
  lv_obj_set_user_data(arc, v);
  *sub = dLbl(c, "", &lv_font_montserrat_12, C_MUT);
  lv_obj_set_width(*sub, w - 8);
  lv_obj_set_style_text_align(*sub, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(*sub, LV_ALIGN_BOTTOM_MID, 0, -6);
  return arc;
}

// label (left) + value (right) in a small card
static lv_obj_t* dStatCard(int tab, int x, int y, int w, int h, const char* cap, const lv_font_t* f) {
  lv_obj_t* c = dCard(tab, x, y, w, h);
  lv_obj_t* l = dLbl(c, cap, &lv_font_montserrat_12, C_MUT);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 10, 0);
  lv_obj_t* v = dLbl(c, "-", f, C_TX);
  lv_obj_align(v, LV_ALIGN_RIGHT_MID, -10, 0);
  return v;
}

// card with caption on top-left, value top-right and an animated bar at the bottom
static lv_obj_t* dBarCard(int tab, int y, const char* cap, lv_obj_t** val, lv_obj_t** sub) {
  lv_obj_t* c = dCard(tab, 6, y, 308, 56);
  lv_obj_t* l = dLbl(c, cap, &lv_font_montserrat_12, C_MUT);
  lv_obj_align(l, LV_ALIGN_TOP_LEFT, 12, 7);
  *val = dLbl(c, "-", &lv_font_montserrat_16, C_TX);
  lv_obj_align(*val, LV_ALIGN_TOP_RIGHT, -12, 5);
  *sub = dLbl(c, "", &lv_font_montserrat_12, C_MUT);
  lv_obj_align(*sub, LV_ALIGN_TOP_LEFT, 12, 24);
  lv_obj_t* b = lv_bar_create(c);
  lv_obj_set_size(b, 284, 7);
  lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_bar_set_range(b, 0, 100);
  lv_obj_set_style_radius(b, 4, 0);
  lv_obj_set_style_radius(b, 4, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(b, K(C_TRK), LV_PART_MAIN);
  lv_obj_set_style_bg_color(b, K(C_CY), LV_PART_INDICATOR);
  lv_obj_set_style_anim_time(b, 600, 0);
  return b;
}
static void barSet(lv_obj_t* b, float pct) {
  int v = constrain((int)(pct + 0.5f), 0, 100);
  lv_bar_set_value(b, v, LV_ANIM_ON);
  lv_obj_set_style_bg_color(b, v >= 90 ? K(C_RED) : (v >= 75 ? K(C_ORG) : K(C_CY)), LV_PART_INDICATOR);
}

static lv_obj_t* dChartCard(int tab, int y, int h, lv_obj_t** chart, lv_chart_series_t** s1, uint32_t c1,
                            lv_chart_series_t** s2, uint32_t c2, const char* t1, const char* t2, int pts) {
  lv_obj_t* c = dCard(tab, 6, y, 308, h);
  lv_obj_t* a = dLbl(c, t1, &lv_font_montserrat_12, c1);
  lv_obj_align(a, LV_ALIGN_TOP_LEFT, 12, 6);
  if (t2) { lv_obj_t* b = dLbl(c, t2, &lv_font_montserrat_12, c2); lv_obj_align(b, LV_ALIGN_TOP_LEFT, 56, 6); }
  lv_obj_t* ch = lv_chart_create(c);
  lv_obj_set_size(ch, 288, h - 28);
  lv_obj_align(ch, LV_ALIGN_BOTTOM_MID, 0, -6);
  lv_chart_set_type(ch, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(ch, pts);
  lv_chart_set_range(ch, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_set_div_line_count(ch, 3, 0);
  lv_chart_set_update_mode(ch, LV_CHART_UPDATE_MODE_SHIFT);
  lv_obj_set_style_bg_opa(ch, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ch, 0, 0);
  lv_obj_set_style_pad_all(ch, 0, 0);
  lv_obj_set_style_line_color(ch, K(C_TRK), LV_PART_MAIN);
  lv_obj_set_style_size(ch, 0, LV_PART_INDICATOR);
  lv_obj_set_style_line_width(ch, 2, LV_PART_ITEMS);
  *s1 = lv_chart_add_series(ch, K(c1), LV_CHART_AXIS_PRIMARY_Y);
  if (t2) *s2 = lv_chart_add_series(ch, K(c2), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_all_value(ch, *s1, LV_CHART_POINT_NONE);
  if (t2) lv_chart_set_all_value(ch, *s2, LV_CHART_POINT_NONE);
  *chart = ch;
  return c;
}

// ================= TAB 0: OVERVIEW =================
static lv_obj_t *gCpu, *gRam, *gGpu, *gCpuS, *gRamS, *gGpuS, *oTemp, *oNet, *oDiskBar, *oDiskV;
static void dBuildOverview() {
  gCpu = dGauge(0, 6, 2, 100, 112, "CPU", &gCpuS);
  gRam = dGauge(0, 110, 2, 100, 112, "RAM", &gRamS);
  gGpu = dGauge(0, 214, 2, 100, 112, "GPU", &gGpuS);
  oTemp = dStatCard(0, 6, 118, 150, 30, "TEMP", &lv_font_montserrat_12);
  oNet  = dStatCard(0, 164, 118, 150, 30, "NET", &lv_font_montserrat_12);
  lv_obj_t* c = dCard(0, 6, 152, 308, 28);
  lv_obj_t* l = dLbl(c, "DISK", &lv_font_montserrat_12, C_MUT);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 12, 0);
  oDiskBar = lv_bar_create(c);
  lv_obj_set_size(oDiskBar, 190, 7);
  lv_obj_align(oDiskBar, LV_ALIGN_LEFT_MID, 54, 0);
  lv_bar_set_range(oDiskBar, 0, 100);
  lv_obj_set_style_radius(oDiskBar, 4, 0);
  lv_obj_set_style_radius(oDiskBar, 4, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(oDiskBar, K(C_TRK), LV_PART_MAIN);
  lv_obj_set_style_bg_color(oDiskBar, K(C_GRN), LV_PART_INDICATOR);
  lv_obj_set_style_anim_time(oDiskBar, 600, 0);
  oDiskV = dLbl(c, "-", &lv_font_montserrat_12, C_TX);
  lv_obj_align(oDiskV, LV_ALIGN_RIGHT_MID, -12, 0);
}
static void dUpdOverview() {
  char b[40];
  arcTo(gCpu, constrain((int)(S.cpu + 0.5f), 0, 100));
  arcTo(gRam, constrain((int)(S.ram + 0.5f), 0, 100));
  arcTo(gGpu, S.gpu >= 0 ? constrain((int)(S.gpu + 0.5f), 0, 100) : 0);
  snprintf(b, sizeof(b), "%.0f MHz", S.freq);                       lv_label_set_text(gCpuS, b);
  snprintf(b, sizeof(b), "%.1f/%.1f G", S.ramUsed, S.ramTotal);     lv_label_set_text(gRamS, b);
  if (S.gpu >= 0) snprintf(b, sizeof(b), "%.1f/%.0f G", S.gpuMemUsed, S.gpuMemTotal); else strcpy(b, "no GPU");
  lv_label_set_text(gGpuS, b);
  if (S.temp > 0 && S.gpuTemp > 0) snprintf(b, sizeof(b), "C %.0f  G %.0f", S.temp, S.gpuTemp);
  else if (S.temp > 0) snprintf(b, sizeof(b), "CPU %.0f C", S.temp);
  else if (S.gpuTemp > 0) snprintf(b, sizeof(b), "GPU %.0f C", S.gpuTemp);
  else strcpy(b, "-");
  lv_label_set_text(oTemp, b);
  String up = fmtRate(S.up), dn = fmtRate(S.down);
  snprintf(b, sizeof(b), "U %s D %s", up.c_str(), dn.c_str());
  lv_label_set_text(oNet, b);
  lv_bar_set_value(oDiskBar, constrain((int)S.disk, 0, 100), LV_ANIM_ON);
  lv_obj_set_style_bg_color(oDiskBar, S.disk >= 90 ? K(C_RED) : (S.disk >= 75 ? K(C_ORG) : K(C_GRN)), LV_PART_INDICATOR);
  snprintf(b, sizeof(b), "%.0f%%", S.disk);
  lv_label_set_text(oDiskV, b);
}

// ================= TAB 1: CPU =================
#define CH_N 60
static lv_obj_t *cChart, *cStat[3], *cCoreBars[16];
static lv_chart_series_t *cSerCpu, *cSerGpu;
static lv_obj_t* cCoreCard;
static void dBuildCpu() {
  dChartCard(1, 2, 96, &cChart, &cSerCpu, C_CY, &cSerGpu, C_VIO, "CPU", "GPU", CH_N);
  for (int i = 0; i < CH_N; i++) {
    lv_chart_set_next_value(cChart, cSerCpu, (lv_coord_t)cpuHist[HN - CH_N + i]);
    lv_chart_set_next_value(cChart, cSerGpu, (lv_coord_t)gpuHist[HN - CH_N + i]);
  }
  static const char* cap[3] = {"FREQ", "TEMP", "PROCS"};
  for (int i = 0; i < 3; i++) {
    lv_obj_t* c = dCard(1, 6 + i * 104, 102, 98, 44);
    lv_obj_t* l = dLbl(c, cap[i], &lv_font_montserrat_12, C_MUT);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 4);
    cStat[i] = dLbl(c, "-", &lv_font_montserrat_14, C_TX);
    lv_obj_align(cStat[i], LV_ALIGN_BOTTOM_MID, 0, -5);
  }
  cCoreCard = dCard(1, 6, 150, 308, 30);                         // per-core mini bars
  for (int i = 0; i < 16; i++) {
    cCoreBars[i] = lv_bar_create(cCoreCard);
    lv_obj_set_size(cCoreBars[i], 12, 20);
    lv_obj_set_pos(cCoreBars[i], 8 + i * 18, 4);
    lv_bar_set_range(cCoreBars[i], 0, 100);
    lv_obj_set_style_radius(cCoreBars[i], 3, 0);
    lv_obj_set_style_radius(cCoreBars[i], 3, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(cCoreBars[i], K(C_TRK), LV_PART_MAIN);
    lv_obj_set_style_bg_color(cCoreBars[i], K(C_CY), LV_PART_INDICATOR);
    lv_obj_set_style_anim_time(cCoreBars[i], 400, 0);
    lv_obj_add_flag(cCoreBars[i], LV_OBJ_FLAG_HIDDEN);
  }
}
static void dUpdCpu() {
  char b[24];
  lv_chart_set_next_value(cChart, cSerCpu, (lv_coord_t)constrain((int)S.cpu, 0, 100));
  lv_chart_set_next_value(cChart, cSerGpu, (lv_coord_t)(S.gpu >= 0 ? constrain((int)S.gpu, 0, 100) : 0));
  snprintf(b, sizeof(b), "%.0f MHz", S.freq);                  lv_label_set_text(cStat[0], b);
  if (S.temp > 0) snprintf(b, sizeof(b), "%.0f C", S.temp); else strcpy(b, "-");
  lv_label_set_text(cStat[1], b);
  if (S.temp > 0) lv_obj_set_style_text_color(cStat[1], S.temp >= 85 ? K(C_RED) : (S.temp >= 70 ? K(C_ORG) : K(C_GRN)), 0);
  snprintf(b, sizeof(b), "%u", (unsigned)S.nproc);             lv_label_set_text(cStat[2], b);
  int n = min((int)S.nCores, 16);
  for (int i = 0; i < 16; i++) {
    if (i < n) {
      lv_obj_clear_flag(cCoreBars[i], LV_OBJ_FLAG_HIDDEN);
      int v = constrain((int)S.cores[i], 0, 100);
      lv_bar_set_value(cCoreBars[i], v, LV_ANIM_ON);
      lv_obj_set_style_bg_color(cCoreBars[i], v >= 90 ? K(C_RED) : (v >= 70 ? K(C_ORG) : K(C_CY)), LV_PART_INDICATOR);
    } else lv_obj_add_flag(cCoreBars[i], LV_OBJ_FLAG_HIDDEN);
  }
}

// ================= TAB 2: MEMORY =================
static lv_obj_t *mBar[3], *mVal[3], *mSub[3];
static void dBuildMem() {
  mBar[0] = dBarCard(2, 2,   "RAM",  &mVal[0], &mSub[0]);
  mBar[1] = dBarCard(2, 62,  "SWAP", &mVal[1], &mSub[1]);
  mBar[2] = dBarCard(2, 122, "DISK", &mVal[2], &mSub[2]);
}
static void dUpdMem() {
  char b[40];
  snprintf(b, sizeof(b), "%.0f%%", S.ram);   lv_label_set_text(mVal[0], b);
  snprintf(b, sizeof(b), "%.1f of %.1f GB used", S.ramUsed, S.ramTotal); lv_label_set_text(mSub[0], b);
  barSet(mBar[0], S.ram);
  snprintf(b, sizeof(b), "%.0f%%", S.swap);  lv_label_set_text(mVal[1], b);
  lv_label_set_text(mSub[1], "page file / swap");
  barSet(mBar[1], S.swap);
  snprintf(b, sizeof(b), "%.0f%%", S.disk);  lv_label_set_text(mVal[2], b);
  if (S.nDrives > 0) snprintf(b, sizeof(b), "%s  %.0f / %.0f GB", S.drives[0].n, S.drives[0].u, S.drives[0].t);
  else strcpy(b, "system drive");
  lv_label_set_text(mSub[2], b);
  barSet(mBar[2], S.disk);
}

// ================= TAB 3: NETWORK =================
static lv_obj_t *nChart, *nDown, *nUp, *nPing, *nTot;
static lv_chart_series_t *nSerD, *nSerU;
static float nPeak = 200;                                          // KB/s, decays so the chart rescales
static void dBuildNet() {
  dChartCard(3, 2, 92, &nChart, &nSerD, C_CY, &nSerU, C_GRN, "DOWN", "UP", CH_N);
  nDown = dStatCard(3, 6, 98, 150, 30, "DOWN", &lv_font_montserrat_14);
  nUp   = dStatCard(3, 164, 98, 150, 30, "UP", &lv_font_montserrat_14);
  nPing = dStatCard(3, 6, 132, 150, 30, "PING", &lv_font_montserrat_14);
  lv_obj_t* c = dCard(3, 164, 132, 150, 30);
  lv_obj_t* l = dLbl(c, "TOTAL", &lv_font_montserrat_12, C_MUT);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 10, 0);
  nTot = dLbl(c, "-", &lv_font_montserrat_12, C_TX);
  lv_obj_align(nTot, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_obj_t* c2 = dCard(3, 6, 166, 308, 14);
  lv_obj_set_style_opa(c2, LV_OPA_TRANSP, 0);                      // spacer keeps the stagger count even
}
static void dUpdNet() {
  char b[40];
  nPeak = max(nPeak * 0.97f, max(max(S.down, S.up), 200.0f));
  lv_chart_set_range(nChart, LV_CHART_AXIS_PRIMARY_Y, 0, (lv_coord_t)nPeak);
  lv_chart_set_next_value(nChart, nSerD, (lv_coord_t)S.down);
  lv_chart_set_next_value(nChart, nSerU, (lv_coord_t)S.up);
  lv_label_set_text(nDown, fmtRate(S.down).c_str());
  lv_label_set_text(nUp, fmtRate(S.up).c_str());
  if (S.ping >= 0) { snprintf(b, sizeof(b), "%.0f ms", S.ping);
    lv_obj_set_style_text_color(nPing, S.ping < 40 ? K(C_GRN) : (S.ping < 100 ? K(C_ORG) : K(C_RED)), 0); }
  else { strcpy(b, "n/a"); lv_obj_set_style_text_color(nPing, K(C_MUT), 0); }
  lv_label_set_text(nPing, b);
  snprintf(b, sizeof(b), "%.1f/%.1f GB", S.rxGb, S.txGb);
  lv_label_set_text(nTot, b);
}

// ================= TAB 4: SYSTEM =================
static lv_obj_t *yVal[6], *ySlider;
static void dSliderEvent(lv_event_t* e) {
  lv_obj_t* s = lv_event_get_target(e);
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) { cfg.bright = (uint8_t)lv_slider_get_value(s); gLastAct = millis(); applyBrightness(); }
  else if (lv_event_get_code(e) == LV_EVENT_RELEASED) saveCfg();
}
static void dBuildSys() {
  static const char* Kc[6] = {"Network", "IP", "PC agent", "Signal", "Free RAM", "Last reset"};
  lv_obj_t* c = dCard(4, 6, 2, 308, 124);
  for (int i = 0; i < 6; i++) {
    lv_obj_t* k = dLbl(c, Kc[i], &lv_font_montserrat_12, C_MUT);
    lv_obj_align(k, LV_ALIGN_TOP_LEFT, 12, 7 + i * 19);
    yVal[i] = dLbl(c, "-", &lv_font_montserrat_12, C_TX);
    lv_obj_align(yVal[i], LV_ALIGN_TOP_RIGHT, -12, 7 + i * 19);
  }
  lv_obj_t* b = dCard(4, 6, 134, 308, 46);
  lv_obj_t* l = dLbl(b, "Brightness", &lv_font_montserrat_12, C_MUT);
  lv_obj_align(l, LV_ALIGN_TOP_LEFT, 12, 6);
  ySlider = lv_slider_create(b);
  lv_obj_set_size(ySlider, 280, 8);
  lv_obj_align(ySlider, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_slider_set_range(ySlider, 5, 255);
  lv_slider_set_value(ySlider, cfg.bright, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(ySlider, K(C_TRK), LV_PART_MAIN);
  lv_obj_set_style_bg_color(ySlider, K(C_CY), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(ySlider, K(C_TX), LV_PART_KNOB);
  lv_obj_set_style_pad_all(ySlider, 4, LV_PART_KNOB);
  lv_obj_add_flag(ySlider, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_set_ext_click_area(ySlider, 12);
  lv_obj_add_event_cb(ySlider, dSliderEvent, LV_EVENT_ALL, NULL);
}
static void dUpdSys() {
  char b[48];
  if (apMode && WiFi.status() != WL_CONNECTED) snprintf(b, sizeof(b), "SETUP AP (%d clients)", (int)WiFi.softAPgetStationNum());
  else snprintf(b, sizeof(b), "%s", cfg.ssid[0] ? cfg.ssid : "-");
  lv_label_set_text(yVal[0], b);
  lv_label_set_text(yVal[1], myIp().c_str());
  snprintf(b, sizeof(b), "%s:%u  %s", cfg.pcIp[0] ? cfg.pcIp : "-", (unsigned)cfg.pcPort, pcOnline ? "online" : "offline");
  lv_label_set_text(yVal[2], b);
  lv_obj_set_style_text_color(yVal[2], pcOnline ? K(C_GRN) : K(C_ORG), 0);
  if (WiFi.status() == WL_CONNECTED) snprintf(b, sizeof(b), "%d dBm", (int)WiFi.RSSI()); else strcpy(b, "-");
  lv_label_set_text(yVal[3], b);
  snprintf(b, sizeof(b), "%u KB (max %u KB)", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getMaxAllocHeap() / 1024));
  lv_label_set_text(yVal[4], b);
  lv_label_set_text(yVal[5], gResetWhy);
  if (!lv_obj_has_state(ySlider, LV_STATE_PRESSED)) lv_slider_set_value(ySlider, cfg.bright, LV_ANIM_OFF);
}

// ---------------- header ----------------
static void dUpdHeader() {
  static int lastSt = -1;
  int st = (apMode && WiFi.status() != WL_CONNECTED) ? 3 : (WiFi.status() != WL_CONNECTED ? 0 : (pcOnline ? 2 : 1));
  const char* host = (S.host[0] && S.host[0] != '-') ? S.host : "PC MONITOR";
  if (st != lastSt) {
    lastSt = st;
    lv_obj_set_style_bg_color(dDot, st == 2 ? K(C_GRN) : (st == 1 ? K(C_ORG) : K(C_RED)), 0);
    if (st == 3) lv_obj_set_tile_id(dTv, 4, 0, LV_ANIM_ON);          // WiFi setup: jump to SYSTEM (shows the AP IP)
  }
  lv_label_set_text(dTitle, st == 3 ? "WIFI SETUP" : (st == 0 ? "NO WIFI" : (st == 1 ? "NO PC" : host)));
  lv_label_set_text(dClock, S.clk);
}

// ---------------- init ----------------
static void dUiInit() {
  lv_init();
  size_t lines = 24, px = LV_HOR * lines;
  dBuf1 = (lv_color_t*)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  dBuf2 = (lv_color_t*)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  if (!dBuf1 || !dBuf2) {                                            // low RAM: smaller render blocks
    if (dBuf1) free(dBuf1);
    if (dBuf2) free(dBuf2);
    lines = 8; px = LV_HOR * lines;
    dBuf1 = (lv_color_t*)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    dBuf2 = (lv_color_t*)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  }
  Serial.printf("LVGL: %u lines x2 (%s) heap=%u\n", (unsigned)lines, (dBuf1 && dBuf2) ? "ok" : "FAILED", (unsigned)ESP.getFreeHeap());
  lv_disp_draw_buf_init(&dDraw, dBuf1, dBuf2, px);
  lv_disp_drv_init(&dDrv);
  dDrv.hor_res = LV_HOR; dDrv.ver_res = LV_VER;
  dDrv.flush_cb = dFlush; dDrv.draw_buf = &dDraw;
  lv_disp_drv_register(&dDrv);
  lv_indev_drv_init(&dIn);
  dIn.type = LV_INDEV_TYPE_POINTER;
  dIn.read_cb = dTouchRead;
  dIn.scroll_throw = 12;
  lv_indev_drv_register(&dIn);
  dStyles();

  lv_obj_t* scr = lv_scr_act();
  lv_obj_remove_style_all(scr);
  lv_obj_set_style_bg_color(scr, K(C_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // header: host + pulsing status dot + clock
  dTitle = dLbl(scr, "PC MONITOR", &lv_font_montserrat_14, C_CY);
  lv_obj_set_pos(dTitle, 10, 4);
  dClock = dLbl(scr, "--:--", &lv_font_montserrat_14, C_TX);
  lv_obj_align(dClock, LV_ALIGN_TOP_RIGHT, -10, 4);
  dDot = lv_obj_create(scr);
  lv_obj_remove_style_all(dDot);
  lv_obj_set_size(dDot, 8, 8);
  lv_obj_set_style_radius(dDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(dDot, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(dDot, K(C_RED), 0);
  lv_obj_set_pos(dDot, 236, 8);
  { lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a, dDot); lv_anim_set_exec_cb(&a, aPulse);
    lv_anim_set_values(&a, 255, 90); lv_anim_set_time(&a, 900); lv_anim_set_playback_time(&a, 900);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE); lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out); lv_anim_start(&a); }

  // tab chips
  static const char* names[TAB_N] = {"OVERVIEW", "CPU", "MEMORY", "NETWORK", "SYSTEM"};
  static const int cw[TAB_N] = {70, 40, 60, 66, 60};
  int x = 6;
  for (int i = 0; i < TAB_N; i++) {
    lv_obj_t* b = lv_btn_create(scr);
    lv_obj_remove_style_all(b);
    lv_obj_add_style(b, &sChip, 0);
    lv_obj_set_size(b, cw[i], 24);
    lv_obj_set_pos(b, x, 28);
    x += cw[i] + 4;
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, names[i]);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, dChipEvent, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    dChips[i] = b;
  }
  dSetChip(0);

  // swipeable pages
  dTv = lv_tileview_create(scr);
  lv_obj_remove_style_all(dTv);
  lv_obj_set_pos(dTv, 0, 58);
  lv_obj_set_size(dTv, LV_HOR, 182);
  lv_obj_set_scrollbar_mode(dTv, LV_SCROLLBAR_MODE_OFF);
  static const lv_dir_t dirs[TAB_N] = {LV_DIR_RIGHT, LV_DIR_HOR, LV_DIR_HOR, LV_DIR_HOR, LV_DIR_LEFT};
  for (int i = 0; i < TAB_N; i++) dTiles[i] = lv_tileview_add_tile(dTv, i, 0, dirs[i]);
  lv_obj_add_event_cb(dTv, dTabChanged, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(dTv, dTabChanged, LV_EVENT_SCROLL_END, NULL);

  dBuildOverview(); dBuildCpu(); dBuildMem(); dBuildNet(); dBuildSys();
  dUpdOverview(); dUpdCpu(); dUpdMem(); dUpdNet(); dUpdSys(); dUpdHeader();
  dEnter(0);                                                           // first page animates in too
  Serial.printf("LVGL dashboard ready: heap=%u maxblock=%u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
}

// ---------------- media hand-off (same behaviour as before) ----------------
static bool dWasMedia = false;
static void dMediaTick() {
  mediaService();
  if (curPage != P_MEDIA) return;
  dWasMedia = true;
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

// ---------------- entry points used by main.cpp ----------------
static void lvUiInit() { dUiInit(); }

static void lvUiTick() {
  if (curPage == P_MEDIA) { dMediaTick(); return; }
  if (dWasMedia) { dWasMedia = false; lv_obj_invalidate(lv_scr_act()); }
  mediaService();

  if (newData) {
    newData = false;
    takeSnapshot();
    dUpdOverview(); dUpdCpu(); dUpdMem(); dUpdNet();                   // cheap: only changes labels / bars / chart points
    dUpdHeader();
  }

  static uint32_t t1 = 0;
  if (millis() - t1 > 1000) {
    t1 = millis();
    if (gIdleMin && !gAsleep && !alertActive && millis() - gLastAct > (uint32_t)gIdleMin * 60000UL) gAsleep = true;
    applyBrightness();
    dUpdHeader();
    if (dActTile == 4) dUpdSys();
  }
  static uint32_t lastHeap = 0;
  if (millis() - lastHeap > 10000) { lastHeap = millis(); Serial.printf("heap=%u maxblock=%u wifi=%d\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(), (int)WiFi.status()); }

  uint32_t wait = lv_timer_handler();
  delay(wait > 6 ? 6 : (wait ? wait : 1));
}
#endif  // USE_LVGL
