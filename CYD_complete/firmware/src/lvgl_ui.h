// UI selector. main.cpp includes this file right before setup(); exactly one UI header gets compiled in.
//   -DUI_DECK              : Stream Deck style keys / profiles / Reel  (lvgl_ui_deck.h)
//   -DUI_PHONE             : phone-style UI (lock screen, home, apps)  (lvgl_ui_phone.h)   <- fallback when no UI flag is set
//   -DUI_DAYSTIL           : "Days til" countdown app                  (lvgl_ui_daystil.h)
//   -DUI_DASH              : card dashboard CPU/RAM/NET                (lvgl_ui_dash.h)
//   -DUI_ORIG              : your previous UI                          (lvgl_ui_orig.h)
#pragma once
#if defined(UI_DECK)
#include "lvgl_ui_deck.h"
#elif defined(UI_DAYSTIL)
#include "lvgl_ui_daystil.h"
#elif defined(UI_DASH)
#include "lvgl_ui_dash.h"
#elif defined(UI_ORIG)
#include "lvgl_ui_orig.h"
#else
#include "lvgl_ui_phone.h"
#endif
