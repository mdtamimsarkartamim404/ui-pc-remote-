// LovyanGFX display layer for ESP32-2432S028R ("CYD", plain ESP32, NO PSRAM)
// 2.8" 240x320 ST7789 (use -DCYD_ILI9341 for the ILI9341 variant), landscape = 320x240.
//
// The original UI was laid out for 480x320.  Rather than rewrite 3000 lines of coordinates,
// the Canvas (page sprite) scales every logical 480x320 coordinate to the real 320x240 panel
// and maps the fonts to smaller ones.  The Display (real panel, used for media) is NOT scaled.
#pragma once
#define LGFX_USE_V1
#include <Arduino.h>
#include <LovyanGFX.hpp>

// ---- pins: fixed on the ESP32-2432S028R ----
#define PIN_TFT_SCK  14
#define PIN_TFT_MOSI 13
#define PIN_TFT_MISO 12
#define PIN_TFT_DC    2
#define PIN_TFT_CS   15
#define PIN_TFT_RST  -1          // tied to EN
#ifndef PIN_TFT_BL
#define PIN_TFT_BL   21          // some CYD revisions use 27: build with -DPIN_TFT_BL=27
#endif
#ifndef DISP_SPI_HZ
#define DISP_SPI_HZ 40000000     // ST7789 on the CYD is usually clean at 40 MHz; try 27000000 / 20000000 if noisy
#endif
#ifndef DISP_RGB_ORDER
#define DISP_RGB_ORDER 0
#endif
#ifndef DISP_INVERT
#define DISP_INVERT 0            // your CYD showed a NEGATIVE picture with 1, so 0 is the default (use env color_invert for the other way)
#endif

// logical UI size (what main.cpp draws in) and real panel size
#define UI_W 480
#define UI_H 320
// 8 = RGB332 sprite (76.8 KB) fits next to WiFi on a no-PSRAM ESP32; 16 (153.6 KB) normally does not
#ifndef SPRITE_DEPTH
#define SPRITE_DEPTH 8
#endif

class LGFX_Panel : public lgfx::LGFX_Device {
#ifdef CYD_ILI9341
  lgfx::Panel_ILI9341 _panel;
#else
  lgfx::Panel_ST7789 _panel;
#endif
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _light;
public:
  LGFX_Panel() {
    { auto c = _bus.config();
      c.spi_host = SPI2_HOST;           // HSPI: TFT on 14/13/12, the SD card uses VSPI
      c.spi_mode = 0;
      c.freq_write = DISP_SPI_HZ;
      c.freq_read = 16000000;
      c.spi_3wire = false;
      c.use_lock = true;
      c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = PIN_TFT_SCK;
      c.pin_mosi = PIN_TFT_MOSI;
      c.pin_miso = -1;                  // read-back not used (GPIO12 is a strapping pin anyway)
      c.pin_dc = PIN_TFT_DC;
      _bus.config(c);
      _panel.setBus(&_bus); }
    { auto c = _panel.config();
      c.pin_cs = PIN_TFT_CS;
      c.pin_rst = PIN_TFT_RST;
      c.pin_busy = -1;
      c.panel_width = 240;
      c.panel_height = 320;
      c.offset_x = 0; c.offset_y = 0;
      c.offset_rotation = 0;
      c.readable = false;
      c.invert = DISP_INVERT;
      c.rgb_order = DISP_RGB_ORDER;
      c.dlen_16bit = false;
      c.bus_shared = false;
      _panel.config(c); }
    { auto c = _light.config();
      c.pin_bl = PIN_TFT_BL;
      c.invert = false;
      c.freq = 5000;
      c.pwm_channel = 7;
      _light.config(c);
      _panel.setLight(&_light); }
    setPanel(&_panel);
  }
};

// colour names (only if the library did not already define them)
#ifndef TFT_BLACK
#define TFT_BLACK 0x0000
#endif
#ifndef TFT_NAVY
#define TFT_NAVY 0x000F
#endif
#ifndef TFT_DARKGREY
#define TFT_DARKGREY 0x7BEF
#endif
#ifndef TFT_LIGHTGREY
#define TFT_LIGHTGREY 0xD69A
#endif
#ifndef TFT_BLUE
#define TFT_BLUE 0x001F
#endif
#ifndef TFT_GREEN
#define TFT_GREEN 0x07E0
#endif
#ifndef TFT_CYAN
#define TFT_CYAN 0x07FF
#endif
#ifndef TFT_RED
#define TFT_RED 0xF800
#endif
#ifndef TFT_MAGENTA
#define TFT_MAGENTA 0xF81F
#endif
#ifndef TFT_YELLOW
#define TFT_YELLOW 0xFFE0
#endif
#ifndef TFT_WHITE
#define TFT_WHITE 0xFFFF
#endif
#ifndef TFT_ORANGE
#define TFT_ORANGE 0xFDA0
#endif

// TFT_eSPI-style datum numbers
#undef TL_DATUM
#undef TC_DATUM
#undef TR_DATUM
#undef ML_DATUM
#undef MC_DATUM
#undef MR_DATUM
#undef BL_DATUM
#undef BC_DATUM
#undef BR_DATUM
#define TL_DATUM 0
#define TC_DATUM 1
#define TR_DATUM 2
#define ML_DATUM 4
#define MC_DATUM 5
#define MR_DATUM 6
#define BL_DATUM 8
#define BC_DATUM 9
#define BR_DATUM 10

extern bool gDirty;     // set by Canvas drawing calls (sprite needs sending to the panel)

// SC = true  : coordinates/fonts are given in the logical 480x320 UI and scaled to the 320x240 panel
template <class B, bool TRACK, bool SC>
class TftApi : public B {
  struct FM { const lgfx::IFont* f; float m; };
  static FM fontFor(int f) {
    if (!SC) {                                  // real panel: same fonts as before
      switch (f) {
        case 2: return {&lgfx::fonts::Font2, 1.0f};
        case 4: return {&lgfx::fonts::Font4, 1.0f};
        case 6: return {&lgfx::fonts::Font6, 1.0f};
        case 7: return {&lgfx::fonts::Font7, 1.0f};
        case 8: return {&lgfx::fonts::Font8, 1.0f};
        default: return {&lgfx::fonts::Font0, 1.0f};
      }
    }
#ifdef CYD_BITMAP_FONTS                         // classic bitmap fonts, scaled (blockier, no extra font data)
    switch (f) {
      case 2: return {&lgfx::fonts::Font2, 0.75f};
      case 4: return {&lgfx::fonts::Font4, 0.70f};
      case 6: return {&lgfx::fonts::Font6, 0.70f};
      case 7: return {&lgfx::fonts::Font7, 0.70f};
      case 8: return {&lgfx::fonts::Font8, 0.70f};
      default: return {&lgfx::fonts::Font0, 1.0f};
    }
#else                                           // smooth DejaVu fonts, picked so text is never wider than the scaled 480-px layout allows
    switch (f) {
      case 2: return {&lgfx::fonts::DejaVu9, 1.0f};
      case 4: return {&lgfx::fonts::DejaVu12, 1.0f};
      case 6: return {&lgfx::fonts::DejaVu24, 1.0f};
      case 7: return {&lgfx::fonts::DejaVu24, 1.0f};
      case 8: return {&lgfx::fonts::DejaVu40, 1.0f};
      default: return {&lgfx::fonts::Font0, 1.0f};
    }
#endif
  }
  static inline void mark() { if (TRACK) gDirty = true; }
  // logical -> physical (rounded)
  static inline int32_t X(int32_t v) { return SC ? (v * 2 + 1) / 3 : v; }
  static inline int32_t Y(int32_t v) { return SC ? (v * 3 + 2) / 4 : v; }
  static inline int32_t R(int32_t v) { return SC ? (v * 2 + 1) / 3 : v; }
  // extent that avoids gaps between neighbouring scaled rectangles (never collapses to 0)
  static inline int32_t WX(int32_t x, int32_t w) { int32_t r = X(x + w) - X(x); return (w > 0 && r < 1) ? 1 : r; }
  static inline int32_t HY(int32_t y, int32_t h) { int32_t r = Y(y + h) - Y(y); return (h > 0 && r < 1) ? 1 : r; }
  float tsz_ = 1.0f;
public:
  using B::B;
  // LovyanGFX datum values: top_left=0, top_center=1, top_right=2, middle_left=4, middle_center=5,
  // middle_right=6, bottom_left=16, bottom_center=17, bottom_right=18
  void setTextDatum(int d) {
    static const uint8_t map[11] = {0, 1, 2, 0, 4, 5, 6, 0, 16, 17, 18};
    B::setTextDatum((lgfx::textdatum::textdatum_t)map[d < 0 || d > 10 ? 0 : d]);
  }
  void setTextSize(float s) { tsz_ = s; }
  void setTextPadding(uint32_t p) { B::setTextPadding(SC ? (uint32_t)X((int32_t)p) : p); }
  size_t drawString(const char* s, int32_t x, int32_t y, int font = 1) {
    mark();
    FM m = fontFor(font);
    B::setFont(m.f);
    B::setTextSize(m.m * tsz_);
    return B::drawString(s, X(x), Y(y));
  }
  size_t drawString(const String& s, int32_t x, int32_t y, int font = 1) { return drawString(s.c_str(), x, y, font); }
  void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) { mark(); B::fillRect(X(x), Y(y), WX(x, w), HY(y, h), (uint16_t)c); }
  void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) { mark(); B::drawRect(X(x), Y(y), WX(x, w), HY(y, h), (uint16_t)c); }
  void fillScreen(uint32_t c) { mark(); B::fillScreen((uint16_t)c); }
  void drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t c) { mark(); B::drawFastHLine(X(x), Y(y), WX(x, w), (uint16_t)c); }
  void drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t c) { mark(); B::drawFastVLine(X(x), Y(y), HY(y, h), (uint16_t)c); }
  void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t c) { mark(); B::drawLine(X(x0), Y(y0), X(x1), Y(y1), (uint16_t)c); }
  void drawCircle(int32_t x, int32_t y, int32_t r, uint32_t c) { mark(); B::drawCircle(X(x), Y(y), R(r), (uint16_t)c); }
  void fillCircle(int32_t x, int32_t y, int32_t r, uint32_t c) { mark(); B::fillCircle(X(x), Y(y), R(r), (uint16_t)c); }
  void setTextColor(uint32_t fg, uint32_t bg) { B::setTextColor((uint16_t)fg, (uint16_t)bg); }
  void setTextColor(uint32_t fg) { B::setTextColor((uint16_t)fg); }
  // RGB565 pixels in native (little-endian) order, as produced by TJpg_Decoder / AnimatedGIF / the web panel.
  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* d) {
    B::pushImage(x, y, w, h, (const lgfx::rgb565_t*)d);
  }
  void setSwapBytes(bool) {}
};
#ifdef CYD_USE_SPRITE
// page sprite in RAM (needs ~77 KB free internal RAM: can starve WiFi/web server on a no-PSRAM ESP32)
class Display : public TftApi<LGFX_Panel, false, false> {};
class Canvas : public TftApi<lgfx::LGFX_Sprite, true, true> {
public:
  explicit Canvas(lgfx::LovyanGFX* parent) : TftApi<lgfx::LGFX_Sprite, true, true>(parent) {}
};
#else
// DEFAULT on the CYD: pages are drawn straight to the panel (no RAM sprite) -> ~77 KB more free heap for WiFi + web UI
class Display : public TftApi<LGFX_Panel, true, true> {};
#endif
