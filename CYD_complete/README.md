# CYD Deck — PC Stream-Deck remote + lock-screen media player for the ESP32-2432S028R

A touch **Stream-Deck style remote for your Windows PC**, a **lock-screen / standby clock with a photo & video player** (with sound), and a **web control panel** — all running on the cheap **ESP32-2432S028R "CYD"** (Cheap Yellow Display): a plain ESP32 with **no PSRAM**, 4 MB flash and a 2.8" 320×240 touch screen.

> The whole project is tuned for the very small RAM of this board. Read [RAM design](#ram-design-and-tuning) before changing buffer sizes.

---

## Contents

1. [Features](#features)
2. [Hardware and connections](#hardware-and-connections)
3. [Repository layout](#repository-layout)
4. [Quick start](#quick-start)
5. [Build environments](#build-environments)
6. [First-time setup](#first-time-setup)
7. [The Deck UI](#the-deck-ui)
8. [Web control panel](#web-control-panel)
9. [Photos, videos and sound](#photos-videos-and-sound)
10. [PC agent (Windows)](#pc-agent-windows)
11. [Built-in actions](#built-in-actions)
12. [Security](#security)
13. [RAM design and tuning](#ram-design-and-tuning)
14. [HTTP API](#http-api)
15. [Troubleshooting](#troubleshooting)
16. [Known limits](#known-limits)
17. [Credits and license](#credits-and-license)

---

## Features

### Remote (Stream-Deck style)
- **Profiles (tabs)** with **10 programmable keys** each + previous/next profile. Default **10 profiles** (`RMT_TABS`, can be raised to 12). Default set: MEDIA, SYSTEM, BROWSER, EDIT, SLIDES, POWER.
- Per key: label (12 chars), action, **8 colours**, **danger** flag (needs a second tap to confirm), **hold-to-repeat** (volume, arrows…), optional **32×32 icon** (32 icon slots, transparent colour supported) — otherwise a big monogram letter.
- **75 built-in actions** (media, window management, editing, browser, navigation, presentation, power) plus free-form actions: `keys:ctrl+shift+n`, `url:https://…`, `app:notepad` (see [Built-in actions](#built-in-actions)).
- Keys are sent from a background task: the UI never blocks. The key flashes green/red and a toast shows *SENT / ERROR / wrong token / PC unreachable*.
- **Auto profile:** the profile whose name matches the active Windows app/window is selected automatically.
- Swipe left/right = next/previous profile, tap the top pill = profile picker, pull down = Control Centre.
- Profiles, buttons and token are stored in one file on LittleFS (`/rmt.cfg`, atomic write); icons in `/ico.bin`.

### Lock screen / standby
- Big **clock with date**, **12 h or 24 h, with or without seconds** (web panel → Display). The time is taken from the **PC agent** (your PC's local time, refreshed with every `/stats` poll) — there is no NTP client, so the clock shows `--:--` / *waiting for time sync* until the agent is reachable.
- **Ambient mode:** optional *Standby = SD slideshow* — photos, GIFs and videos from the SD card start by themselves on the lock screen.
- **Control strip** while the lock-screen media plays: tap the screen → picture and sound pause and big buttons appear: `<<` previous · `>` continue · `>>` next · `-` / `+` volume · `X` unlock.
- Lock screen can also be started from the web panel (**Control → Lock**).

### Media player ("Reel")
- Plays from the **SD card**, straight to the panel, with no RAM frame buffer:
  `.mjv` (own MJPEG container, "MJV2"), MJPEG `.avi`, `.gif`, `.jpg`, raw RGB565 `.rgb`.
- Swipe = previous/next, tap = leave (Reel) / control strip (lock screen), playlist, loop, still-image seconds.
- **Sound** (experimental): a `<clip>.snd` file next to the clip is played on the CYD **speaker connector** (ESP32 DAC, GPIO26), 11 025 Hz 8-bit mono, with volume control.
- Frame pacing, late-frame dropping, SD streaming and DMA push are all done without PSRAM.

### Web control panel
- Runs on the device (`http://<device-ip>/`), **password protected** (HTTP Basic, user `admin`), with **password change**, **recovery key** and a **BOOT-button reset**.
- Tabs: **Control · Remote · Image · Video / SD · Display · System** — see [Web control panel](#web-control-panel).
- **Browser-side converter:** turns your mp4/webm/mov/gif/photos into the device formats (320×240), picks the highest JPEG quality that fits the device's frame buffer, can **measure the real decoder speed of your board (AUTO preset)** and can extract the **sound track**.
- OTA firmware update (OTA build), Wi-Fi scan/connect, first-run **access-point setup mode**.

### PC agent (Windows)
- One Python program, one port: **remote control** (`/cmd`) + **monitor data** (`/stats`: active window, CPU, RAM, …) + discovery (`/ping`).
- Runs **hidden in the background** and starts with Windows (no console window).
- Token protected; without a token the dangerous actions (shutdown, restart, sleep, `app:`, `url:`) are refused.

### Stability features (no-PSRAM board)
- Video/upload buffer allocated **last**, only from RAM above a reserve — it shrinks instead of starving Wi-Fi.
- Wi-Fi modem sleep off, TX power limited, 20 s keep-alive packet to the router (stops the "IP not reachable after a while" problem).
- Upload mode: PC polling is paused, the UI stays alive, the idle video buffer doubles as the SD write buffer.
- Reset-reason and heap/maxblock/UI-stack logging every 10 s; auto-restart if RAM stays critically low.

---

## Hardware and connections

| Part | Details |
|---|---|
| Board | **ESP32-2432S028R** ("CYD", 2.8", ESP32-WROOM-32, **no PSRAM**, 4 MB flash) |
| Display | 2.8" 240×320 **ST7789** (use env `cyd_ili9341` for the older ILI9341 board), used in **landscape 320×240** |
| Touch | XPT2046 resistive |
| Storage | microSD slot (FAT32) |
| Sound | Small speaker (≈ 8 Ω, 0.5–1 W) on the board's **SPEAKER** connector |
| PC | Windows 10/11 with Python 3 |

### Pin map (fixed on the board — no wiring needed)

| Function | GPIO |
|---|---|
| **Display** (HSPI): SCK / MOSI / DC / CS | 14 / 13 / 2 / 15 |
| Display RST | tied to EN |
| Backlight | **21** (some boards: **27** → env `cyd_bl27`) |
| **Touch** (VSPI, shared with SD): CLK / MOSI / MISO / CS / IRQ | 25 / 32 / 39 / 33 / 36 |
| **microSD** (VSPI): SCK / MISO / MOSI / CS | 18 / 19 / 23 / 5 |
| **Speaker** (DAC channel 2) | **26** |
| BOOT button | 0 |
| Optional rotary encoder A / B / SW | 22 / 27 / 35 (defined, not used by the Deck UI) |

Notes:
- The touch controller and the SD card **share the VSPI host**; the firmware time-shares it with a mutex and re-pins the bus (the touch library only talks SPI while a finger is down).
- **GPIO25 (DAC1) is never enabled** by the audio code because it is the touch clock; only DAC2/GPIO26 is used.
- Power: use a good USB cable/port. Weak USB power causes brown-out resets, Wi-Fi drops and screen noise.

### Connecting the speaker
Plug the speaker into the **SPEAKER** (P4) connector of the CYD. No other wiring. Volume: web panel → Display, or the `-` `+` keys on the lock-screen control strip.

---

## Repository layout

```
.
├── README.md
├── firmware/                 PlatformIO project (ESP32)
│   ├── platformio.ini        environments and build flags
│   ├── lvgl_inc.py           makes include/lv_conf.h visible to the LVGL build
│   ├── include/lv_conf.h     LVGL 8.3 configuration
│   └── src/
│       ├── main.cpp          Wi-Fi, web server, SD, storage, media player, audio, tasks
│       ├── display.h         LovyanGFX panel + scaled drawing API
│       ├── touch.h           XPT2046 + SD bus sharing, calibration
│       ├── webui.h           the whole web panel (one embedded page)
│       ├── lvgl_ui.h         selects one UI header
│       ├── lvgl_ui_deck.h    Stream-Deck UI (default)
│       └── lvgl_ui_phone.h / _dash.h / _daystil.h / _orig.h   other UIs (inherited, best effort)
└── pc_agent/                 Windows agent + hidden launcher
    ├── pc_agent.py
    ├── install_autostart.bat   set token, start hidden now and at every login
    ├── pc_agent_start.vbs      start hidden (harmless if already running)
    ├── stop_pc_agent.bat · set_token.bat · show_log.bat · remove_autostart.bat
    └── README.txt
```

---

## Quick start

### 1) Flash the ESP32
Requirements: [VS Code](https://code.visualstudio.com/) + the PlatformIO extension (or the `pio` CLI).

```bash
cd firmware
pio run -e cyd_deck_fast -t erase      # first time: wipe flash
pio run -e cyd_deck_fast -t upload
pio device monitor                     # 115200 baud, shows the boot log
```

`cyd_deck_fast` (the default environment) runs the panel SPI at **80 MHz** and the SD card at **40 MHz**. If the picture sparkles use `cyd_deck` (panel 40 MHz); if the SD card shows read errors use `cyd_deck` / `cyd_deck_safe` (SD 20 MHz).

### 2) Prepare the SD card
FAT32. The firmware creates `/videos`, `/gifs`, `/images`. You can also copy files by hand, but the web panel converter is easier (see [Photos, videos and sound](#photos-videos-and-sound)).

### 3) Connect Wi-Fi
On first boot (no saved Wi-Fi) the device starts an access point (`PC-Monitor-Setup`). Join it, open any page (captive portal) or `http://192.168.4.1/`, log in (`admin` / `admin`), open **System**, scan, choose your Wi-Fi, **Connect**. The device then shows its IP on the Wi-Fi screen and in the serial log.

### 4) Log in and secure the panel
`http://<device-ip>/` → user **admin**, password **admin** → **System → Panel password**: set a new password and **write down the recovery key**.

### 5) Link the PC
In the panel → **System → PC**: enter the **PC IP**, port **8080** and a **token** (any secret, 12+ characters) → Save. Then install the PC agent below with the **same token**.

### 6) Install the PC agent
Copy the `pc_agent` folder to the PC → double-click **`install_autostart.bat`** → type the same token when asked. The agent now runs hidden and starts at every login. Allow Python in the Windows Firewall for **private** networks the first time.

Press a key on the CYD — `show_log.bat` should show `OK  playpause …`.

---

## Build environments

Default: **`cyd_deck_fast`**.

| Environment | Purpose |
|---|---|
| `cyd_deck_fast` | **Deck UI, panel 80 MHz + SD 40 MHz** (default, best video) |
| `cyd_deck` | Deck UI, panel 40 MHz, SD 20 MHz (safer) |
| `cyd_deck_safe` | Deck UI at 160 MHz CPU — fewer brown-outs on weak USB power |
| `cyd_deck_lite` | **Smallest RAM use**: 6 profiles, 16 KB video buffer, no icons kept in RAM (monogram letters). Use it if the web page still stops answering |
| `cyd_ota` | Two 1.9 MB app slots: flash once by cable, then upload `firmware.bin` from the web panel |
| `cyd_ili9341` | Older single-USB boards with an ILI9341 panel |
| `cyd_bl27` | Boards whose backlight is on GPIO27 (screen stays black but alive) |
| `color_bgr` / `color_invert` | Red/blue swapped / negative colours |
| `cyd_flip` | Upside-down or mirrored picture |
| `cyd_20mhz` / `cyd_80mhz` | Panel SPI speed experiments |
| `cyd_touch_inv_x`, `cyd_touch_inv` | Touch X (and Y) reversed |
| `cyd_touch_bitbang` | Software-SPI touch (no library, no bus sharing) |
| `cyd_nobrownout` | Switches the brown-out reset off (testing only) |
| `cyd_phone`, `cyd_dash`, `cyd_daystil`, `cyd_lvgl_orig` | Other LVGL UIs inherited from the original project (best effort; the web panel has no dashboard tab) |
| `cyd`, `cyd_sprite`, `cyd_bitmap_fonts` | Classic non-LVGL UI and its variants |

Useful extra flags (add to `build_flags`): `-DTOUCH_INV_X -DTOUCH_INV_Y -DTOUCH_SWAP_XY`, `-DTOUCH_AFFINE`, `-DLV_CPU_160`, `-DWIFI_SLEEP`, `-DWIFI_TX_POWER=WIFI_POWER_11dBm`, `-DRMT_TABS=12`, `-DLIGHT_POOL_KB=..`, `-DLIGHT_RESERVE_KB=..`.

---

## First-time setup

| Step | Where |
|---|---|
| Wi-Fi | Setup AP → System tab, or later System → Wi-Fi |
| Panel password + recovery key | System → Panel password |
| PC IP / port / token | System → PC |
| Brightness, rotation, 12 h clock, seconds, volume | Display |
| Standby timeout, auto profile, **Standby = SD slideshow** | on the device: Settings (dock) |
| Profiles and keys | Remote tab of the web panel |

---

## The Deck UI

| Gesture / control | Result |
|---|---|
| Tap a key | Send its action to the PC (danger keys: tap twice) |
| Hold a "repeat" key | Repeats (volume, arrows…) |
| Swipe left / right on the keys | Next / previous profile |
| Tap the top pill | Profile picker |
| Pull down | Control Centre |
| Dock: Reel · Wi-Fi · Control · Standby · Settings | Open that screen |
| Reel: tap a file | Play it (draws after LVGL has finished refreshing, so the list never paints over the picture) |
| In the player: swipe | Previous / next file |
| In the player: tap | Leave (Reel) · control strip (lock screen) |
| **BOOT** short press | Next picture while playing · lock ↔ unlock otherwise (first press only wakes a sleeping screen) |
| **BOOT** hold 1 s | Leave the player |
| **BOOT** hold 10 s | Reset the web password to `admin` and restart |

---

## Web control panel

`http://<device-ip>/` (HTTP Basic login).

| Tab | What it does |
|---|---|
| **Control** | Open device screens (Deck, Reel, Wi-Fi, Settings…), Home / **Lock** / Screen off / Wake, stop video, previous/next media, brightness |
| **Remote** | Add/rename/delete profiles, edit keys (action, label, colour, danger, repeat), upload icons, test a key on the PC, PC token |
| **Image** | Convert and upload photos / GIFs |
| **Video / SD** | Convert and upload videos (presets, **AUTO**, **Include sound**), SD file list, play, play all, delete, remount |
| **Display** | Brightness, rotation, 12-hour clock, seconds, **video volume**, GIF mode |
| **System** | Wi-Fi scan/connect, PC IP/port/token, **panel password + recovery key**, firmware info, OTA update, reboot |

---

## Photos, videos and sound

The device cannot decode mp4/H.264 itself. The **browser converts** your files to formats the ESP32 can play straight from the SD card:

| Input (browser must be able to open it) | Output on SD |
|---|---|
| mp4 / webm / mov / m4v / mkv / ogv | `/videos/<name>.mjv` (MJPEG, 320×240 max) |
| gif / animated images | `/gifs/<name>.gif` or clip |
| jpg / png / webp … | `/images/<name>.jpg` |

> mp4 files with H.265/HEVC or exotic codecs may not open in your browser. Re-encode them first: `ffmpeg -i in.mp4 -c:v libx264 -pix_fmt yuv420p out.mp4`.

### Video presets (Video / SD tab)

| Preset | Size | fps |
|---|---|---|
| **AUTO** | 320×240 | measured on *your* board |
| **BEST** (default) | 320×240 | 12 |
| High | 320×240 | 15 |
| Smooth | 320×240 | 20 |
| Balanced | 288×216 | 20 |
| Fast | 240×180 | 25 |
| Wide 16:9 | 320×180 | 20 |
| Small | 160×120 | 30 |

- The converter finds the **highest JPEG quality that still fits the device's frame buffer** (shown as *Frame budget* in the panel) and downsamples large sources in several steps for a sharper picture.
- **AUTO** sends sample frames to the device (`/api/bench`), which decodes them 10× without drawing and reports the sustainable fps (decode + SD read time + margin).
- Actual speed depends on the decoder (≈ 15–25 fps at 320×240 on a no-PSRAM ESP32); lower the fps if playback stutters.

### Sound
- Tick **Include sound** (default **off**) before converting. The browser decodes the audio track (files up to **60 MB**), resamples it to **11 025 Hz mono 8-bit**, normalises it and uploads `<name>.snd` first, then the video.
- File format: `"SND1"` + `uint32` sample rate (little endian) + unsigned 8-bit samples.
- On the device the I2S built-in DAC (GPIO26) plays it while the clip plays (needs ~9 KB RAM, only during playback). Loops restart the sound. Deleting a video deletes its `.snd`.
- No `.snd` next to a clip → silent, exactly as before.
- The audio code is compiled out on Arduino-ESP32 core 3.x (no built-in DAC mode there).

---

## PC agent (Windows)

Files: `pc_agent/pc_agent.py` + launcher scripts.

```
GET /ping                          -> {"agent":"pc_agent", ...}        (device finds the port)
GET /stats                         -> {"host","name","cpu","ram","win","wapp","time","date",...}
GET /cmd?c=<action>&l=<label>&k=<token>
                                   -> 200 ok | 403 bad token | 400/404 unknown or failed action
```

- **Port** 8080 preferred; if busy the next free port (up to +12) is used and the CYD finds it by itself.
- **Token** stored in `agent_config.json` (`set_token.bat` / `python pc_agent.py --set-token`). It must be **identical** to the one in the web panel. Log line for a mismatch: `DENIED (bad token) from <ip>`.
- **Empty token:** normal key actions work, but `shutdown`, `restart`, `sleep`, `app:` and `url:` are refused.
- **Hidden start:** `install_autostart.bat` (installs `psutil`, asks for the token, creates a Startup shortcut, starts hidden). Log: `pc_agent.log` (cleared at 1 MB), open with `show_log.bat`.
- `stop_pc_agent.bat`, `remove_autostart.bat`, `set_token.bat` (restarts the agent).
- Needs: Python 3 (tick *Add Python to PATH*), `psutil` (installed automatically), optional `nvidia-smi` for GPU numbers.
- `/stats` uses `psutil`; the Deck UI only needs the active window name (auto-profile) and the **time** (the device clock is set from it).

---

## Built-in actions

`playpause next prev stop mute volup voldown` ·
`lock desktop alttab taskview taskmgr screenshot minimize maximize snap_left snap_right run explorer settings search clipboard emoji newdesk nextdesk prevdesk gamebar project notify voicetype monitor_off sleep restart shutdown cancel_shutdown closewin` ·
`copy cut paste undo redo selectall save find` ·
`newtab closetab reopentab nexttab prevtab refresh fullscreen back forward zoomin zoomout zoomreset addrbar incognito` ·
`up down left right esc enter space tab backspace delete pgup pgdn home end` ·
`slide_start slide_here blackscreen`

Free-form actions (type them in the key's action field):

| Action | Meaning |
|---|---|
| `keys:ctrl+shift+n` | Press a key combination (`ctrl shift alt win enter esc tab space f1…f24 a…z 0…9 plus minus comma period …`) |
| `url:https://example.com` | Open a link (http/https only; needs a token) |
| `app:notepad` | Start a program (needs a token) |

---

## Security

- The panel uses **HTTP Basic authentication over plain HTTP**: fine on a home Wi-Fi, **do not expose it to the internet or use it on public Wi-Fi**.
- Default login is `admin` / `admin` — a red banner reminds you until you change it. The password is stored in NVS as plain text (the device cannot hash and verify Basic credentials cheaply).
- Brute-force protection: 8 wrong passwords → 30 s lock; 5 wrong recovery keys → 5 min lock.
- **Forgot the password?** `http://<ip>/recover` + the 12-character recovery key (single use: a new key is shown after use), or hold **BOOT for 10 s**, or erase the flash.
- The PC agent only accepts commands with the shared token; dangerous actions are refused without one.
- Uploads are checked **before** a single byte is stored.

---

## RAM design and tuning

The ESP32 has ~320 KB of RAM and Wi-Fi alone uses ~55 KB. The project therefore:

- allocates the LVGL pool (`LV_MEM_KB=32`), one 10-line DMA draw buffer, SD/LittleFS and the task stacks first;
- allocates the **video/upload buffer last** (`LIGHT_POOL_KB=32` max) from what is left above `LIGHT_RESERVE_KB=48`; it appears in the boot log as `video pool: X KB …`;
- uses the same buffer as the **SD upload write buffer** and for the AUTO benchmark;
- allocates the GIF decoder, the audio driver and the icon scratch buffers only while they are used;
- **polls the PC every 3 s only while the Deck home screen needs the active window (auto profile)**; on the lock screen, in the Reel, in Settings or with the screen off it polls every 20 s (the clock keeps running by itself) — far fewer TCP connections and less heap churn;
- keeps only a few stats (active window + clock) from the PC and uses no history arrays in the Deck build;
- stores tabs in a heap array (`RMT_TABS=10`) and on LittleFS (not NVS, which is only 20 KB).

If the device gets sluggish or the web page stops answering, check the serial log (`heap=… maxblock=…`) and try, one at a time: `-DLIGHT_POOL_KB=20`, `-DRMT_TABS=8`, fewer icons, `cyd_deck_safe`.

---

## HTTP API

All routes except `/recover` and `/api/recover` need the login.

| Route | Purpose |
|---|---|
| `GET /`, `/stats` | Panel page, device stats |
| `GET /api/state`, `POST /api/settings`, `POST /api/reset` | State / settings (brightness, rotation, clock, volume `vol`, …) |
| `GET /api/remote/state`, `POST /api/remote/send`, `POST /api/icon` | Profiles, key test, icons |
| `POST /api/ui`, `/api/page`, `/api/next`, `/api/prev` | Control the device screens |
| `GET /api/scan`, `POST /api/wifi`, `GET /api/wifi/status`, `POST /api/ap/stop` | Wi-Fi |
| `POST /sd/upload?name=&size=&play=` | Upload to the SD card (multipart) |
| `GET /api/sd/list`, `POST /api/sd/play`, `/api/sd/playall`, `/api/sd/delete`, `/api/sd/mount` | SD manager |
| `POST /api/bench` | Decoder speed test (one JPEG frame) |
| `GET/POST /api/passwd` | Password, recovery key (`?key=1`), reset |
| `GET /recover`, `POST /api/recover` | Forgot-password page (no login, recovery key) |
| `POST /api/ota`, `POST /api/reboot` | Firmware update (OTA build), reboot |

---

## Troubleshooting

| Problem | What to do |
|---|---|
| Black screen but the board is alive | Wrong backlight pin → `cyd_bl27` |
| Red/blue swapped · negative colours | `color_bgr` · `color_invert` |
| Picture sparkles / noise | `cyd_deck` (40 MHz) or `cyd_20mhz` |
| SD read errors / video stutters hard | `cyd_deck` (SD 20 MHz), try another card (Class 10, ≤ 32 GB, FAT32) |
| Touch reversed | `-DTOUCH_INV_X` / `-DTOUCH_INV_Y` / `-DTOUCH_SWAP_XY` |
| Touch never reacts | `cyd_touch_bitbang`, check the serial `touch: z=… irq=…` line |
| Random resets / Wi-Fi drops | Better USB cable/port, `cyd_deck_safe`; check `RESET REASON` in the log (BROWN-OUT = power) |
| PC log: `DENIED (bad token)` | Token differs between panel and agent → `set_token.bat` and re-enter it in System → PC (case-sensitive) |
| Clock shows `--:--` / *waiting for time sync* | The clock comes from the PC agent: PC IP/port correct and agent running? |
| Keys do nothing | PC IP/port correct? Agent running (`show_log.bat`)? Firewall allows Python on private networks? |
| "no RAM" / web stops answering | See [RAM design](#ram-design-and-tuning); post the serial `heap=` lines in an issue |
| Upload fails | Browser can't decode the file (re-encode to H.264), SD card missing/full, or "Frame budget" too small → use a smaller preset |
| Video has no sound | Tick **Include sound**, speaker on the SPEAKER connector, volume > 0, file ≤ 60 MB with an audio track, re-upload the video |
| Forgot the web password | `/recover` + recovery key, or hold BOOT 10 s |
| Image appears under the file list | Update to this firmware (the player now starts after LVGL has finished refreshing) |

---

## Known limits

- **No PSRAM:** max picture 320×240; practical video ≈ 15–20 fps at full size; no H.264 on the device (the browser converts).
- Audio is **experimental**: 8-bit mono, small speaker, a soft click when playback starts/stops, picture and sound run on separate clocks.
- HTTP only (no HTTPS) and plain-text password storage — see [Security](#security).
- The UI variants other than Deck (`phone`, `dash`, `daystil`, `orig`) are inherited and not maintained; their monitor dashboards are not in the web panel.
- Windows only for the PC agent (the `/cmd` actions use Windows key codes; on other systems the agent runs in a log-only dry-run mode).

---

## Credits and license

Built on [LVGL](https://lvgl.io/), [LovyanGFX](https://github.com/lovyan03/LovyanGFX), [ArduinoJson](https://arduinojson.org/), [TJpg_Decoder](https://github.com/Bodmer/TJpg_Decoder), [AnimatedGIF](https://github.com/bitbank2/AnimatedGIF), [XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen), [psutil](https://github.com/giampaolo/psutil) and the Arduino-ESP32 core.

License: add your license here (for example MIT) — check the licenses of the libraries above if you redistribute binaries.
