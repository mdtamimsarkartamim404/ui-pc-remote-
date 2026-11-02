// XPT2046 resistive touch on the ESP32-2432S028R.
//  * DEFAULT (-DTOUCH_USE_LIB): the real XPT2046_Touchscreen library, exactly like the working remote-only build.
//    HSPI = display, VSPI = SD card (18/19/23/5) AND touch (25/39/32/33): the bus is time-shared, see touchBusAcquire().
//  * -UTOUCH_USE_LIB (env cyd_touch_bitbang): software SPI copy of the library, no bus sharing.
//   T_CLK=25  T_DIN(MOSI)=32  T_DO(MISO)=39  T_CS=33  T_IRQ=36
//
// This is a 1:1 software copy of what Paul Stoffregen's XPT2046_Touchscreen does (the library your working
// TFT_eSPI test uses): same command sequence, same pipelined 16-bit frames, same pressure formula, same
// best-two-of-three averaging, same default rotation 0 -> raw values are IDENTICAL to the working test,
// so the 5-point calibration below applies directly.
#pragma once
#include <Arduino.h>
#ifdef TOUCH_USE_LIB            // use the real XPT2046_Touchscreen library (same as the working TFT_eSPI test)
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#endif

#ifndef TOUCH_CLK
#define TOUCH_CLK  25
#endif
#ifndef TOUCH_MOSI
#define TOUCH_MOSI 32
#endif
#ifndef TOUCH_MISO
#define TOUCH_MISO 39
#endif
#ifndef TOUCH_CS
#define TOUCH_CS   33
#endif
#ifndef TOUCH_IRQ
#define TOUCH_IRQ  36
#endif
#ifndef TOUCH_X_MIN
#define TOUCH_X_MIN 396
#define TOUCH_X_MAX 3634
#define TOUCH_Y_MIN 461
#define TOUCH_Y_MAX 3575
#endif

// half-clock delay: keeps DCLK well under the XPT2046 limit (~500 kHz here, the library uses 2 MHz)
#ifndef TOUCH_CLK_US
#define TOUCH_CLK_US 1
#endif
#ifndef TOUCH_Z_MIN
#define TOUCH_Z_MIN 400          // same threshold as the library (Z_THRESHOLD)
#endif

static volatile int gTouchZ = 0, gTouchIrq = 1;   // last pressure / IRQ level (shown in the 10 s serial status line)

#ifdef TOUCH_USE_LIB
// ---- real XPT2046_Touchscreen library (what the working remote-only build uses) ----
// The SD card sits on the same VSPI host (pins 18/19/23/5) as the touch controller (25/39/32/33), so they are time-shared:
//   touchBusAcquire() = take the SD mutex + re-pin the shared SPIClass to the touch pins
//   touchBusRelease() = pin it back to the SD card + give the mutex back
// Both live in main.cpp (they need sdSpi / sdMtx). The library only talks SPI after the PENIRQ pin fell
// (tirqTouched), so an idle screen costs nothing and never touches the SD bus.
static XPT2046_Touchscreen touchLib(TOUCH_CS, TOUCH_IRQ);
static bool touchLibStarted = false;
static bool touchBusAcquire();
static void touchBusRelease();
static SPIClass* touchBusSpi();
static void touchInit() {}                       // the library is started lazily from the input task (it needs the bus)
static bool touchRaw(int& rx, int& ry) {
  gTouchIrq = digitalRead(TOUCH_IRQ) ? 1 : 0;
  if (touchLibStarted && !touchLib.tirqTouched()) { gTouchZ = 0; return false; }   // finger is up: no SPI traffic at all
  if (!touchBusAcquire()) return false;          // SD card busy (or not ready yet): try again on the next poll
  if (!touchLibStarted) { touchLib.begin(*touchBusSpi()); touchLibStarted = true; }  // default rotation 0, same as the test
  bool pressed = touchLib.touched();
  TS_Point p;
  if (pressed) p = touchLib.getPoint();
  touchBusRelease();
  if (!pressed) { gTouchZ = 0; return false; }
  gTouchZ = p.z;
  rx = p.x; ry = p.y;
  return true;
}
#else
static void xpt8(uint8_t v) {                      // like SPI.transfer(v), MSB first, mode 0
  for (int i = 0; i < 8; i++) {
    digitalWrite(TOUCH_MOSI, (v & 0x80) ? HIGH : LOW);
    v <<= 1;
    delayMicroseconds(TOUCH_CLK_US);
    digitalWrite(TOUCH_CLK, HIGH);
    delayMicroseconds(TOUCH_CLK_US);
    digitalWrite(TOUCH_CLK, LOW);
  }
}
// like SPI.transfer16(next): sends 0x00,next and returns the 16 bits clocked in = result (>>3) of the PREVIOUS command
static uint16_t xpt16(uint8_t next) {
  uint16_t out = next, in = 0;
  for (int i = 0; i < 16; i++) {
    digitalWrite(TOUCH_MOSI, (out & 0x8000) ? HIGH : LOW);
    out <<= 1;
    delayMicroseconds(TOUCH_CLK_US);
    digitalWrite(TOUCH_CLK, HIGH);
    delayMicroseconds(TOUCH_CLK_US);
    in = (in << 1) | (digitalRead(TOUCH_MISO) ? 1 : 0);
    digitalWrite(TOUCH_CLK, LOW);
  }
  return in;
}

static void touchInit() {
  pinMode(TOUCH_CLK, OUTPUT);
  pinMode(TOUCH_MOSI, OUTPUT);
  pinMode(TOUCH_CS, OUTPUT);
  pinMode(TOUCH_MISO, INPUT);
  pinMode(TOUCH_IRQ, INPUT);                    // GPIO36/39 are input-only, no pull-ups (the XPT2046 drives them)
  digitalWrite(TOUCH_CS, HIGH);
  digitalWrite(TOUCH_CLK, LOW);
  digitalWrite(TOUCH_CS, LOW);
  xpt8(0x90); xpt16(0);                         // power-down mode with PENIRQ enabled
  digitalWrite(TOUCH_CS, HIGH);
}

static inline int xptBest2(int x, int y, int z) {   // best two of three, as in the library
  int da = x > y ? x - y : y - x, db = x > z ? x - z : z - x, dc = z > y ? z - y : y - z;
  if (da <= db && da <= dc) return (x + y) >> 1;
  if (db <= da && db <= dc) return (x + z) >> 1;
  return (y + z) >> 1;
}

// plain conversion: send ONE command, then clock the 12-bit result out (fallback when the pipelined frames above misalign)
static uint16_t xptRead1(uint8_t cmd) {
  digitalWrite(TOUCH_CLK, LOW);
  digitalWrite(TOUCH_CS, LOW);
  delayMicroseconds(TOUCH_CLK_US);
  xpt8(cmd);
  uint16_t v = xpt16(0);
  digitalWrite(TOUCH_CS, HIGH);
  return (v >> 3) & 0x0FFF;
}
static inline int xptMed3(int a, int b, int c) {
  if (a > b) { int t = a; a = b; b = t; }
  if (b > c) { int t = b; b = c; c = t; }
  if (a > b) { b = a; }
  return b;
}

// returns true while the screen is pressed; rx/ry = the raw values XPT2046_Touchscreen (rotation 0) gives
static bool touchRaw(int& rx, int& ry) {
  int d[6] = {0, 0, 0, 0, 0, 0};
  digitalWrite(TOUCH_CLK, LOW);
  digitalWrite(TOUCH_CS, LOW);
  delayMicroseconds(TOUCH_CLK_US);
  xpt8(0xB1);                                   // Z1
  int z1 = (xpt16(0xC1) >> 3) & 0xFFF;          // Z2 next
  int z = z1 + 4095;
  int z2 = (xpt16(0x91) >> 3) & 0xFFF;
  z -= z2;
  if (z >= TOUCH_Z_MIN) {
    xpt16(0x91);                                // dummy: the first measurement is always noisy
    d[0] = (xpt16(0xD3) >> 3) & 0xFFF;
    d[1] = (xpt16(0x93) >> 3) & 0xFFF;
    d[2] = (xpt16(0xD3) >> 3) & 0xFFF;
    d[3] = (xpt16(0x93) >> 3) & 0xFFF;
  }
  d[4] = (xpt16(0xD0) >> 3) & 0xFFF;            // last: power down
  d[5] = (xpt16(0) >> 3) & 0xFFF;
  digitalWrite(TOUCH_CS, HIGH);
  gTouchZ = z; gTouchIrq = digitalRead(TOUCH_IRQ) ? 1 : 0;
  if (z >= TOUCH_Z_MIN) {
    int lx = xptBest2(d[0], d[2], d[4]);
    int ly = xptBest2(d[1], d[3], d[5]);
    if (lx >= 50 && ly >= 50 && lx <= 4090 && ly <= 4090) {   // not saturated = valid contact
      rx = 4095 - ly;                             // library rotation 0:  xraw = 4095 - y
      ry = lx;                                    //                      yraw = x
      return true;
    }
  }
#ifndef TOUCH_NO_FALLBACK
  // the pipelined read saw no touch, but PENIRQ is LOW (= finger down): read X / Y with plain single conversions
  if (!digitalRead(TOUCH_IRQ)) {
    delayMicroseconds(60);
    if (!digitalRead(TOUCH_IRQ)) {
      int sx[3], sy[3];
      for (int i = 0; i < 3; i++) { sx[i] = xptRead1(0xD1); sy[i] = xptRead1(0x91); }
      xptRead1(0x90);                             // back to PENIRQ mode
      int fx = xptMed3(sx[0], sx[1], sx[2]), fy = xptMed3(sy[0], sy[1], sy[2]);
      if (fx > 80 && fy > 80 && fx < 4015 && fy < 4015) {
        gTouchZ = TOUCH_Z_MIN;                    // shown in the status line: fallback path was used
        rx = 4095 - fx;
        ry = fy;
        return true;
      }
    }
  }
#endif
  return false;
}

#endif  // TOUCH_USE_LIB

// Calibration from your 5-point test (landscape, 320x240):
//   TL raw 483,547 | TR 3634,461 | BL 396,3548 | BR 3502,3575 | centre 1990,2089
// DEFAULT = the direct min/max map from your working test (Rotation 1):
//   x = map(rawX, 396, 3634, 0, 319);  y = map(rawY, 461, 3575, 0, 239);   (TOUCH_X_MIN/MAX, TOUCH_Y_MIN/MAX above)
// A plain min/max map is off by up to ~13 px at the corners (panel slightly skewed). Build with -DTOUCH_AFFINE
// for the 5-point affine fit instead (max error ~4 px). TOUCH_INV_X / TOUCH_INV_Y / TOUCH_SWAP_XY are applied by the caller.
#ifndef TOUCH_CAL_W
#define TOUCH_CAL_W 320
#define TOUCH_CAL_H 240
#endif
static inline void touchToScreen(int rx, int ry, int& x, int& y) {
#ifdef TOUCH_AFFINE
  float fx = 0.101994442f * rx + 0.00367350476f * ry - 51.9995218f;
  float fy = 0.000749845822f * rx + 0.0781501317f * ry - 41.6393106f;
  x = constrain((int)(fx + 0.5f), 0, TOUCH_CAL_W - 1);
  y = constrain((int)(fy + 0.5f), 0, TOUCH_CAL_H - 1);
#else
  x = (int)((long)(rx - TOUCH_X_MIN) * (TOUCH_CAL_W - 1) / (TOUCH_X_MAX - TOUCH_X_MIN));
  y = (int)((long)(ry - TOUCH_Y_MIN) * (TOUCH_CAL_H - 1) / (TOUCH_Y_MAX - TOUCH_Y_MIN));
  x = constrain(x, 0, TOUCH_CAL_W - 1);
  y = constrain(y, 0, TOUCH_CAL_H - 1);
#endif
}
