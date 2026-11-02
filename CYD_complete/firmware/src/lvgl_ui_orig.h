// LVGL 8.3 user interface for the CYD (320x240, no PSRAM).  Built only with -DUSE_LVGL (env:cyd_lvgl).
// Included by main.cpp right before setup(), so it can use all the data / remote / WiFi code of the firmware.
//
//   swipe left / right  : Dashboard  <->  Details  <->  Remote  <->  System
//   Dashboard : three animated gauges (CPU / RAM / GPU), temperatures, network, disk
//   Details   : live CPU + GPU chart, temps, disk, ping, network speed
//   Remote    : your web-panel tabs and buttons (tap = send to the PC, danger buttons need a 2nd tap, hold = repeat)
//   System    : network info, free RAM, last reset reason, brightness slider
//   Media (video / GIF / image uploaded from the web panel) still plays full screen, a tap brings the UI back.
#pragma once
#ifdef USE_LVGL
#include <lvgl.h>

#define LV_HOR 320
#define LV_VER 240
#define LVC_BG   0x0B0F14
#define LVC_CARD 0x141A22
#define LVC_LINE 0x263040
#define LVC_TX   0xE8EEF5
#define LVC_MUT  0x8A98A8
#define LVC_CY   0x22D3EE
#define LVC_GRN  0x34D399
#define LVC_ORG  0xFBBF24
#define LVC_RED  0xF87171

static inline lv_color_t lvC(uint32_t h) { return lv_color_hex(h); }
static lv_color_t lv565(uint16_t c) { return lv_color_make(((c >> 11) & 31) * 255 / 31, ((c >> 5) & 63) * 255 / 63, (c & 31) * 255 / 31); }

// ---------------- display + touch drivers ----------------
static lv_disp_draw_buf_t lvDraw;
static lv_disp_drv_t lvDrv;
static lv_indev_drv_t lvInDrv;
static lv_color_t *lvBuf1 = nullptr, *lvBuf2 = nullptr;

static void lvFlush(lv_disp_drv_t* d, const lv_area_t* a, lv_color_t* c) {
  int32_t w = a->x2 - a->x1 + 1, h = a->y2 - a->y1 + 1;
  tft.waitDMA();                                   // previous block is still on its way: wait, then send this one by DMA
  tft.pushImageDMA(a->x1, a->y1, w, h, (const lgfx::rgb565_t*)c);
  lv_disp_flush_ready(d);                          // LVGL renders the next block into the OTHER buffer meanwhile
}

static void lvTouchRead(lv_indev_drv_t*, lv_indev_data_t* d) {
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
      if (gAsleep) { gAsleep = false; swallow = true; applyBrightness(); }   // the touch that wakes the screen is not a click
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
    } else if (pressed && ++miss >= 4) {            // SD / touch bus sharing: a single missed poll is not a release
      pressed = false; swallow = false;
    }
  }
  d->point.x = lx; d->point.y = ly;
  d->state = (pressed && !swallow) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// ---------------- shared styles + small helpers ----------------
static lv_style_t stCard, stBtn, stBtnPr, stPlain;
static lv_style_transition_dsc_t stTr;
static const lv_style_prop_t stTrProps[] = {LV_STYLE_TRANSFORM_WIDTH, LV_STYLE_TRANSFORM_HEIGHT, LV_STYLE_BG_COLOR, LV_STYLE_BORDER_COLOR, (lv_style_prop_t)0};

static void lvStylesInit() {
  lv_style_transition_dsc_init(&stTr, stTrProps, lv_anim_path_ease_out, 130, 0, NULL);

  lv_style_init(&stPlain);                         // transparent container without padding / border / scrollbar
  lv_style_set_bg_opa(&stPlain, LV_OPA_TRANSP);
  lv_style_set_border_width(&stPlain, 0);
  lv_style_set_pad_all(&stPlain, 0);
  lv_style_set_radius(&stPlain, 0);

  lv_style_init(&stCard);
  lv_style_set_bg_color(&stCard, lvC(LVC_CARD));
  lv_style_set_bg_opa(&stCard, LV_OPA_COVER);
  lv_style_set_radius(&stCard, 12);
  lv_style_set_border_color(&stCard, lvC(LVC_LINE));
  lv_style_set_border_width(&stCard, 1);
  lv_style_set_pad_all(&stCard, 0);

  lv_style_init(&stBtn);
  lv_style_set_bg_color(&stBtn, lvC(LVC_CARD));
  lv_style_set_bg_opa(&stBtn, LV_OPA_COVER);
  lv_style_set_radius(&stBtn, 12);
  lv_style_set_border_color(&stBtn, lvC(LVC_LINE));
  lv_style_set_border_width(&stBtn, 1);
  lv_style_set_shadow_width(&stBtn, 0);
  lv_style_set_pad_all(&stBtn, 3);
  lv_style_set_transition(&stBtn, &stTr);

  lv_style_init(&stBtnPr);
  lv_style_set_transform_width(&stBtnPr, -6);      // the button "sinks" when pressed
  lv_style_set_transform_height(&stBtnPr, -6);
}

static lv_obj_t* lvCard(lv_obj_t* p, int x, int y, int w, int h) {
  lv_obj_t* o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_add_style(o, &stCard, 0);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}
static lv_obj_t* lvLbl(lv_obj_t* p, const char* t, const lv_font_t* f, uint32_t col) {
  lv_obj_t* l = lv_label_create(p);
  lv_label_set_text(l, t);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, lvC(col), 0);
  return l;
}

// ---------------- animation helpers ----------------
static void lvXExec(void* v, int32_t x) { lv_obj_set_x((lv_obj_t*)v, x); }
static void lvYExec(void* v, int32_t y) { lv_obj_set_y((lv_obj_t*)v, y); }
static void lvWExec(void* v, int32_t w) { lv_obj_set_width((lv_obj_t*)v, w); }
static void lvOpaExec(void* v, int32_t o) { lv_obj_set_style_opa((lv_obj_t*)v, (lv_opa_t)o, 0); }
static void lvAnim(void* var, lv_anim_exec_xcb_t cb, int32_t from, int32_t to, uint32_t ms, lv_anim_path_cb_t path = lv_anim_path_ease_out, uint32_t delay = 0) {
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

// gauge value animation: arc + number + colour all follow the same animated value
static void lvArcExec(void* var, int32_t v) {
  lv_obj_t* arc = (lv_obj_t*)var;
  lv_arc_set_value(arc, v);
  lv_obj_t* l = (lv_obj_t*)lv_obj_get_user_data(arc);
  if (l) lv_label_set_text_fmt(l, "%d%%", (int)v);
  lv_obj_set_style_arc_color(arc, v >= 90 ? lvC(LVC_RED) : (v >= 70 ? lvC(LVC_ORG) : lvC(LVC_CY)), LV_PART_INDICATOR);
}
static void lvAnimArc(lv_obj_t* arc, int to) { lvAnim(arc, lvArcExec, lv_arc_get_value(arc), to, 650); }

// ---------------- screen skeleton ----------------
static lv_obj_t *lvTv = nullptr, *lvTiles[4];
static lv_obj_t *lvTitle, *lvClock, *lvDot, *lvDots[4];
static lv_obj_t *lvToastO = nullptr, *lvToastL = nullptr;
static lv_timer_t* lvToastT = nullptr;
static bool lvWasMedia = false;

// ---- toast (slides up from the bottom, hides itself) ----
static void lvToastHide(lv_timer_t*) {
  lvToastT = nullptr;                              // repeat count 1: LVGL deletes the timer itself
  lvAnim(lvToastO, lvYExec, lv_obj_get_y(lvToastO), LV_VER + 4, 220, lv_anim_path_ease_in);
}
static void lvToast(const char* txt, uint32_t col, uint32_t ms = 1400) {
  if (!lvToastO) return;
  lv_label_set_text(lvToastL, txt);
  lv_obj_set_style_border_color(lvToastO, lvC(col), 0);
  lv_obj_set_style_text_color(lvToastL, lvC(col), 0);
  lv_obj_align(lvToastL, LV_ALIGN_CENTER, 0, 0);
  lvAnim(lvToastO, lvYExec, lv_obj_get_y(lvToastO), 176, 260, lv_anim_path_overshoot);
  if (lvToastT) lv_timer_del(lvToastT);
  lvToastT = lv_timer_create(lvToastHide, ms, NULL);
  lv_timer_set_repeat_count(lvToastT, 1);
}

// ---------------- DASHBOARD ----------------
static lv_obj_t *gCpu, *gRam, *gGpu, *gCpuSub, *gRamSub, *gGpuSub;
static lv_obj_t *dTempV, *dNetV, *dDiskBar, *dDiskV;

static lv_obj_t* lvGauge(lv_obj_t* parent, int x, const char* cap, lv_obj_t** subOut) {
  lv_obj_t* arc = lv_arc_create(parent);
  lv_obj_set_size(arc, 92, 92);
  lv_obj_set_pos(arc, x, 2);
  lv_arc_set_rotation(arc, 135);
  lv_arc_set_bg_angles(arc, 0, 270);
  lv_arc_set_range(arc, 0, 100);
  lv_arc_set_value(arc, 0);
  lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(arc, 9, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, 9, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, lvC(0x1E2733), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, lvC(LVC_CY), LV_PART_INDICATOR);
  lv_obj_t* v = lvLbl(arc, "0%", &lv_font_montserrat_20, LVC_TX);
  lv_obj_align(v, LV_ALIGN_CENTER, 0, -4);
  lv_obj_t* c = lvLbl(arc, cap, &lv_font_montserrat_12, LVC_MUT);
  lv_obj_align(c, LV_ALIGN_CENTER, 0, 16);
  lv_obj_set_user_data(arc, v);
  lv_obj_t* s = lvLbl(parent, "", &lv_font_montserrat_12, LVC_MUT);
  lv_obj_set_width(s, 100);
  lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(s, x - 4, 96);
  *subOut = s;
  return arc;
}

static void lvBuildDash(lv_obj_t* t) {
  gCpu = lvGauge(t, 8, "CPU", &gCpuSub);
  gRam = lvGauge(t, 114, "RAM", &gRamSub);
  gGpu = lvGauge(t, 220, "GPU", &gGpuSub);

  lv_obj_t* c1 = lvCard(t, 6, 118, 150, 34);
  lvLbl(c1, "TEMP", &lv_font_montserrat_12, LVC_MUT);
  lv_obj_align(lv_obj_get_child(c1, 0), LV_ALIGN_LEFT_MID, 10, 0);
  dTempV = lvLbl(c1, "-", &lv_font_montserrat_14, LVC_TX);
  lv_obj_align(dTempV, LV_ALIGN_RIGHT_MID, -10, 0);

  lv_obj_t* c2 = lvCard(t, 164, 118, 150, 34);
  lvLbl(c2, "NET", &lv_font_montserrat_12, LVC_MUT);
  lv_obj_align(lv_obj_get_child(c2, 0), LV_ALIGN_LEFT_MID, 10, 0);
  dNetV = lvLbl(c2, "-", &lv_font_montserrat_12, LVC_TX);
  lv_obj_align(dNetV, LV_ALIGN_RIGHT_MID, -8, 0);

  lv_obj_t* c3 = lvCard(t, 6, 158, 308, 34);
  lv_obj_t* dl = lvLbl(c3, "DISK", &lv_font_montserrat_12, LVC_MUT);
  lv_obj_align(dl, LV_ALIGN_LEFT_MID, 10, 0);
  dDiskBar = lv_bar_create(c3);
  lv_obj_set_size(dDiskBar, 190, 8);
  lv_obj_align(dDiskBar, LV_ALIGN_LEFT_MID, 52, 0);
  lv_bar_set_range(dDiskBar, 0, 100);
  lv_obj_set_style_bg_color(dDiskBar, lvC(0x1E2733), LV_PART_MAIN);
  lv_obj_set_style_bg_color(dDiskBar, lvC(LVC_GRN), LV_PART_INDICATOR);
  lv_obj_set_style_anim_time(dDiskBar, 600, 0);
  dDiskV = lvLbl(c3, "-", &lv_font_montserrat_12, LVC_TX);
  lv_obj_align(dDiskV, LV_ALIGN_RIGHT_MID, -10, 0);
}

static void lvUpdateDash() {
  char b[40];
  lvAnimArc(gCpu, constrain((int)(S.cpu + 0.5f), 0, 100));
  lvAnimArc(gRam, constrain((int)(S.ram + 0.5f), 0, 100));
  lvAnimArc(gGpu, S.gpu >= 0 ? constrain((int)(S.gpu + 0.5f), 0, 100) : 0);
  snprintf(b, sizeof(b), "%.0f MHz", S.freq);              lv_label_set_text(gCpuSub, b);
  snprintf(b, sizeof(b), "%.1f / %.1f G", S.ramUsed, S.ramTotal); lv_label_set_text(gRamSub, b);
  if (S.gpu >= 0) snprintf(b, sizeof(b), "%.1f / %.0f G", S.gpuMemUsed, S.gpuMemTotal); else strcpy(b, "no GPU");
  lv_label_set_text(gGpuSub, b);
  if (S.temp > 0 && S.gpuTemp > 0) snprintf(b, sizeof(b), "C %.0f  G %.0f", S.temp, S.gpuTemp);
  else if (S.temp > 0) snprintf(b, sizeof(b), "CPU %.0f C", S.temp);
  else if (S.gpuTemp > 0) snprintf(b, sizeof(b), "GPU %.0f C", S.gpuTemp);
  else strcpy(b, "-");
  lv_label_set_text(dTempV, b);
  String up = fmtRate(S.up), dn = fmtRate(S.down);
  snprintf(b, sizeof(b), "U %s D %s", up.c_str(), dn.c_str());
  lv_label_set_text(dNetV, b);
  lv_bar_set_value(dDiskBar, constrain((int)S.disk, 0, 100), LV_ANIM_ON);
  lv_obj_set_style_bg_color(dDiskBar, S.disk >= 90 ? lvC(LVC_RED) : (S.disk >= 75 ? lvC(LVC_ORG) : lvC(LVC_GRN)), LV_PART_INDICATOR);
  snprintf(b, sizeof(b), "%.0f%%", S.disk);
  lv_label_set_text(dDiskV, b);
}

// ---------------- DETAILS ----------------
#define LV_CH_N 60
static lv_obj_t *dChart, *dStat[4];
static lv_chart_series_t *serCpu, *serGpu;

static lv_obj_t* lvMini(lv_obj_t* p, int x, const char* cap, lv_obj_t** val) {
  lv_obj_t* c = lvCard(p, x, 112, 74, 46);
  lv_obj_t* l = lvLbl(c, cap, &lv_font_montserrat_12, LVC_MUT);
  lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 5);
  *val = lvLbl(c, "-", &lv_font_montserrat_16, LVC_TX);
  lv_obj_align(*val, LV_ALIGN_BOTTOM_MID, 0, -5);
  return c;
}
static void lvBuildDetails(lv_obj_t* t) {
  lv_obj_t* c = lvCard(t, 6, 4, 308, 102);
  lv_obj_t* a = lvLbl(c, "CPU", &lv_font_montserrat_12, LVC_CY);
  lv_obj_align(a, LV_ALIGN_TOP_LEFT, 10, 5);
  lv_obj_t* g = lvLbl(c, "GPU", &lv_font_montserrat_12, LVC_ORG);
  lv_obj_align(g, LV_ALIGN_TOP_LEFT, 44, 5);
  dChart = lv_chart_create(c);
  lv_obj_set_size(dChart, 292, 76);
  lv_obj_align(dChart, LV_ALIGN_BOTTOM_MID, 0, -6);
  lv_chart_set_type(dChart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(dChart, LV_CH_N);
  lv_chart_set_range(dChart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_set_div_line_count(dChart, 3, 0);
  lv_chart_set_update_mode(dChart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_obj_set_style_bg_opa(dChart, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(dChart, 0, 0);
  lv_obj_set_style_pad_all(dChart, 0, 0);
  lv_obj_set_style_line_color(dChart, lvC(0x1E2733), LV_PART_MAIN);
  lv_obj_set_style_size(dChart, 0, LV_PART_INDICATOR);
  lv_obj_set_style_line_width(dChart, 2, LV_PART_ITEMS);
  serCpu = lv_chart_add_series(dChart, lvC(LVC_CY), LV_CHART_AXIS_PRIMARY_Y);
  serGpu = lv_chart_add_series(dChart, lvC(LVC_ORG), LV_CHART_AXIS_PRIMARY_Y);
  for (int i = 0; i < LV_CH_N; i++) {
    lv_chart_set_next_value(dChart, serCpu, (lv_coord_t)cpuHist[HN - LV_CH_N + i]);
    lv_chart_set_next_value(dChart, serGpu, (lv_coord_t)gpuHist[HN - LV_CH_N + i]);
  }
  lvMini(t, 6, "CPU TEMP", &dStat[0]);
  lvMini(t, 83, "GPU TEMP", &dStat[1]);
  lvMini(t, 160, "SWAP", &dStat[2]);
  lvMini(t, 237, "PING", &dStat[3]);
}
static void lvUpdateDetails() {
  char b[24];
  lv_chart_set_next_value(dChart, serCpu, (lv_coord_t)constrain((int)S.cpu, 0, 100));
  lv_chart_set_next_value(dChart, serGpu, (lv_coord_t)(S.gpu >= 0 ? constrain((int)S.gpu, 0, 100) : 0));
  if (S.temp > 0) snprintf(b, sizeof(b), "%.0f C", S.temp); else strcpy(b, "-");
  lv_label_set_text(dStat[0], b);
  if (S.gpuTemp > 0) snprintf(b, sizeof(b), "%.0f C", S.gpuTemp); else strcpy(b, "-");
  lv_label_set_text(dStat[1], b);
  snprintf(b, sizeof(b), "%.0f%%", S.swap);
  lv_label_set_text(dStat[2], b);
  if (S.ping >= 0) snprintf(b, sizeof(b), "%.0f ms", S.ping); else strcpy(b, "-");
  lv_label_set_text(dStat[3], b);
  lv_obj_set_style_text_color(dStat[3], S.ping < 0 ? lvC(LVC_MUT) : (S.ping < 40 ? lvC(LVC_GRN) : (S.ping < 100 ? lvC(LVC_ORG) : lvC(LVC_RED))), 0);
}

// ---------------- REMOTE ----------------
static lv_obj_t *rGrid, *rName, *rEmpty;
static uint16_t* lvIcoBuf[RMT_PER];
static lv_img_dsc_t lvIcoDsc[RMT_PER];
static volatile int lvSendBusy = 0, lvSendRes = -1;
static char lvSendId[64], lvSendLab[13];
static int lvConfirmIdx = -1;
static uint32_t lvConfirmUntil = 0;
static lv_obj_t* lvBtnObj[RMT_PER];

static void lvSendTask(void*) {
  lvSendRes = remoteSend(lvSendId, lvSendLab);     // blocks up to ~1 s on the network: that is why it runs in its own task
  lvSendBusy = 0;
  vTaskDelete(NULL);
}
static void lvConfirmClear() {
  if (lvConfirmIdx >= 0 && lvConfirmIdx < RMT_PER && lvBtnObj[lvConfirmIdx] && rmtTabs) {
    lv_obj_t* b = lvBtnObj[lvConfirmIdx];
    lv_obj_set_style_border_color(b, lv565(RMT_COL[rmtTab[rmtCur].b[lvConfirmIdx].col & 7]), 0);
    lv_obj_t* l = lv_obj_get_child(b, -1);
    if (l) lv_label_set_text(l, rmtTab[rmtCur].b[lvConfirmIdx].lab);
  }
  lvConfirmIdx = -1;
}
static void lvTap(int i, bool repeat) {
  if (!rmtTabs || i < 0 || i >= rmtTab[rmtCur].n) return;
  const RmtBtn& a = rmtTab[rmtCur].b[i];
  if (lvSendBusy) return;
  if (a.danger && !repeat) {
    if (!(lvConfirmIdx == i && millis() < lvConfirmUntil)) {          // dangerous: first tap only arms the button
      lvConfirmClear();
      lvConfirmIdx = i; lvConfirmUntil = millis() + 3000;
      lv_obj_set_style_border_color(lvBtnObj[i], lvC(LVC_ORG), 0);
      lv_obj_t* l = lv_obj_get_child(lvBtnObj[i], -1);
      if (l) lv_label_set_text(l, "TAP AGAIN");
      return;
    }
    lvConfirmClear();
  }
  strlcpy(lvSendId, a.id, sizeof(lvSendId));
  strlcpy(lvSendLab, a.lab, sizeof(lvSendLab));
  lvSendBusy = 1; lvSendRes = -1;
  if (xTaskCreatePinnedToCore(lvSendTask, "lvsend", 6144, NULL, 1, NULL, 0) != pdPASS) { lvSendBusy = 0; lvToast("Out of memory", LVC_RED); }
}
static void lvBtnEvent(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_CLICKED) lvTap(i, false);
  else if (c == LV_EVENT_LONG_PRESSED_REPEAT && rmtTabs && i < rmtTab[rmtCur].n && rmtTab[rmtCur].b[i].rep) lvTap(i, true);
}

static void lvRmtFreeIcons() {
  for (int i = 0; i < RMT_PER; i++) { if (lvIcoBuf[i]) { free(lvIcoBuf[i]); lvIcoBuf[i] = nullptr; } lvBtnObj[i] = nullptr; }
}
// dir: -1 / +1 = slide direction of the tab change, 0 = no slide
static void lvRmtBuild(int dir) {
  lvConfirmIdx = -1;
  lv_obj_clean(rGrid);
  lvRmtFreeIcons();
  if (rmtTabs <= 0) {
    lv_label_set_text(rName, "REMOTE");
    lv_obj_clear_flag(rEmpty, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_add_flag(rEmpty, LV_OBJ_FLAG_HIDDEN);
  if (rmtCur >= rmtTabs) rmtCur = rmtTabs - 1;
  if (rmtCur < 0) rmtCur = 0;
  char hb[24];
  snprintf(hb, sizeof(hb), "%s  %d/%d", rmtTab[rmtCur].name, rmtCur + 1, rmtTabs);
  lv_label_set_text(rName, hb);
  RmtTab& T = rmtTab[rmtCur];
  for (int i = 0; i < T.n && i < RMT_PER; i++) {
    const RmtBtn& a = T.b[i];
    lv_color_t col = lv565(RMT_COL[a.col & 7]);
    lv_obj_t* b = lv_btn_create(rGrid);
    lvBtnObj[i] = b;
    lv_obj_remove_style_all(b);
    lv_obj_add_style(b, &stBtn, 0);
    lv_obj_add_style(b, &stBtnPr, LV_STATE_PRESSED);
    lv_obj_set_size(b, 96, 60);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_border_color(b, a.danger ? lvC(LVC_RED) : col, 0);
    lv_obj_set_style_bg_color(b, col, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(b, 1, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_event_cb(b, lvBtnEvent, LV_EVENT_ALL, (void*)(intptr_t)i);
    bool hasIco = false;
    if (a.ico) {                                                       // 32x32 RGB565 icon from the flash file system
      uint16_t* buf = (uint16_t*)malloc(ICON_BYTES);
      if (buf && iconRead(a.ico, buf)) {
        for (int k = 0; k < ICON_W * ICON_W; k++) if (!buf[k]) buf[k] = 0x07E0;      // transparent -> chroma key (green)
        lvIcoBuf[i] = buf;
        lvIcoDsc[i].header.cf = LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED;
        lvIcoDsc[i].header.always_zero = 0;
        lvIcoDsc[i].header.w = ICON_W; lvIcoDsc[i].header.h = ICON_W;
        lvIcoDsc[i].data_size = ICON_BYTES;
        lvIcoDsc[i].data = (const uint8_t*)buf;
        lv_obj_t* im = lv_img_create(b);
        lv_img_set_src(im, &lvIcoDsc[i]);
        hasIco = true;
      } else if (buf) free(buf);
    }
    lv_obj_t* l = lvLbl(b, a.lab, hasIco ? &lv_font_montserrat_12 : &lv_font_montserrat_14, LVC_TX);
    lv_obj_set_width(l, 88);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  }
  lv_obj_scroll_to_y(rGrid, 0, LV_ANIM_OFF);
  if (dir) {                                                           // slide + fade the new tab in
    lvAnim(rGrid, lvXExec, dir * 46, 0, 220);
    lvAnim(rGrid, lvOpaExec, 0, 255, 220);
  }
}
static void lvRmtStep(int d) {
  if (rmtTabs <= 1) return;
  rmtCur = (rmtCur + d + rmtTabs) % rmtTabs;
  rmtConfirmSlot = -1;
  lvRmtBuild(d);
}
static void lvRmtNavEvent(lv_event_t* e) { if (lv_event_get_code(e) == LV_EVENT_CLICKED) lvRmtStep((int)(intptr_t)lv_event_get_user_data(e)); }

static lv_obj_t* lvNavBtn(lv_obj_t* p, int x, const char* sym, int dir) {
  lv_obj_t* b = lv_btn_create(p);
  lv_obj_remove_style_all(b);
  lv_obj_add_style(b, &stBtn, 0);
  lv_obj_add_style(b, &stBtnPr, LV_STATE_PRESSED);
  lv_obj_set_size(b, 46, 28);
  lv_obj_set_pos(b, x, 2);
  lv_obj_set_style_bg_color(b, lvC(LVC_CY), LV_STATE_PRESSED);
  lv_obj_t* l = lvLbl(b, sym, &lv_font_montserrat_16, LVC_CY);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, lvRmtNavEvent, LV_EVENT_CLICKED, (void*)(intptr_t)dir);
  return b;
}
static void lvBuildRemote(lv_obj_t* t) {
  lvNavBtn(t, 6, "<", -1);
  lvNavBtn(t, 268, ">", 1);
  rName = lvLbl(t, "REMOTE", &lv_font_montserrat_16, LVC_TX);
  lv_obj_set_width(rName, 200);
  lv_obj_set_style_text_align(rName, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(rName, 60, 6);
  rGrid = lv_obj_create(t);
  lv_obj_remove_style_all(rGrid);
  lv_obj_set_pos(rGrid, 0, 34);
  lv_obj_set_size(rGrid, LV_HOR, 160);
  lv_obj_set_flex_flow(rGrid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_all(rGrid, 6, 0);
  lv_obj_set_style_pad_row(rGrid, 6, 0);
  lv_obj_set_style_pad_column(rGrid, 6, 0);
  lv_obj_set_scroll_dir(rGrid, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(rGrid, LV_SCROLLBAR_MODE_OFF);
  rEmpty = lvLbl(t, "No tabs yet.\nAdd them in the web panel (Remote tab).", &lv_font_montserrat_14, LVC_MUT);
  lv_obj_set_style_text_align(rEmpty, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(rEmpty, LV_ALIGN_CENTER, 0, 10);
  lvRmtBuild(0);
}

// ---------------- SYSTEM ----------------
static lv_obj_t *sVal[6], *sSlider;
static void lvSliderEvent(lv_event_t* e) {
  lv_obj_t* s = lv_event_get_target(e);
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) { cfg.bright = (uint8_t)lv_slider_get_value(s); gLastAct = millis(); applyBrightness(); }
  else if (lv_event_get_code(e) == LV_EVENT_RELEASED) saveCfg();
}
static void lvBuildSystem(lv_obj_t* t) {
  static const char* K[6] = {"Network", "IP", "PC agent", "Signal", "Free RAM", "Last reset"};
  lv_obj_t* c = lvCard(t, 6, 4, 308, 128);
  for (int i = 0; i < 6; i++) {
    lv_obj_t* k = lvLbl(c, K[i], &lv_font_montserrat_12, LVC_MUT);
    lv_obj_align(k, LV_ALIGN_TOP_LEFT, 12, 7 + i * 19);
    sVal[i] = lvLbl(c, "-", &lv_font_montserrat_12, LVC_TX);
    lv_obj_align(sVal[i], LV_ALIGN_TOP_RIGHT, -12, 7 + i * 19);
  }
  lv_obj_t* b = lvCard(t, 6, 138, 308, 54);
  lv_obj_t* l = lvLbl(b, "Brightness", &lv_font_montserrat_12, LVC_MUT);
  lv_obj_align(l, LV_ALIGN_TOP_LEFT, 12, 7);
  sSlider = lv_slider_create(b);
  lv_obj_set_size(sSlider, 280, 8);
  lv_obj_align(sSlider, LV_ALIGN_BOTTOM_MID, 0, -14);
  lv_slider_set_range(sSlider, 5, 255);
  lv_slider_set_value(sSlider, cfg.bright, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(sSlider, lvC(0x1E2733), LV_PART_MAIN);
  lv_obj_set_style_bg_color(sSlider, lvC(LVC_CY), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(sSlider, lvC(LVC_TX), LV_PART_KNOB);
  lv_obj_set_style_pad_all(sSlider, 4, LV_PART_KNOB);
  lv_obj_add_flag(sSlider, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_set_ext_click_area(sSlider, 12);
  lv_obj_add_event_cb(sSlider, lvSliderEvent, LV_EVENT_ALL, NULL);
}
static void lvUpdateSys() {
  char b[48];
  if (apMode && WiFi.status() != WL_CONNECTED) snprintf(b, sizeof(b), "SETUP AP (%d clients)", (int)WiFi.softAPgetStationNum());
  else snprintf(b, sizeof(b), "%s", cfg.ssid[0] ? cfg.ssid : "-");
  lv_label_set_text(sVal[0], b);
  lv_label_set_text(sVal[1], myIp().c_str());
  snprintf(b, sizeof(b), "%s:%u  %s", cfg.pcIp[0] ? cfg.pcIp : "-", (unsigned)cfg.pcPort, pcOnline ? "online" : "offline");
  lv_label_set_text(sVal[2], b);
  lv_obj_set_style_text_color(sVal[2], pcOnline ? lvC(LVC_GRN) : lvC(LVC_ORG), 0);
  if (WiFi.status() == WL_CONNECTED) snprintf(b, sizeof(b), "%d dBm", (int)WiFi.RSSI()); else strcpy(b, "-");
  lv_label_set_text(sVal[3], b);
  snprintf(b, sizeof(b), "%u KB (max block %u KB)", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getMaxAllocHeap() / 1024));
  lv_label_set_text(sVal[4], b);
  lv_label_set_text(sVal[5], gResetWhy);
  if (!lv_obj_has_state(sSlider, LV_STATE_PRESSED)) lv_slider_set_value(sSlider, cfg.bright, LV_ANIM_OFF);
}

// ---------------- header + page dots ----------------
static int lvActTile = 0;
static void lvTvEvent(lv_event_t*) {
  lv_obj_t* act = lv_tileview_get_tile_act(lvTv);
  for (int i = 0; i < 4; i++) {
    if (lvTiles[i] == act && lvActTile != i) {
      lvActTile = i;
      for (int k = 0; k < 4; k++) {
        lvAnim(lvDots[k], lvWExec, lv_obj_get_width(lvDots[k]), k == i ? 20 : 6, 220);
        lv_obj_set_style_bg_color(lvDots[k], k == i ? lvC(LVC_CY) : lvC(0x3A4656), 0);
      }
      lvConfirmClear();
    }
  }
}
static void lvUpdateHeader() {
  static int lastSt = -1;
  int st = (apMode && WiFi.status() != WL_CONNECTED) ? 3 : (WiFi.status() != WL_CONNECTED ? 0 : (pcOnline ? 2 : 1));
  if (st != lastSt) {
    lastSt = st;
    lv_obj_set_style_bg_color(lvDot, st == 2 ? lvC(LVC_GRN) : (st == 1 ? lvC(LVC_ORG) : lvC(LVC_RED)), 0);
    if (st == 3) lv_obj_set_tile_id(lvTv, 3, 0, LV_ANIM_ON);                 // WiFi setup mode: show the System page (IP of the setup AP)
    lv_label_set_text(lvTitle, st == 3 ? "WIFI SETUP" : (st == 0 ? "NO WIFI" : (st == 1 ? "NO PC" : (S.host[0] && S.host[0] != '-' ? S.host : "PC MONITOR"))));
  } else if (st == 2) {
    lv_label_set_text(lvTitle, S.host[0] && S.host[0] != '-' ? S.host : "PC MONITOR");
  }
  lv_label_set_text(lvClock, S.clk);
}

static void lvPulseExec(void* v, int32_t o) { lv_obj_set_style_bg_opa((lv_obj_t*)v, (lv_opa_t)o, 0); }

static void lvUiInit() {
  lv_init();
  size_t lines = 24, px = LV_HOR * lines;
  lvBuf1 = (lv_color_t*)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  lvBuf2 = (lv_color_t*)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  if (!lvBuf1 || !lvBuf2) {                                            // low RAM: smaller render blocks instead of failing
    if (lvBuf1) free(lvBuf1);
    if (lvBuf2) free(lvBuf2);
    lines = 8; px = LV_HOR * lines;
    lvBuf1 = (lv_color_t*)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    lvBuf2 = (lv_color_t*)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  }
  Serial.printf("LVGL: draw buffers %u lines x2 (%s), heap=%u maxblock=%u\n", (unsigned)lines, (lvBuf1 && lvBuf2) ? "ok" : "FAILED", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  lv_disp_draw_buf_init(&lvDraw, lvBuf1, lvBuf2, px);
  lv_disp_drv_init(&lvDrv);
  lvDrv.hor_res = LV_HOR;
  lvDrv.ver_res = LV_VER;
  lvDrv.flush_cb = lvFlush;
  lvDrv.draw_buf = &lvDraw;
  lv_disp_drv_register(&lvDrv);
  lv_indev_drv_init(&lvInDrv);
  lvInDrv.type = LV_INDEV_TYPE_POINTER;
  lvInDrv.read_cb = lvTouchRead;
  lvInDrv.scroll_throw = 12;                       // swipe momentum
  lvInDrv.long_press_time = 400;
  lvInDrv.long_press_repeat_time = 140;            // hold-to-repeat buttons
  lv_indev_drv_register(&lvInDrv);
  lvStylesInit();

  lv_obj_t* scr = lv_scr_act();
  lv_obj_remove_style_all(scr);
  lv_obj_set_style_bg_color(scr, lvC(LVC_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // header
  lvTitle = lvLbl(scr, "PC MONITOR", &lv_font_montserrat_14, LVC_CY);
  lv_obj_set_pos(lvTitle, 10, 5);
  lvClock = lvLbl(scr, "--:--", &lv_font_montserrat_14, LVC_TX);
  lv_obj_align(lvClock, LV_ALIGN_TOP_RIGHT, -10, 5);
  lvDot = lv_obj_create(scr);
  lv_obj_remove_style_all(lvDot);
  lv_obj_set_size(lvDot, 8, 8);
  lv_obj_set_style_radius(lvDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(lvDot, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(lvDot, lvC(LVC_RED), 0);
  lv_obj_set_pos(lvDot, 244, 9);
  { lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a, lvDot); lv_anim_set_exec_cb(&a, lvPulseExec);
    lv_anim_set_values(&a, 255, 90); lv_anim_set_time(&a, 900); lv_anim_set_playback_time(&a, 900);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE); lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out); lv_anim_start(&a); }

  // pages
  lvTv = lv_tileview_create(scr);
  lv_obj_remove_style_all(lvTv);
  lv_obj_set_pos(lvTv, 0, 26);
  lv_obj_set_size(lvTv, LV_HOR, 196);
  lv_obj_set_scrollbar_mode(lvTv, LV_SCROLLBAR_MODE_OFF);
  static const lv_dir_t dirs[4] = {LV_DIR_RIGHT, LV_DIR_HOR, LV_DIR_HOR, LV_DIR_LEFT};
  for (int i = 0; i < 4; i++) lvTiles[i] = lv_tileview_add_tile(lvTv, i, 0, dirs[i]);
  lv_obj_add_event_cb(lvTv, lvTvEvent, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(lvTv, lvTvEvent, LV_EVENT_SCROLL_END, NULL);

  // page dots (flex row, the active one is wider and animates its width)
  lv_obj_t* dots = lv_obj_create(scr);
  lv_obj_remove_style_all(dots);
  lv_obj_set_size(dots, LV_HOR, 16);
  lv_obj_set_pos(dots, 0, 224);
  lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(dots, 6, 0);
  lv_obj_clear_flag(dots, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(dots, LV_OBJ_FLAG_CLICKABLE);
  for (int i = 0; i < 4; i++) {
    lvDots[i] = lv_obj_create(dots);
    lv_obj_remove_style_all(lvDots[i]);
    lv_obj_set_size(lvDots[i], i == 0 ? 20 : 6, 6);
    lv_obj_set_style_radius(lvDots[i], 3, 0);
    lv_obj_set_style_bg_opa(lvDots[i], LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(lvDots[i], i == 0 ? lvC(LVC_CY) : lvC(0x3A4656), 0);
  }

  // toast (on the top layer, parked below the screen)
  lvToastO = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(lvToastO);
  lv_obj_add_style(lvToastO, &stCard, 0);
  lv_obj_set_style_bg_color(lvToastO, lvC(0x1B2430), 0);
  lv_obj_set_style_border_width(lvToastO, 2, 0);
  lv_obj_set_size(lvToastO, 240, 32);
  lv_obj_set_pos(lvToastO, 40, LV_VER + 4);
  lv_obj_clear_flag(lvToastO, LV_OBJ_FLAG_CLICKABLE);
  lvToastL = lvLbl(lvToastO, "", &lv_font_montserrat_14, LVC_TX);

  lvBuildDash(lvTiles[0]);
  lvBuildDetails(lvTiles[1]);
  lvBuildRemote(lvTiles[2]);
  lvBuildSystem(lvTiles[3]);
  lvUpdateDash(); lvUpdateDetails(); lvUpdateSys(); lvUpdateHeader();
  lv_obj_fade_in(lvTv, 450, 0);                                        // soft start
  Serial.printf("LVGL UI ready: heap=%u maxblock=%u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
}

// ---------------- main loop hook ----------------
// video / GIF / image from the web panel: legacy full-screen player, tap = back to the UI
static void lvMediaTick() {
  mediaService();
  if (curPage != P_MEDIA) return;
  lvWasMedia = true;
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
  if (curPage == P_MEDIA) { lvMediaTick(); return; }
  if (lvWasMedia) { lvWasMedia = false; lv_obj_invalidate(lv_scr_act()); }
  mediaService();                                                      // pending uploads / "play after upload" start here (they switch to P_MEDIA)

  if (newData) {
    newData = false;
    takeSnapshot();
    lvUpdateDash();
    lvUpdateDetails();
    lvUpdateHeader();
  }
  if (gLvRmtDirty) { gLvRmtDirty = false; lvRmtBuild(0); }             // the web panel saved new remote tabs

  if (!lvSendBusy && lvSendRes >= 0) {                                 // answer of the background send task
    int r = lvSendRes; lvSendRes = -1;
    if (r == 1) lvToast(lvSendLab, LVC_GRN);
    else if (r == 2) lvToast("Wrong token", LVC_RED, 2200);
    else lvToast("PC not reachable", LVC_RED, 2200);
  }
  if (lvConfirmIdx >= 0 && millis() > lvConfirmUntil) lvConfirmClear();

  static uint32_t t1 = 0;
  if (millis() - t1 > 1000) {
    t1 = millis();
    if (gIdleMin && !gAsleep && !alertActive && millis() - gLastAct > (uint32_t)gIdleMin * 60000UL) gAsleep = true;
    applyBrightness();
    lvUpdateHeader();
    if (lvActTile == 3) lvUpdateSys();
  }
  static uint32_t lastHeap = 0;
  if (millis() - lastHeap > 10000) { lastHeap = millis(); Serial.printf("heap=%u maxblock=%u wifi=%d touch z=%d irq=%d\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(), (int)WiFi.status(), (int)gTouchZ, (int)gTouchIrq); }

  uint32_t wait = lv_timer_handler();
  delay(wait > 6 ? 6 : (wait ? wait : 1));
}
#endif  // USE_LVGL
