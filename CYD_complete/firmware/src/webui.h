#pragma once
#include <Arduino.h>

// Single-page control panel (http://ESP32_IP/)
const char PANEL_HTML[] PROGMEM = R"UI(<!doctype html>
<html lang=en><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>PC Monitor Control</title>
<style>
:root{color-scheme:dark;--bg:#0A0E16;--card:#141A26;--line:#232C3D;--tx:#F1F5F9;--mut:#8B97AB;--ac:#22D3EE;--acbg:#0F2C38;--ok:#34D399;--warn:#FB923C;--bad:#F87171;--in:#1D2535;--vio:#A78BFA;--pnk:#F472B6}
*{box-sizing:border-box}
[hidden]{display:none!important}
body{margin:0;background:var(--bg);color:var(--tx);font:16px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
.skip{position:absolute;left:-999px;top:8px;background:var(--ac);color:#fff;padding:8px 14px;border-radius:8px;z-index:50}
.skip:focus{left:8px}
.vh{position:absolute;width:1px;height:1px;overflow:hidden;clip:rect(0 0 0 0);white-space:nowrap}
:focus-visible{outline:3px solid var(--ac);outline-offset:2px;border-radius:6px}
header{position:sticky;top:0;z-index:10;background:var(--bg);border-bottom:1px solid var(--line)}
.bar{max-width:1100px;margin:auto;padding:10px 16px;display:flex;align-items:center;gap:10px;flex-wrap:wrap}
h1{font-size:18px;margin:0;flex:1;min-width:200px}
h1 span{color:var(--mut);font-weight:400}
.chips{display:flex;gap:6px;flex-wrap:wrap}
.chip{padding:3px 10px;border-radius:99px;background:var(--card);border:1px solid var(--line);font-size:13px;color:var(--mut)}
.chip.ok{color:var(--ok);border-color:var(--ok)}.chip.bad{color:var(--bad);border-color:var(--bad)}
.tabs{max-width:1100px;margin:auto;padding:0 10px;display:flex;gap:2px;overflow-x:auto}
.tabs button{background:none;border:0;color:var(--mut);padding:12px 16px;min-height:44px;cursor:pointer;font:inherit;border-bottom:3px solid transparent;white-space:nowrap}
.tabs button[aria-selected=true]{color:var(--tx);border-color:var(--ac);font-weight:600}
main{max-width:1100px;margin:auto;padding:16px}
#apBanner,#pwBanner{max-width:1100px;margin:12px auto 0;padding:12px 16px;border-radius:10px;background:var(--acbg);border:1px solid var(--ac);width:calc(100% - 32px)}
.grid{display:grid;gap:14px;grid-template-columns:repeat(auto-fit,minmax(300px,1fr))}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px}
.card.wide{grid-column:1/-1}
.card h2{margin:0 0 12px;font-size:13px;letter-spacing:.06em;text-transform:uppercase;color:var(--mut);font-weight:700}
.mut{color:var(--mut)}
.row{display:flex;align-items:center;gap:10px;flex-wrap:wrap}
.sp{flex:1}
.big{font-size:52px;font-weight:700;line-height:1}
.btn{background:var(--in);color:var(--tx);border:1px solid var(--line);border-radius:10px;padding:8px 16px;min-height:44px;cursor:pointer;font:inherit}
.btn:hover{border-color:var(--ac)}
.btn.pri{background:var(--ac);border-color:var(--ac);color:#fff}
.btn.danger{color:var(--bad);border-color:var(--bad);background:transparent}
.btn:disabled{opacity:.5;cursor:not-allowed}
input[type=number],input[type=text],input[type=password],select{background:var(--in);color:var(--tx);border:1px solid var(--line);border-radius:10px;padding:8px 12px;min-height:44px;font:inherit;width:100%}
input[type=number]{width:96px}
input[type=range]{width:100%;min-height:32px;accent-color:var(--ac)}
label.f{display:block;margin:12px 0 4px;color:var(--mut);font-size:14px}
.hint{color:var(--mut);font-size:14px;margin:8px 0 0}
.pgrid{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:8px}
.pbtn{background:var(--in);border:2px solid var(--line);color:var(--tx);border-radius:12px;padding:12px 8px;min-height:64px;cursor:pointer;font:inherit;text-align:center}
.pbtn small{display:block;color:var(--mut)}
.pbtn[aria-pressed=true]{border-color:var(--ac);background:var(--acbg)}
.pbtn.off{opacity:.6}
.sw{position:relative;display:inline-flex;align-items:center;gap:10px;cursor:pointer;min-height:44px}
.sw input{position:absolute;opacity:0;width:44px;height:28px;margin:0;cursor:pointer}
.sw .trk{position:relative;width:46px;height:26px;border-radius:99px;background:var(--line);transition:background .15s;flex:none}
.sw .trk:before{content:"";position:absolute;width:20px;height:20px;left:3px;top:3px;border-radius:50%;background:#fff;transition:transform .15s}
.sw input:checked+.trk{background:var(--ac)}
.sw input:checked+.trk:before{transform:translateX(20px)}
.sw input:focus-visible+.trk{outline:3px solid var(--ac);outline-offset:2px}
.oo{font-size:12px;padding:2px 8px;border-radius:6px;min-width:40px;text-align:center;border:1px solid var(--line)}
.oo.on{color:var(--ok);border-color:var(--ok)}.oo.off{color:var(--mut)}
.prow{display:flex;align-items:center;gap:14px;padding:8px 0;border-bottom:1px solid var(--line);flex-wrap:wrap}
.prow .pn{flex:1;font-weight:600;min-width:110px}
.meter{height:10px;background:var(--line);border-radius:6px;overflow:hidden;margin:6px 0 12px}
.meter i{display:block;height:100%;width:0;background:var(--ok);transition:width .4s}
table{width:100%;border-collapse:collapse}
td,th{padding:8px 6px;border-bottom:1px solid var(--line);text-align:left}
th{color:var(--mut);font-weight:500;font-size:13px}
canvas{width:100%;height:80px;display:block;margin-top:10px}
#dz,#dzI{border:2px dashed var(--line);border-radius:14px;padding:30px 16px;text-align:center;color:var(--mut);cursor:pointer;background:transparent;width:100%;font:inherit;min-height:120px}
#dz:hover,#dz.hot,#dzI:hover,#dzI.hot{border-color:var(--ac);background:var(--acbg);color:var(--tx)}
#upl,#uplI{list-style:none;margin:10px 0 0;padding:0}
.urow{display:flex;align-items:center;gap:10px;padding:6px 0;font-size:14px;flex-wrap:wrap}
.urow .un{flex:1;min-width:120px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.pb{width:120px;height:8px;background:var(--line);border-radius:4px;overflow:hidden}
.pb i{display:block;height:100%;width:0;background:var(--ac)}
.ust{min-width:110px;font-size:13px}
#nets{list-style:none;margin:8px 0 0;padding:0;max-height:240px;overflow:auto}
#nets button{width:100%;display:flex;justify-content:space-between;gap:8px;background:none;border:0;border-bottom:1px solid var(--line);color:var(--tx);padding:10px 6px;min-height:44px;cursor:pointer;font:inherit;text-align:left}
#nets button:hover{background:var(--acbg)}
#toast{position:fixed;left:50%;bottom:24px;transform:translate(-50%,20px);background:#1d6b2c;color:#fff;padding:12px 20px;border-radius:12px;opacity:0;transition:.25s;pointer-events:none;z-index:40;max-width:90vw}
#toast.show{opacity:1;transform:translate(-50%,0)}
#toast.bad{background:#a32b27}
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
.rm-wrap{display:grid;gap:16px;grid-template-columns:minmax(300px,520px) minmax(280px,1fr);align-items:start}
@media (max-width:820px){.rm-wrap{grid-template-columns:1fr}}
.rm-tabs{display:flex;gap:6px;flex-wrap:wrap;margin-bottom:10px}
.rm-tab{background:var(--in);color:var(--tx);border:2px solid var(--line);border-radius:99px;padding:6px 16px;min-height:40px;font:inherit;font-weight:600;cursor:pointer}
.rm-tab[aria-pressed=true]{border-color:var(--ac);background:var(--acbg)}
.rm-tab.add{border-style:dashed;color:var(--mut)}
.rm-screen{background:#000;border:3px solid var(--line);border-radius:16px;padding:8px;display:grid;gap:6px;align-content:start}
.rm-hd{display:flex;justify-content:space-between;align-items:center;background:#000a5c;color:#fff;border-radius:6px;padding:2px 10px;font-weight:700;font-size:13px}
.rm-hd i{width:10px;height:10px;border-radius:50%;background:#2ecc71;display:inline-block}
.rm-strip{display:grid;grid-template-columns:56px 1fr 56px;gap:6px;align-items:center}
.rm-strip button{background:#2a2f45;color:#ffd84a;border:2px solid #ffd84a;border-radius:6px;font:700 18px/1 system-ui;min-height:34px;cursor:pointer}
.rm-strip b{text-align:center;color:#3ee6ff;font-size:18px}
.rm-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:6px}
.rm-cell{min-height:56px;border-radius:6px;border:2px solid #3ee6ff;background:#000a5c;color:#fff;font:700 13px/1.15 system-ui;cursor:pointer;padding:4px;word-break:break-word;display:flex;align-items:center;justify-content:center;text-align:center}
.rm-cell.has{flex-direction:column;gap:2px}
.rm-cell small{display:block;font:400 9px/1.1 system-ui;opacity:.7;word-break:break-all}
.rm-cell.danger{border-color:#ff4d4d;color:#ffa44d}
.rm-cell.nav{background:#2a2f45;border-color:#ffd84a;color:#ffd84a;font-size:11px;flex-direction:column;gap:2px}
.rm-cell.nav b{color:#fff;font-size:12px}
.rm-cell.empty{background:#000;border:2px dashed #29323d;color:#5c6b7a;font-size:22px}
.rm-cell[aria-pressed=true]{outline:3px solid #fff;outline-offset:1px;box-shadow:0 0 0 5px var(--ac)}
.rm-cell:disabled{opacity:.5;cursor:default}
.rm-toast{background:#10101a;border:2px solid #2ecc71;border-radius:6px;color:#bbb;font-size:12px;padding:3px 8px;min-height:24px}
.rm-ins h3{margin:0 0 10px;font-size:18px}
.rm-bar{position:sticky;bottom:0;z-index:5;display:flex;gap:10px;align-items:center;flex-wrap:wrap;background:var(--card);border:1px solid var(--line);border-radius:14px;padding:10px 14px;margin-top:14px}
.rm-dirty{color:var(--warn);font-weight:600}
.rm-help{display:grid;grid-template-columns:auto 1fr;gap:4px 12px;font-size:14px;color:var(--mut);margin:12px 0 0}
code{background:var(--in);border:1px solid var(--line);border-radius:5px;padding:0 5px}
/* ---- phone look (same palette / shapes as the device UI) ---- */
body{background:radial-gradient(900px 420px at 50% -80px,#1B1464 0%,rgba(27,20,100,0) 72%),var(--bg);background-attachment:fixed}
header{background:rgba(10,14,22,.88);-webkit-backdrop-filter:blur(12px);backdrop-filter:blur(12px)}
h1{font-size:17px;letter-spacing:.01em}
.card{border-radius:18px;box-shadow:0 6px 18px rgba(0,0,0,.28)}
.btn{border-radius:99px}
.btn.pri{color:#06202B;font-weight:700}
.btn:active{transform:translateY(2px)}
.tabs{gap:6px;padding:0 12px 10px}
.tabs button{border-radius:99px;border:0;padding:8px 16px;min-height:40px;background:rgba(255,255,255,.06)}
.tabs button[aria-selected=true]{background:var(--ac);color:#06202B;font-weight:700}
.chip{background:rgba(255,255,255,.06);border-color:transparent}
.glass{background:rgba(255,255,255,.12);border:1px solid rgba(255,255,255,.24);border-radius:20px;padding:14px 18px;display:flex;align-items:center;gap:18px;flex-wrap:wrap}
.glass .gh b{display:block;font-size:17px}
.glass .gh span{font-size:13px}
.glass .gv{text-align:center;min-width:56px;padding-left:16px;border-left:1px solid rgba(255,255,255,.28)}
.glass .gv b{display:block;font-size:20px}
.glass .gv small{color:#C7D2FE;font-size:11px;letter-spacing:.06em}
.apps{display:grid;grid-template-columns:repeat(5,1fr);gap:16px 6px}
@media (max-width:640px){.apps{grid-template-columns:repeat(4,1fr)}}
.app{background:none;border:0;color:var(--tx);cursor:pointer;font:inherit;display:flex;flex-direction:column;align-items:center;gap:6px;padding:2px 0;min-height:44px}
.app .ic{width:58px;height:58px;border-radius:18px;display:grid;place-items:center;font-size:26px;color:#fff;box-shadow:0 5px 12px rgba(0,0,0,.4);transition:transform .1s}
.app:active .ic{transform:translateY(2px) scale(.96)}
.app small{font-size:12px;color:var(--mut)}
.app[aria-pressed=true] small{color:var(--tx);font-weight:700}
.app[aria-pressed=true] .ic{outline:3px solid rgba(255,255,255,.55);outline-offset:2px}
.pill{width:120px;height:5px;border-radius:3px;background:rgba(255,255,255,.7);margin:18px auto 2px}
.sdrow{display:flex;align-items:center;gap:10px;padding:10px 12px;margin:6px 0;border:1px solid var(--line);border-radius:12px;background:var(--card);flex-wrap:wrap}
.sdrow .gi{font-size:20px}
.sdrow .un{flex:1;min-width:140px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-weight:600}
.player{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-top:12px}
</style></head><body>
<a class=skip href="#main">Skip to content</a>
<header role=banner>
<div class=bar><h1>CYD Control <span id=scrTxt>Control Panel</span></h1>
<div class=chips role=status aria-label="Device status"><span class=chip id=cPc>PC --</span><span class=chip id=cWifi>WiFi --</span><span class=chip id=cIp>--</span><span class=chip id=cUp>--</span></div></div>
<div class=tabs role=tablist aria-label="Sections" id=nav>
<button role=tab id=tab-dash aria-controls=t-dash aria-selected=true>Control</button>
<button role=tab id=tab-remote aria-controls=t-remote aria-selected=false tabindex=-1>Remote</button>
<button role=tab id=tab-image aria-controls=t-image aria-selected=false tabindex=-1>Image</button>
<button role=tab id=tab-video aria-controls=t-video aria-selected=false tabindex=-1>Video / SD</button>
<button role=tab id=tab-disp aria-controls=t-disp aria-selected=false tabindex=-1>Display</button>
<button role=tab id=tab-sys aria-controls=t-sys aria-selected=false tabindex=-1>System</button>
</div></header>
<div id=pwBanner role=alert hidden><b>Default password!</b> Anyone on your WiFi can open this panel. Open <b>System</b> &rarr; <b>Panel password</b> and change it.</div>
<div id=apBanner role=alert hidden><b>Setup mode.</b> Open the <b>System</b> tab, scan, pick your WiFi and press Connect.</div>
<main id=main>

<section role=tabpanel id=t-dash aria-labelledby=tab-dash tabindex=0>
<div class=grid>
<div class="card wide"><h2>Device screen <span class=mut id=scrNow></span></h2>
<div class=apps id=apps role=group aria-label="Open an app on the device"></div>
<div class=player><button class=btn id=uHome>&#8962; Home</button><button class=btn id=uLock>&#128274; Lock</button><button class=btn id=uSleep>&#9211; Screen off</button><button class=btn id=uWake>&#9728; Wake</button><button class=btn id=uStop>&#9209; Stop video</button><button class=btn id=uPrev aria-label="Previous media">&#9198;</button><button class=btn id=uNext aria-label="Next media">&#9197;</button></div>
<label class=f for=uBr>Brightness <b id=uBrv></b></label><input type=range id=uBr min=5 max=255></div>
</div></section>

<section role=tabpanel id=t-disp aria-labelledby=tab-disp tabindex=0 hidden>
<div class=grid>
<div class=card><h2>Screen</h2>
<label class=f for=bright>Brightness <b id=brv></b></label><input type=range id=bright min=5 max=255>
<label class=f for=avol>Video volume <b id=avv></b></label><input type=range id=avol min=0 max=100>
<label class=f for=rot>Orientation</label><select id=rot><option value=1>Landscape</option><option value=3>Landscape (flipped 180&deg;)</option></select>
<label class=sw><input type=checkbox role=switch id=clk12><span class=trk aria-hidden=true></span><span class=lbl>12-hour clock (AM/PM)</span><b class="oo off" aria-hidden=true>OFF</b></label>
<label class=sw><input type=checkbox role=switch id=clkSec><span class=trk aria-hidden=true></span><span class=lbl>Show seconds on the clock</span><b class="oo off" aria-hidden=true>OFF</b></label>
<label class=f for=gifSkip>Native GIF playback</label>
<select id=gifSkip><option value=1>Smooth: frame skip kore speed thik rakhe</option><option value=0>All frames: slow hole dhire cholbe</option></select>
<div class=row style="margin-top:18px"><span class=sp></span><button class="btn danger" id=bReset>Reset display settings</button></div>
</div></div></section>

<section role=tabpanel id=t-image aria-labelledby=tab-image tabindex=0 hidden>
<div class=grid>
<div class="card wide"><h2>Now showing</h2>
<div class=row><div id=nowTxt class=sp>Nothing loaded.</div><button class=btn id=bShow>Show on display</button><button class="btn danger" id=bClear>Clear</button></div>
<p class=hint>Media flash-e save hoy na. Upload korle ESP32-r RAM-e giye sathe sathe display-te dekhay. Reboot korle muche jay.</p></div>
<div class=card><h2>Image options</h2>
<label class=f for=fit>Image fit</label>
<select id=fit><option value=contain>Fit inside (no crop)</option><option value=cover selected>Fill screen 320x240 (crop)</option><option value=stretch>Stretch to 320x240</option></select>
<label class=f for=destI>Upload destination</label>
<select id=destI><option value=ram>Show now (RAM, lost on reboot)</option><option value=sdplay>Save to SD card + show</option><option value=sdonly>Save to SD card only</option></select>
<label class=sw><input type=checkbox role=switch id=holdSw><span class=trk aria-hidden=true></span><span class=lbl>Hold on uploaded media</span><b class="oo off" aria-hidden=true>OFF</b></label></div>
<div class=card><h2>Upload image</h2>
<button type=button id=dzI>Drop an image here, click to choose, or paste (Ctrl+V)</button>
<input type=file id=fiI accept="image/*" multiple hidden>
<ul id=uplI aria-label="Image upload progress"></ul>
<p class=hint>JPG, PNG, WebP, BMP... browser-ei 320x240 e auto convert hoy (CYD). Animated GIF/WebP dile Video tab use koro.</p></div>
</div></section>

<section role=tabpanel id=t-video aria-labelledby=tab-video tabindex=0 hidden>
<div class=grid>
<div class="card wide"><h2>Now showing</h2>
<div class=row><div id=nowTxtV class=sp>Nothing loaded.</div><button class=btn id=bShowV>Show on display</button><button class="btn danger" id=bClearV>Clear</button></div></div>
<div class=card><h2>Video options</h2>
<label class=f for=vq>Video / clip quality</label>
<select id=vq><option value=auto>AUTO 320x240 - tested on this device: top quality + smoothest fps</option><option value=best selected>BEST picture - 320x240, 12 fps</option><option value=full>High - 320x240, 15 fps</option><option value=smooth>Smooth - 320x240, 20 fps</option><option value=balanced>Balanced - 288x216, 20 fps</option><option value=fast>Fast - 240x180, 25 fps (very smooth)</option><option value=wide>Wide 16:9 - 320x180, 20 fps</option><option value=full18>Small - 160x120, 30 fps</option></select>
<p class=hint id=vqHint></p>
<label class=f for=animMode>GIF / animation</label>
<select id=animMode><option value=clip>Convert to smooth clip (recommended)</option><option value=native>Send original GIF</option></select>
<label class=f for=dest>Upload destination</label>
<select id=dest><option value=ram>Play now (RAM, lost on reboot)</option><option value=sdplay>Save to SD card + play</option><option value=sdonly>Save to SD card only</option></select>
<label class=f for=vmax>Max clip length (seconds)</label><input type=number id=vmax value=15 min=1 max=3600>
<label class=sw style="margin-top:10px"><input type=checkbox role=switch id=vSnd><span class=trk aria-hidden=true></span><span class=lbl>Include sound (CYD speaker, files up to 60 MB)</span><b class="oo off" aria-hidden=true>OFF</b></label></div>
<div class=card><h2>Upload video / GIF</h2>
<button type=button id=dz>Drop a video or GIF here, click to choose, or paste (Ctrl+V)</button>
<input type=file id=fi accept="video/*,.avi,.mkv,.mp4,.mov,.webm,image/gif,image/webp,image/apng" multiple hidden>
<ul id=upl aria-label="Video upload progress"></ul>
<p class=hint>Video/GIF browser-eii smooth clip-e convert hoy. Ekbar-e ekta media dekhay, notun upload ager-ta replace kore.</p></div>
<div class="card wide" id=sdCard><h2>SD card</h2>
<div class=row><span class=chip id=sdChip>SD --</span><span id=sdInfo class=sp></span><button class=btn id=bSdRef>Refresh / mount</button></div>
<div class=meter aria-hidden=true><i id=sdBar></i></div>
<ul id=sdList aria-label="Videos on SD card" style="list-style:none;margin:10px 0 0;padding:0"></ul>
<div class=row style="margin-top:12px"><button class=btn id=bPlayAll>Play all (playlist)</button><button class=btn id=bStopAll>Stop playlist</button></div>
<label class=f for=sdLoops>Loops per video (playlist)</label><input type=number id=sdLoops min=1 max=50 style="width:110px">
<label class=f for=sdStill>Seconds per still image (playlist)</label><input type=number id=sdStill min=2 max=3600 style="width:110px">
<label class=sw><input type=checkbox role=switch id=sdPre><span class=trk aria-hidden=true></span><span class=lbl>Preload small clips into RAM</span><b class="oo off" aria-hidden=true>OFF</b></label>
<p class=hint>SD card-e jawa video ESP32 theke read-ahead buffer diye cholbe. Folders on the card: /videos (.mjv .avi)  /gifs (.gif)  /images (.rgb .jpg). On the device: Gallery app = tap a file to play, swipe left/right = next/previous, tap = back.</p></div>
</div></section>

<section role=tabpanel id=t-remote aria-labelledby=tab-remote tabindex=0 hidden>
<div class=grid>
<div class="card wide"><h2>PC remote &mdash; CYD buttons</h2>
<p class=hint style="margin:0 0 12px">Below is a preview of the CYD screen. Click a <b>tab</b> chip to switch tab, a <b>button</b> to edit it, <b>+</b> to add a button. Every tab has 10 buttons plus the automatic PREV / NEXT tab buttons. Press <b>Save changes</b> when done. <b>On the device:</b> long-press a button = edit / move / delete it, tap the <b>+</b> slot = add a button, long-press the tab name = tab menu (new / rename / delete / reset). Tap the header centre = lock the buttons, hold it = unlock. Tap the name box in the editor = button colour. Icons / logos are added in this panel (any image, auto-resized to 32x32). Custom keys, URLs, macros and free labels are typed here. Buttons marked <b>Hold to repeat</b> (volume, arrows) keep sending while you hold them; edit those in this panel.</p>
<div class=rm-wrap>
<div>
<div class=rm-tabs id=rmTabs role=group aria-label="Remote tabs"></div>
<div class=rm-screen aria-label="CYD screen preview">
<div class=rm-hd><span>PC REMOTE</span><i></i></div>
<div class=rm-strip><button type=button id=rmPrevT aria-label="Previous tab">&lt;</button><b id=rmTabName></b><button type=button id=rmNextT aria-label="Next tab">&gt;</button></div>
<div class=rm-grid id=rmGrid role=group aria-label="Button grid"></div>
<div class=rm-toast id=rmToast aria-hidden=true>toast bar: SENT / ERROR shows here on the CYD</div>
</div></div>
<div class=rm-ins aria-live=polite>
<div id=rmTabEd class=card style="background:var(--in);margin-bottom:12px"><h3>Tab</h3>
<div class=row><div class=sp><label class=f for=rmName style="margin-top:0">Tab name</label><input id=rmName type=text maxlength=10 autocomplete=off></div></div>
<div class=row style="margin-top:10px"><button class=btn type=button id=rmTabL>&larr; Move left</button><button class=btn type=button id=rmTabR>Move right &rarr;</button><button class="btn danger" type=button id=rmTabDel>Delete tab</button></div></div>
<div id=rmBtnEd class=card style="background:var(--in)"><h3 id=rmBtnTitle>Button</h3>
<div id=rmBtnForm>
<label class=f for=rmAct style="margin-top:0">Action</label><select id=rmAct></select>
<div id=rmCustomW hidden><label class=f for=rmCustom id=rmCustomL>Value</label><input id=rmCustom type=text maxlength=56 autocomplete=off spellcheck=false></div>
<label class=f for=rmLabel>Label on the button (max 12)</label><input id=rmLabel type=text maxlength=12 autocomplete=off>
<label class=f for=rmCol>Colour</label><select id=rmCol><option value=0>Cyan (default)</option><option value=1>Green</option><option value=2>Orange</option><option value=3>Magenta</option><option value=4>Yellow</option><option value=5>Pink</option><option value=6>White</option><option value=7>Blue</option></select>
<label class=f for=rmIcoF>Icon / logo (PNG, JPG, SVG... shown at 32x32)</label>
<div class=row><input type=file id=rmIcoF accept="image/*" style="flex:1;min-width:0"><button class=btn type=button id=rmIcoDel>Remove icon</button></div><p class=hint id=rmIcoSt style="margin:4px 0 0"></p>
<div class=row style="margin-top:12px"><label class=sw><input type=checkbox role=switch id=rmConfirm><span class=trk aria-hidden=true></span><span>Confirm (tap twice)</span></label></div>
<div class=row style="margin-top:8px"><label class=sw><input type=checkbox role=switch id=rmRep><span class=trk aria-hidden=true></span><span>Hold to repeat (volume, arrows...)</span></label></div>
<div class=row style="margin-top:12px"><button class="btn pri" type=button id=rmTest>Test on PC</button><button class=btn type=button id=rmL aria-label="Move button earlier">&larr;</button><button class=btn type=button id=rmR aria-label="Move button later">&rarr;</button><button class="btn danger" type=button id=rmDel>Remove</button></div>
</div>
<p class=hint id=rmNone>Click a button or <b>+</b>.</p>
</div>
<div class=rm-help><b>keys:</b><span><code>keys:ctrl+shift+n</code> any shortcut</span><b>url:</b><span><code>url:https://youtube.com</code> open a link</span><b>app:</b><span><code>app:notepad</code> <code>app:chrome</code> <code>app:C:\path\x.exe</code> any app</span></div>
</div></div>
<div class=rm-bar><button class="btn pri" type=button id=rmSave>Save changes</button><button class=btn type=button id=rmRevert>Undo changes</button><button class=btn type=button id=rmReset>Default layout</button><button class=btn type=button id=rmBackup>Backup</button><button class=btn type=button id=rmRestore>Restore</button><input type=file id=rmRestoreF accept=".json,application/json" hidden><span id=rmState class=mut role=status></span></div>
</div>
</div></section>

<section role=tabpanel id=t-sys aria-labelledby=tab-sys tabindex=0 hidden>
<div class=grid>
<div class=card><h2>PC connection</h2>
<label class=f for=pcIp>PC IP (pc_agent.py cholche ja)</label><input type=text id=pcIp inputmode=decimal autocomplete=off>
<label class=f for=pcPort>Port</label><input type=number id=pcPort min=1 max=65535 style="width:130px">
<label class=f for=rmtTok>Remote token <span class=mut id=rmtTokSt></span></label><input type=text id=rmtTok autocomplete=off spellcheck=false placeholder="e.g. 65b6fc3b (empty = keep)">
<div class=row style="margin-top:16px"><span class=sp></span><button class="btn pri" id=bSavePc>Save</button></div></div>
<div class=card><h2>WiFi</h2>
<div class=row><button class=btn id=bScan>Scan networks</button><span id=scanSt class=mut role=status></span></div>
<ul id=nets aria-label="Available WiFi networks"></ul>
<label class=f for=ssid>Network name (SSID)</label><input type=text id=ssid autocomplete=off>
<label class=f for=pass>Password</label><input type=password id=pass autocomplete=off>
<label class=sw style="margin-top:6px"><input type=checkbox id=showPw><span class=lbl>Show password</span></label>
<div class=row style="margin-top:14px"><button class="btn" id=bApStop hidden>Finish setup (close hotspot)</button><span class=sp></span><button class="btn pri" id=bConn>Connect</button></div>
<p class=hint id=wifiMsg role=status>Connect na hole device nijei "PC-Monitor-Setup" hotspot chalu kore. Boot button 4 sec dhorleo setup mode ashe.</p></div>
<div class=card><h2>Device</h2><table><tbody id=sysInfo></tbody></table>
<div class=row style="margin-top:14px"><span class=sp></span><button class="btn danger" id=bReboot>Reboot</button></div></div>
<div class=card><h2>Firmware update (OTA)</h2>
<p class=hint style="margin:0 0 10px">Choose <code>firmware.bin</code> (<code>.pio/build/cyd_ota/firmware.bin</code>) and upload. Needs the <code>cyd_ota</code> build flashed once by cable. Settings and remote buttons are kept.</p>
<input type=file id=otaFile accept=".bin" aria-label="Firmware file">
<div class=row style="margin-top:12px"><progress id=otaPg max=100 value=0 style="flex:1;min-width:0"></progress><button class="btn pri" type=button id=bOta>Upload &amp; restart</button></div>
<p class=hint id=otaSt role=status></p></div>
<div class=card><h2>Panel password</h2>
<p class=hint style="margin:0 0 10px">Login name: <code>admin</code>. Default password: <code>admin</code> (change it). Forgot it? Open <code>http://&lt;device-ip&gt;/recover</code> and enter the recovery key below. Last resort: hold the <b>BOOT</b> button for 10 seconds, the password goes back to <code>admin</code> and the device restarts.</p>
<label class=f>Recovery key (write it down, keep it safe)</label>
<div class=row><code id=rkv>&bull;&bull;&bull;&bull;-&bull;&bull;&bull;&bull;-&bull;&bull;&bull;&bull;</code><span class=sp></span><button class=btn type=button id=bRkShow>Show</button><button class=btn type=button id=bRkNew>New key</button></div>
<label class=f for=pw1>New password (4-32 characters)</label><input type=password id=pw1 autocomplete=new-password>
<label class=f for=pw2>Repeat new password</label><input type=password id=pw2 autocomplete=new-password>
<label class=sw style="margin-top:6px"><input type=checkbox id=showPw2><span class=lbl>Show password</span></label>
<div class=row style="margin-top:12px"><button class=btn type=button id=bPwDef>Reset to admin</button><span class=sp></span><button class="btn pri" type=button id=bPw>Change password</button></div>
<p class=hint id=pwMsg role=status></p></div>
<div class=card><h2>Screen sleep</h2>
<label class=f for=idleMin>Turn the screen off after (minutes, 0 = never)</label><input type=number id=idleMin min=0 max=240 style="width:130px">
<p class=hint>Touch the screen (or press a button) to wake it. The media page and alerts keep it on. The touch that wakes it does not press a button.</p>
<div class=row style="margin-top:12px"><span class=sp></span><button class="btn pri" type=button id=bIdle>Save</button></div></div>
</div></section>
</main>
<div id=toast role=status aria-live=polite></div>

<script>
const $=s=>document.querySelector(s),$$=s=>[...document.querySelectorAll(s)];
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
let S=null,formInit=false,busy=false;
function toast(m,bad){const t=$('#toast');t.textContent=m;t.className='show'+(bad?' bad':'');clearTimeout(toast.h);toast.h=setTimeout(()=>{t.className=''},2800)}
async function api(p,o,quiet){try{const r=await fetch(p,o);const t=await r.text();try{return JSON.parse(t)}catch(e){return{ok:r.ok}}}catch(e){if(!quiet)toast('Device unreachable',1);return null}}
const post=(p,b)=>api(p,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b||{})});
const col=p=>p<60?'#46c15a':p<85?'#e0a82e':'#ff6b63';
const tcol=t=>t<60?'#46c15a':t<75?'#e0a82e':t<85?'#f0883e':'#ff6b63';
const fmtB=b=>b>=1048576?(b/1048576).toFixed(1)+' MB':Math.round(b/1024)+' KB';
const rate=k=>k>=1024?(k/1024).toFixed(1)+' MB/s':Math.round(k)+' KB/s';
const dur=s=>{const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return(d?d+'d ':'')+h+'h '+m+'m'};
function setBar(id,p,c){const e=$(id);e.style.width=Math.min(100,Math.max(0,p))+'%';e.style.background=c}

/* ---------- tabs (keyboard accessible) ---------- */
const TABS=['dash','remote','image','video','disp','sys'];
function tab(t,focus){TABS.forEach(k=>{const on=k===t,b=$('#tab-'+k);b.setAttribute('aria-selected',on);b.tabIndex=on?0:-1;$('#t-'+k).hidden=!on});history.replaceState(null,'','#'+t);if((t==='remote'||t==='sys')&&typeof rmLoad==='function')rmLoad();if(focus)$('#tab-'+t).focus()}
TABS.forEach(k=>{$('#tab-'+k).onclick=()=>tab(k)});
$('#nav').addEventListener('keydown',e=>{const i=TABS.indexOf(document.activeElement.id.replace('tab-',''));if(i<0)return;let n=-1;
 if(e.key==='ArrowRight')n=(i+1)%TABS.length;else if(e.key==='ArrowLeft')n=(i+TABS.length-1)%TABS.length;else if(e.key==='Home')n=0;else if(e.key==='End')n=TABS.length-1;
 if(n>=0){e.preventDefault();tab(TABS[n],true)}});

/* ---------- switches with ON/OFF text + message ---------- */
function syncSw(inp){const b=inp.closest('label').querySelector('.oo');if(b){b.textContent=inp.checked?'ON':'OFF';b.className='oo '+(inp.checked?'on':'off')}}
function onSw(inp,name,fn){inp.addEventListener('change',async()=>{syncSw(inp);const r=await fn(inp.checked);toast(name+': '+(inp.checked?'ON':'OFF'),!(r&&r.ok))})}
const swHtml=(id,label)=>'<label class=sw><input type=checkbox role=switch id='+id+'><span class=trk aria-hidden=true></span><span class=vh>'+label+'</span><b class="oo off" aria-hidden=true>OFF</b></label>';

/* ---------- display + media options ---------- */
onSw($('#holdSw'),'Hold on uploaded media',v=>post('/api/settings',{hold:v,save:true}));
function fillForms(s){
 $('#gifSkip').value=s.gifSkip?'1':'0';
 $('#clk12').checked=!!s.clock12;syncSw($('#clk12'));$('#clkSec').checked=s.clockSec!==false;syncSw($('#clkSec'));
 $('#avol').value=s.vol==null?60:s.vol;$('#avv').textContent=(s.vol==null?60:s.vol)+'%';$('#bright').value=s.bright;$('#brv').textContent=Math.round(s.bright/2.55)+'%';$('#rot').value=s.rot;
 $('#holdSw').checked=s.hold;syncSw($('#holdSw'));
 $('#pcIp').value=s.pc.ip;$('#pcPort').value=s.pc.port;$('#ssid').value=s.wifi.ssid}

$('#vSnd').addEventListener('change',()=>syncSw($('#vSnd')));
$('#avol').addEventListener('input',e=>{$('#avv').textContent=e.target.value+'%'});
$('#avol').addEventListener('change',async e=>{const r=await post('/api/settings',{vol:+e.target.value,save:true});toast(r&&r.ok?'Volume saved':'Failed',!(r&&r.ok))});
let bt;$('#bright').addEventListener('input',e=>{$('#brv').textContent=Math.round(e.target.value/2.55)+'%';clearTimeout(bt);bt=setTimeout(()=>{post('/api/settings',{bright:+e.target.value})},120)});
$('#bright').addEventListener('change',async e=>{const r=await post('/api/settings',{bright:+e.target.value,save:true});toast('Brightness '+Math.round(e.target.value/2.55)+'%'+(r&&r.ok?' (saved)':' (failed)'),!(r&&r.ok))});
$('#rot').addEventListener('change',async e=>{const r=await post('/api/settings',{rot:+e.target.value,save:true});toast('Orientation changed',!(r&&r.ok))});
onSw($('#clk12'),'12-hour clock',v=>post('/api/settings',{clock12:v,save:true}));
onSw($('#clkSec'),'Clock seconds',v=>post('/api/settings',{clockSec:v,save:true}));
$('#gifSkip').addEventListener('change',async e=>{const r=await post('/api/settings',{gifSkip:e.target.value==='1',save:true});toast('GIF mode: '+(e.target.value==='1'?'Smooth':'All frames'),!(r&&r.ok))});
$('#bReset').onclick=async()=>{if(!confirm('Reset display settings to default? (WiFi/PC settings thakbe)'))return;const r=await post('/api/reset');toast(r&&r.ok?'Defaults restored':'Failed',!(r&&r.ok));formInit=false;loadState()};

/* ---------- system ---------- */
$('#bSavePc').onclick=async()=>{const b={pcIp:$('#pcIp').value.trim(),pcPort:Math.round($('#pcPort').value),save:true};const tk=$('#rmtTok').value.trim();if(tk)b.rmt={token:tk};const r=await post('/api/settings',b);$('#rmtTok').value='';toast(r&&r.ok?'PC settings saved':'Failed',!(r&&r.ok));formInit=false;loadState()};
$('#bIdle').onclick=async()=>{const v=Math.max(0,Math.min(240,parseInt($('#idleMin').value)||0));const r=await post('/api/settings',{idle:v});toast(r&&r.ok?'Saved':'Save failed',!(r&&r.ok))};
$('#bOta').onclick=()=>{const f=$('#otaFile').files[0];if(!f){toast('Choose firmware.bin first',1);return}
 if(!confirm('Upload '+f.name+' ('+Math.round(f.size/1024)+' KB) and restart the device?'))return;
 const fd=new FormData();fd.append('fw',f,f.name);const x=new XMLHttpRequest();x.open('POST','/api/ota');
 $('#bOta').disabled=true;
 x.upload.onprogress=e=>{if(e.lengthComputable){const p=Math.round(e.loaded/e.total*100);$('#otaPg').value=p;$('#otaSt').textContent='Uploading '+p+'%'}};
 x.onload=()=>{let r={};try{r=JSON.parse(x.responseText)}catch(e){}$('#bOta').disabled=false;
  $('#otaSt').textContent=r.ok?'Done - the device is restarting, this page reloads in 15 s':'Failed: '+(r.err||('HTTP '+x.status));
  if(r.ok)setTimeout(()=>location.reload(),15000)};
 x.onerror=()=>{$('#bOta').disabled=false;$('#otaSt').textContent='Connection lost (the device may already be restarting)'};
 x.send(fd)};
$('#bRkShow').onclick=async()=>{const r=await api('/api/passwd?key=1');if(r&&r.key)$('#rkv').textContent=r.key};
$('#bRkNew').onclick=async()=>{if(!confirm('Make a new recovery key? The old one stops working.'))return;const r=await post('/api/passwd',{newkey:true});if(r&&r.key){$('#rkv').textContent=r.key;toast('New recovery key - save it')}else toast('Failed',1)};
$('#showPw2').onchange=e=>{$('#pw1').type=$('#pw2').type=e.target.checked?'text':'password'};
function pwDone(r,msg){if(r&&r.ok){toast(msg);$('#pwMsg').textContent=msg+' - the page reloads, sign in again';setTimeout(()=>location.reload(),1500)}else toast((r&&r.err)||'Failed',1)}
$('#bPw').onclick=async()=>{const a=$('#pw1').value,b=$('#pw2').value;if(a.length<4||a.length>32){toast('Password must be 4-32 characters',1);return}if(a!==b){toast('Passwords do not match',1);return}pwDone(await post('/api/passwd',{pw:a}),'Password changed')};
$('#bPwDef').onclick=async()=>{if(!confirm('Set the password back to "admin"?'))return;pwDone(await post('/api/passwd',{reset:true}),'Password reset to admin')};
api('/api/passwd',null,true).then(r=>{if(r&&r.def)$('#pwBanner').hidden=false});
$('#bReboot').onclick=async()=>{if(!confirm('Reboot device?'))return;await post('/api/reboot');toast('Rebooting...')};
$('#showPw').onchange=e=>{$('#pass').type=e.target.checked?'text':'password'};
function renderNets(a){const m={};a.forEach(n=>{if(!m[n.s]||m[n.s].r<n.r)m[n.s]=n});const l=Object.values(m).sort((x,y)=>y.r-x.r);
 $('#nets').innerHTML=l.length?l.map(n=>'<li><button type=button data-s="'+esc(n.s)+'"><span>'+esc(n.s)+'</span><span class=mut>'+(n.e?'secured':'open')+' &middot; '+n.r+' dBm</span></button></li>').join(''):'<li class=mut style="padding:8px">No networks found</li>';
 $$('#nets button').forEach(b=>{b.onclick=()=>{$('#ssid').value=b.dataset.s;$('#pass').focus();toast('Selected: '+b.dataset.s)}})}
async function scan(){const st=$('#scanSt'),btn=$('#bScan');st.textContent='Scanning...';btn.disabled=true;await api('/api/scan?start=1');
 for(let i=0;i<20;i++){await sleep(1000);const r=await api('/api/scan',null,true);if(r&&r.state==='done'){renderNets(r.nets||[]);st.textContent=(r.nets||[]).length+' networks found';btn.disabled=false;return}}
 st.textContent='Scan timed out';btn.disabled=false}
$('#bScan').onclick=scan;
function wmsg(t){$('#wifiMsg').textContent=t}
$('#bConn').onclick=async()=>{const ssid=$('#ssid').value.trim();if(!ssid){toast('Enter a network name',1);return}
 const r=await post('/api/wifi',{ssid:ssid,pass:$('#pass').value});if(!r||!r.ok){toast('Failed: '+((r&&r.err)||'error'),1);return}
 wmsg('Connecting to '+ssid+'... (device hotspot-o chalu thakbe)');
 for(let i=0;i<25;i++){await sleep(1000);const s=await api('/api/wifi/status',null,true);if(!s)continue;
  if(s.state==='ok'){wmsg('Connected! IP: '+s.ip+'. Phone/PC-ke oi WiFi-te niye http://'+s.ip+' ba http://'+s.mdns+' kholo.');toast('WiFi connected: '+s.ip);loadState();return}
  if(s.state==='fail'){wmsg('Could not connect. SSID/password check kore abar try koro.');toast('WiFi connection failed',1);return}}
 wmsg('Still trying... Status check korte abar page reload koro.')};
$('#bApStop').onclick=async()=>{await post('/api/ap/stop');toast('Hotspot closing...')};

/* ---------- state ---------- */
function renderSys(s){$('#sysInfo').innerHTML=[['PC hostname',s.pc.host||'-'],['ESP32 IP',s.wifi.ip],['mDNS name',s.wifi.mdns],['Signal',s.wifi.rssi+' dBm'],['Uptime',dur(s.sys.up)],['Free heap',fmtB(s.sys.heap)],['Largest free block',fmtB(s.sys.maxblock||0)],['Last reset',s.sys.reset||'-'],['Firmware',s.sys.build]].map(r=>'<tr><th scope=row>'+r[0]+'</th><td>'+esc(r[1])+'</td></tr>').join('')}
function renderMedia(m){const t=m.type;$('#nowTxt').textContent=t==='none'?'Nothing loaded.':(m.name||'untitled')+' - '+t+' '+m.w+'x'+m.h+(m.frames>1?' ('+m.frames+' frames)':'')+', '+fmtB(m.bytes)+(m.perf&&m.perf.fps>0?' | '+m.perf.src+' '+m.perf.fps.toFixed(1)+' fps, decode '+m.perf.dec.toFixed(0)+' ms, push '+m.perf.push.toFixed(0)+' ms, dropped '+m.perf.drops:'');$('#nowTxtV').textContent=$('#nowTxt').textContent;$('#bShow').disabled=$('#bClear').disabled=$('#bShowV').disabled=$('#bClearV').disabled=(t==='none')}
async function loadState(){if(busy)return;const s=await api('/api/state',null,true);if(!s||!s.pages)return;S=s;vqHint();
 if(!formInit){fillForms(s);formInit=true}
 $('#cPc').textContent='PC '+(s.pc.online?'live':'offline');$('#cPc').className='chip '+(s.pc.online?'ok':'bad');
 $('#cWifi').textContent=s.wifi.ap?'Setup hotspot':'WiFi '+s.wifi.rssi+' dBm';
 $('#cIp').textContent=s.wifi.ap&&!s.wifi.connected?'192.168.4.1':s.wifi.ip;
 $('#cUp').textContent='Up '+dur(s.sys.up);$('#rmtTokSt').textContent=s.pc.tok?'(set)':'(not set)';
 const ap=$('#apBanner');const wasHidden=ap.hidden;ap.hidden=!(s.wifi.ap&&!s.wifi.connected);if(wasHidden&&!ap.hidden)tab('sys');
 $('#bApStop').hidden=!(s.wifi.ap&&s.wifi.connected);
 phoneState(s);renderMedia(s.media);renderSys(s);renderSd(s);if(!SDL||(s.sd&&s.sd.ok!==SDL.ok))sdRefresh()}


/* ---------- SD card manager ---------- */
let SDL=null;
const KN={1:'clip',2:'image',3:'gif',4:'jpeg',5:'avi video'};
function renderSd(s){if(!s||!s.sd)return;const d=s.sd,c=$('#sdChip');c.textContent=d.ok?'SD ready':'No SD';c.className='chip '+(d.ok?'ok':'bad');
 const used=d.total?Math.round(d.used*100/d.total):0;$('#sdBar').style.width=used+'%';
 $('#sdInfo').textContent=d.ok?(fmtB(d.total-d.used)+' free of '+fmtB(d.total)+' ('+d.mode+')'):(d.err||'Insert card (FAT32) and press Refresh');
 if(!sdFormInit){$('#sdLoops').value=d.loops;$('#sdStill').value=d.still;$('#sdPre').checked=d.preload;syncSw($('#sdPre'));sdFormInit=true}
 ['#dest','#destI'].forEach(id=>{const o=$(id);[...o.options].forEach(x=>{if(x.value!=='ram')x.disabled=!d.ok});if(!d.ok&&o.value!=='ram')o.value='ram'});
 $('#bStopAll').disabled=!d.playlist;$('#bPlayAll').disabled=!d.ok}
let sdFormInit=false;
async function sdRefresh(force){const r=await api('/api/sd/list',null,!force);if(!r)return;SDL=r;const ul=$('#sdList');ul.innerHTML='';
 if(!r.ok){ul.innerHTML='<li class=mut>'+esc(r.err||'No card')+'</li>';return}
 if(!r.files.length){ul.innerHTML='<li class=mut>No videos yet. Upload with destination "Save to SD card".</li>';return}
 const GI={1:'\u{1F3AC}',2:'\u{1F5BC}',3:'\u{1F39E}',4:'\u{1F5BC}',5:'\u{1F3AC}'};
 r.files.sort((a,b)=>a.n.localeCompare(b.n)).forEach(f=>{const li=document.createElement('li');li.className='sdrow';
  const gi=document.createElement('span');gi.className='gi';gi.textContent=GI[f.k]||'\u{1F4C4}';
  const nm=document.createElement('span');nm.className='un';nm.textContent=f.n;
  const inf=document.createElement('span');inf.className='mut';inf.textContent=(KN[f.k]||'?')+(f.w?' '+f.w+'x'+f.h:'')+(f.fr?' '+f.fr+' frames':'')+' - '+fmtB(f.s);
  const pb=document.createElement('button');pb.className='btn pri';pb.textContent='\u25B6 Play';pb.setAttribute('aria-label','Play '+f.n);
  pb.onclick=async()=>{const x=await post('/api/sd/play?f='+encodeURIComponent(f.n));toast(x&&x.ok?'Playing '+f.n:'Failed: '+((x&&x.err)||'error'),!(x&&x.ok));loadState()};
  const db=document.createElement('button');db.className='btn danger';db.textContent='Delete';db.setAttribute('aria-label','Delete '+f.n);
  db.onclick=async()=>{if(!confirm('Delete '+f.n+' from SD card?'))return;const x=await post('/api/sd/delete?f='+encodeURIComponent(f.n));toast(x&&x.ok?'Deleted':'Failed',!(x&&x.ok));sdRefresh(true);loadState()};
  li.append(gi,nm,inf,pb,db);ul.appendChild(li)})}
$('#bSdRef').onclick=async()=>{await post('/api/sd/mount');await loadState();sdRefresh(true)};
$('#bPlayAll').onclick=async()=>{const x=await post('/api/sd/playall?on=1');toast(x&&x.ok?'Playlist started':'Failed: '+((x&&x.err)||'no files'),!(x&&x.ok));loadState()};
$('#bStopAll').onclick=async()=>{await post('/api/sd/playall?on=0');toast('Playlist stopped');loadState()};
$('#sdLoops').addEventListener('change',async e=>{const r=await post('/api/settings',{sdLoops:Math.max(1,+e.target.value||1),save:true});toast(r&&r.ok?'Saved':'Failed',!(r&&r.ok))});
$('#sdStill').addEventListener('change',async e=>{const r=await post('/api/settings',{sdStill:Math.max(2,+e.target.value||8),save:true});toast(r&&r.ok?'Saved':'Failed',!(r&&r.ok))});
onSw($('#sdPre'),'Preload small clips',v=>post('/api/settings',{sdPreload:v,save:true}));
async function blobExt(b){const h=new Uint8Array(await b.slice(0,4).arrayBuffer());const m=String.fromCharCode(...h);if(m==='MJV2')return'.mjv';if(m==='RGB5')return'.rgb';if(m==='GIF8')return'.gif';if(m==='SND1')return'.snd';return'.jpg'}
function sendSd(blob,name,row,play){return new Promise(async(ok,no)=>{
 const ext=await blobExt(blob),x=new XMLHttpRequest();
 x.open('POST','/sd/upload?name='+encodeURIComponent(name+ext)+'&size='+blob.size+'&play='+(play?1:0));
 x.upload.onprogress=e=>{if(e.lengthComputable)row.prog(e.loaded/e.total)};
 x.onload=()=>{let r={};try{r=JSON.parse(x.responseText)}catch(e){}r.ok?ok(r):no(new Error(r.err||'SD rejected'))};
 x.onerror=()=>no(new Error('network error'));x.timeout=3600000;x.ontimeout=()=>no(new Error('timeout'));
 const fd=new FormData();fd.append('f',blob,'media.bin');x.send(fd)})}

/* ---------- media: convert in browser, send to RAM, show instantly ---------- */
const showMedia=()=>{api('/api/page?n='+(S?S.pages.length-1:5)).then(()=>{toast('Showing media');loadState()})};$('#bShow').onclick=$('#bShowV').onclick=showMedia;
$('#bClear').onclick=$('#bClearV').onclick=async()=>{if(!confirm('Clear media from device RAM?'))return;const r=await post('/api/media/clear');toast(r&&r.ok?'Media cleared':'Failed',!(r&&r.ok));loadState()};
const tb=(c,q)=>new Promise((ok,no)=>{c.toBlob(b=>b?ok(b):no(new Error('JPEG encode failed')),'image/jpeg',q)});
/* high quality downscale: halve step by step (one big drawImage from 1080p aliases and looks harsh) */
const _sc=[document.createElement('canvas'),document.createElement('canvas')];
function dsc(ox,src,sw,sh,w,h){let cur=src,cw=sw,ch=sh,i=0;
 while(cw>=w*2||ch>=h*2){const nw=Math.max(w,Math.ceil(cw/2)),nh=Math.max(h,Math.ceil(ch/2)),c=_sc[i&1];c.width=nw;c.height=nh;const x=c.getContext('2d');x.imageSmoothingEnabled=true;x.imageSmoothingQuality='high';x.drawImage(cur,0,0,cw,ch,0,0,nw,nh);cur=c;cw=nw;ch=nh;i++}
 ox.imageSmoothingEnabled=true;ox.imageSmoothingQuality='high';ox.drawImage(cur,0,0,cw,ch,0,0,w,h)}
let BUDGET=5300000;
/* ---------- pure helpers: GIF decode, delta boxes, MJV2 pack ---------- */
function lzwDecode(data,minCode,total){
 const clear=1<<minCode,eoi=clear+1;let size=minCode+1,mask=(1<<size)-1,next=eoi+1;
 const prefix=new Uint16Array(4096),suffix=new Uint8Array(4096),stack=new Uint8Array(4097);
 const out=new Uint8Array(total);let o=0,bits=0,acc=0,di=0,prev=-1,first=0;
 for(let i=0;i<clear;i++)suffix[i]=i;
 while(o<total){
  while(bits<size){if(di>=data.length)return out;acc|=data[di++]<<bits;bits+=8}
  const code=acc&mask;acc>>=size;bits-=size;
  if(code===clear){size=minCode+1;mask=(1<<size)-1;next=eoi+1;prev=-1;continue}
  if(code===eoi)break;
  if(prev===-1){out[o++]=suffix[code];prev=code;first=suffix[code];continue}
  let sp=0,cur=code;
  if(code>=next){stack[sp++]=first;cur=prev}
  while(cur>=clear){stack[sp++]=suffix[cur];cur=prefix[cur]}
  first=suffix[cur];stack[sp++]=first;
  while(sp>0&&o<total)out[o++]=stack[--sp];
  if(next<4096){prefix[next]=prev;suffix[next]=first;next++;if(next===(1<<size)&&size<12){size++;mask=(1<<size)-1}}
  prev=code}
 return out}
function interlaceRows(h){const r=[];for(let y=0;y<h;y+=8)r.push(y);for(let y=4;y<h;y+=8)r.push(y);for(let y=2;y<h;y+=4)r.push(y);for(let y=1;y<h;y+=2)r.push(y);return r}
function clearRect(cv,W,H,r){const x0=Math.max(0,r[0]),y0=Math.max(0,r[1]),x1=Math.min(W,r[0]+r[2]),y1=Math.min(H,r[1]+r[3]);if(x1<=x0)return;for(let y=y0;y<y1;y++)cv.fill(0,(y*W+x0)*4,(y*W+x1)*4)}
function* gifFrames(buf){
 const u=new Uint8Array(buf);
 if(u.length<13||u[0]!==71||u[1]!==73||u[2]!==70)throw new Error('Not a GIF file');
 const W=u[6]|(u[7]<<8),H=u[8]|(u[9]<<8),fl=u[10];let p=13,gct=null;
 if(fl&0x80){const n=3<<((fl&7)+1);gct=u.subarray(p,p+n);p+=n}
 const cv=new Uint8ClampedArray(W*H*4);let gce=null,prevDisp=0,prevRect=null,snap=null;
 while(p<u.length){
  const b=u[p++];
  if(b===0x3B)break;
  if(b===0x21){const lab=u[p++];
   if(lab===0xF9){const pk=u[p+1];gce={disp:(pk>>2)&7,tr:(pk&1)?u[p+4]:-1,delay:(u[p+2]|(u[p+3]<<8))*10}}
   while(p<u.length&&u[p]!==0)p+=u[p]+1;p++;continue}
  if(b!==0x2C)continue;
  const fx=u[p]|(u[p+1]<<8),fy=u[p+2]|(u[p+3]<<8),fw=u[p+4]|(u[p+5]<<8),fh=u[p+6]|(u[p+7]<<8),pk=u[p+8];p+=9;
  let ct=gct;if(pk&0x80){const n=3<<((pk&7)+1);ct=u.subarray(p,p+n);p+=n}
  const minCode=u[p++];
  const chunks=[];let len=0;
  while(p<u.length&&u[p]!==0){const s=u[p];chunks.push(u.subarray(p+1,p+1+s));len+=s;p+=s+1}p++;
  const data=new Uint8Array(len);let o=0;for(const c of chunks){data.set(c,o);o+=c.length}
  const idx=lzwDecode(data,minCode,fw*fh);
  if(prevDisp===2&&prevRect)clearRect(cv,W,H,prevRect);else if(prevDisp===3&&snap)cv.set(snap);
  const disp=gce?gce.disp:0,tr=gce?gce.tr:-1;
  if(disp===3)snap=cv.slice();
  const rows=(pk&0x40)?interlaceRows(fh):null;
  if(ct){for(let r=0;r<fh;r++){const yy=fy+(rows?rows[r]:r);if(yy>=H)continue;
   for(let c=0;c<fw;c++){const xx=fx+c;if(xx>=W)continue;const ci=idx[r*fw+c];if(ci===tr)continue;const q=(yy*W+xx)*4,k=ci*3;cv[q]=ct[k];cv[q+1]=ct[k+1];cv[q+2]=ct[k+2];cv[q+3]=255}}}
  yield{w:W,h:H,delay:gce?gce.delay:0,rgba:cv};
  prevDisp=disp;prevRect=[fx,fy,fw,fh];gce=null}
}
function diffBox(a,b,w,h,thr){let x0=w,y0=h,x1=-1,y1=-1;
 for(let y=0;y<h;y++){const r=y*w*4;let hit=false;
  for(let x=0;x<w;x++){const i=r+x*4;const d=Math.abs(a[i]-b[i])+Math.abs(a[i+1]-b[i+1])+Math.abs(a[i+2]-b[i+2]);
   if(d>thr){if(x<x0)x0=x;if(x>x1)x1=x;hit=true}}
  if(hit){if(y<y0)y0=y;y1=y}}
 return x1<0?null:[x0,y0,x1,y1]}
function alignBox(bx,w,h,pad){const x0=Math.max(0,(bx[0]-pad)&~1),y0=Math.max(0,(bx[1]-pad)&~1);
 const x1=Math.min(w,(bx[2]+pad+2)&~1),y1=Math.min(h,(bx[3]+pad+2)&~1);return[x0,y0,x1-x0,y1-y0]}
function packMjv2(ents,w,h,delay,allKey){const n=ents.length;let total=16+12*n;ents.forEach(e=>{total+=e.data?e.data.length:0});
 const buf=new ArrayBuffer(total),dv=new DataView(buf),u8=new Uint8Array(buf);
 u8.set([77,74,86,50],0);dv.setUint16(4,w,true);dv.setUint16(6,h,true);dv.setUint16(8,delay,true);dv.setUint16(10,allKey?1:0,true);dv.setUint32(12,n,true);
 let t=16,o=16+12*n;
 ents.forEach(e=>{dv.setUint16(t,e.x,true);dv.setUint16(t+2,e.y,true);dv.setUint16(t+4,e.w,true);dv.setUint16(t+6,e.h,true);const s=e.data?e.data.length:0;dv.setUint32(t+8,s,true);t+=12;if(s){u8.set(e.data,o);o+=s}});
 return buf}

function preset(){const m={auto:{w:320,h:240,fps:15,q:.88,auto:true},best:{w:320,h:240,fps:12,q:.88},full:{w:320,h:240,fps:15,q:.86},smooth:{w:320,h:240,fps:20,q:.84},balanced:{w:288,h:216,fps:20,q:.86},fast:{w:240,h:180,fps:25,q:.86},wide:{w:320,h:180,fps:20,q:.86},full18:{w:160,h:120,fps:30,q:.88}}[$('#vq').value];return Object.assign({maxSec:Math.max(1,Math.min(3600,+$('#vmax').value||15)),thr:30},m)}
function vqHint(){const e=$('#vqHint');if(e){const p=S&&S.sys&&S.sys.pool;e.textContent='Frame budget on this CYD: '+(p?Math.round((p-2048)/1024)+' KB per frame':'?')+'. JPEG quality is raised as high as the budget allows. AUTO sends a test frame to the CYD, measures the real decode speed and picks the fps that plays smoothly.'+(BENCH?' Last test: '+BENCH+'.':'')}}
$('#vq').addEventListener('change',vqHint);
function fitSize(vw,vh,P){const s=Math.min(P.w/vw,P.h/vh);return[Math.max(8,Math.round(vw*s)&~1),Math.max(8,Math.round(vh*s)&~1)]}

async function imgToRgb5(file,mode){
 const bmp=await createImageBitmap(file);const W=320,H=240;
 let cw=W,ch=H,dx=0,dy=0,dw=W,dh=H,sx=0,sy=0,sw=bmp.width,sh=bmp.height;
 if(mode==='contain'){const s=Math.min(W/bmp.width,H/bmp.height);dw=Math.max(1,Math.round(bmp.width*s));dh=Math.max(1,Math.round(bmp.height*s));cw=dw;ch=dh}
 else if(mode==='cover'){const s=Math.max(W/bmp.width,H/bmp.height);sw=Math.round(W/s);sh=Math.round(H/s);sx=Math.round((bmp.width-sw)/2);sy=Math.round((bmp.height-sh)/2)}
 const c=document.createElement('canvas');c.width=cw;c.height=ch;const x=c.getContext('2d');x.imageSmoothingQuality='high';x.fillStyle='#000';x.fillRect(0,0,cw,ch);
 x.drawImage(bmp,sx,sy,sw,sh,dx,dy,dw,dh);if(bmp.close)bmp.close();
 const px=x.getImageData(0,0,cw,ch).data;const out=new ArrayBuffer(8+cw*ch*2),dv=new DataView(out);
 new Uint8Array(out).set([82,71,66,53],0);dv.setUint16(4,cw,true);dv.setUint16(6,ch,true);
 /* Floyd-Steinberg dithering to RGB565: no colour banding (much nicer gradients / skin / sky) */
 const f=new Float32Array(cw*ch*3);for(let i=0,j=0;i<px.length;i+=4,j+=3){f[j]=px[i];f[j+1]=px[i+1];f[j+2]=px[i+2]}
 let o=8;const sp=(j,v)=>{if(j>=0&&j<f.length)f[j]+=v};
 for(let y=0;y<ch;y++)for(let xx=0;xx<cw;xx++){const j=(y*cw+xx)*3;
  const r=Math.max(0,Math.min(255,f[j])),g=Math.max(0,Math.min(255,f[j+1])),b=Math.max(0,Math.min(255,f[j+2]));
  const rq=Math.round(r/255*31),gq=Math.round(g/255*63),bq=Math.round(b/255*31);
  const er=r-((rq<<3)|(rq>>2)),eg=g-((gq<<2)|(gq>>4)),eb=b-((bq<<3)|(bq>>2));
  const R=xx<cw-1,L=xx>0,D=y<ch-1;
  for(let k=0;k<3;k++){const e=k===0?er:k===1?eg:eb;
   if(R)sp(j+3+k,e*7/16);
   if(D){if(L)sp(j+cw*3-3+k,e*3/16);sp(j+cw*3+k,e*5/16);if(R)sp(j+cw*3+3+k,e/16)}}
  dv.setUint16(o,(rq<<11)|(gq<<5)|bq,true);o+=2}
 return new Blob([out])}

/* frames -> MJV2 clip. allowDelta: sudhu bodlano tile pathay (GIF/animation), na hole full frame (video, skip-able) */
async function clipBuild(w,h,P,total,drawFrame,prog,allowDelta){
 const out=document.createElement('canvas');out.width=w;out.height=h;
 const ox=out.getContext('2d',{willReadFrequently:true});ox.imageSmoothingQuality='high';
 const tmp=document.createElement('canvas'),tx=tmp.getContext('2d');
 const ents=[];let sent=null,bytes=0,nDelta=0;
 for(let j=0;j<total;j++){
  await drawFrame(j,ox,w,h);
  const cur=ox.getImageData(0,0,w,h).data;
  let rect=[0,0,w,h];
  if(sent&&allowDelta){
   const b=diffBox(sent,cur,w,h,P.thr);
   if(!b){ents.push({x:0,y:0,w:0,h:0,data:null});prog((j+1)/total);continue}
   const r=alignBox(b,w,h,4);
   if(r[2]*r[3]<=0.7*w*h){rect=r;nDelta++}
  }
  tmp.width=rect[2];tmp.height=rect[3];
  tx.drawImage(out,rect[0],rect[1],rect[2],rect[3],0,0,rect[2],rect[3]);
  /* one JPEG frame must fit the CYD video buffer (S.sys.pool bytes), else the player skips it: lower the quality until it fits */
  const cap=Math.max(8192,((S&&S.sys&&S.sys.pool)||40960)-2048);
  const enc=async qq=>new Uint8Array(await(await tb(tmp,qq)).arrayBuffer());
  let q=Math.min(P.q,0.88),u8=await enc(q);          /* <= 0.88 keeps 4:2:0 chroma = faster decode on the CYD */
  if(u8.length>cap){let lo=0.2,hi=q,best=null;        /* highest quality that still fits the CYD frame buffer */
   for(let k=0;k<6;k++){const mid=(lo+hi)/2,b=await enc(mid);if(b.length<=cap){best=b;lo=mid}else hi=mid}
   if(!best){const b=await enc(0.2);if(b.length<=cap)best=b}
   if(best)u8=best}
  if(u8.length>cap)throw new Error('Frame too complex for the CYD buffer ('+Math.round(u8.length/1024)+' KB > '+Math.round(cap/1024)+' KB): choose a smaller preset');
  if(bytes+u8.length>BUDGET)break;
  bytes+=u8.length;
  ents.push({x:rect[0],y:rect[1],w:rect[2],h:rect[3],data:u8});
  if(!sent||(rect[2]===w&&rect[3]===h))sent=new Uint8ClampedArray(cur);
  else{for(let y=rect[1];y<rect[1]+rect[3];y++){const a=(y*w+rect[0])*4;sent.set(cur.subarray(a,a+rect[2]*4),a)}}
  prog((j+1)/total)}
 if(!ents.length)throw new Error('No frames extracted');
 return new Blob([packMjv2(ents,w,h,Math.round(1000/P.fps),nDelta===0)])}

/* ---- sound track: 11025 Hz mono 8-bit, played by the CYD speaker (GPIO26). File = 'SND1' + rate + samples, stored as <clip>.snd ---- */
function sndBlob(f,sr){let pk=0;for(let i=0;i<f.length;i++){const a=Math.abs(f[i]);if(a>pk)pk=a}
 const g=pk>0.02?Math.min(4,0.92/pk):1;                       /* normalise (max x4): a small speaker needs a full-scale signal */
 const o=new Uint8Array(8+f.length);o.set([83,78,68,49]);new DataView(o.buffer).setUint32(4,sr,true);
 for(let i=0;i<f.length;i++){const v=128+Math.round(f[i]*g*127);o[8+i]=v<0?0:v>255?255:v}
 return new Blob([o])}
async function audioFromVideo(file,maxSec){
 try{if(file.size>60*1048576)return null;                      /* decodeAudioData needs the whole file + the decoded PCM in RAM: big files would freeze the tab */
  const AC=window.AudioContext||window.webkitAudioContext,ac=new AC();
  const buf=await ac.decodeAudioData(await file.arrayBuffer());if(ac.close)ac.close();
  const sr=11025,len=Math.max(1,Math.floor(Math.min(buf.duration,maxSec)*sr)),oc=new OfflineAudioContext(1,len,sr),s=oc.createBufferSource();
  s.buffer=buf;s.connect(oc.destination);s.start(0);
  const out=await oc.startRendering();return sndBlob(out.getChannelData(0),sr)}
 catch(e){return null}}
let BENCH='';
/* ask the CYD how fast it really decodes 320x240 JPEG: send the 3 heaviest-looking sample frames, keep the biggest one as the worst case */
async function benchFps(v,w,h,P){
 try{
  const cap=Math.max(8192,((S&&S.sys&&S.sys.pool)||40960)-2048),c=document.createElement('canvas');c.width=w;c.height=h;const x=c.getContext('2d');
  let worst=null;
  for(const f of [0.2,0.5,0.8]){
   v.currentTime=Math.min(v.duration*f,Math.max(0,v.duration-0.1));await Promise.race([new Promise(r=>{v.onseeked=r}),sleep(4000)]);
   dsc(x,v,v.videoWidth,v.videoHeight,w,h);
   let q=Math.min(P.q,0.88),b=await tb(c,q);
   if(b.size>cap){let lo=0.2,hi=q,best=null;for(let k=0;k<6;k++){const mid=(lo+hi)/2,bb=await tb(c,mid);if(bb.size<=cap){best=bb;lo=mid}else hi=mid}if(best)b=best}
   if(!worst||b.size>worst.size)worst=b}
  const fd=new FormData();fd.append('f',worst,'bench.jpg');
  const ab=new AbortController(),tm=setTimeout(()=>ab.abort(),8000);let r;try{r=await fetch('/api/bench',{method:'POST',body:fd,signal:ab.signal})}finally{clearTimeout(tm)}const j=await r.json();
  if(j&&j.ok&&j.fps>0){BENCH=j.decMs+' ms decode + '+j.readMs+' ms SD read per frame ('+Math.round(j.bytes/1024)+' KB) = '+j.fps+' fps possible';vqHint();return Math.max(6,Math.min(24,j.fps))}
 }catch(e){}
 return 0}
async function videoToClip(file,P,prog){
 const url=URL.createObjectURL(file);
 try{
  const v=document.createElement('video');v.muted=true;v.playsInline=true;v.preload='auto';v.src=url;
  await new Promise((ok,no)=>{v.onloadeddata=ok;v.onerror=()=>no(new Error('Browser cannot decode this video'))});
  if(!isFinite(v.duration)||v.duration<=0)throw new Error('Unknown video length');
  const dur=Math.min(v.duration,P.maxSec),[w,h]=fitSize(v.videoWidth,v.videoHeight,P);
  if(P.auto){const f=await benchFps(v,w,h,P);if(f)P.fps=f}
  const n=Math.max(1,Math.floor(dur*P.fps));
  const draw=async(j,ox)=>{v.currentTime=Math.min((j+0.5)/P.fps,Math.max(0,v.duration-0.05));await Promise.race([new Promise(r=>{v.onseeked=r}),sleep(4000)]);dsc(ox,v,v.videoWidth,v.videoHeight,w,h)};
  return await clipBuild(w,h,P,n,draw,prog,false)
 }finally{URL.revokeObjectURL(url)}}

async function gifToClip(file,P,prog){
 const buf=await file.arrayBuffer();
 let W=0,H=0;const durs=[];
 for(const f of gifFrames(buf)){W=f.w;H=f.h;durs.push(Math.max(20,f.delay||100))}
 if(!durs.length)throw new Error('Empty GIF');
 const cum=[];let t=0;durs.forEach(d=>{t+=d;cum.push(t)});
 const total=Math.max(1,Math.floor(Math.min(t,P.maxSec*1000)*P.fps/1000)),[w,h]=fitSize(W,H,P);
 const src=document.createElement('canvas');src.width=W;src.height=H;const sx=src.getContext('2d');
 const it=gifFrames(buf);let cur=it.next(),fi=0,drawn=-1;
 const draw=async(j,ox)=>{const tj=j*1000/P.fps;
  while(fi<cum.length-1&&cum[fi]<=tj){cur=it.next();fi++}
  if(drawn!==fi){sx.putImageData(new ImageData(cur.value.rgba,W,H),0,0);drawn=fi}
  ox.fillStyle='#000';ox.fillRect(0,0,w,h);dsc(ox,src,W,H,w,h)};
 return await clipBuild(w,h,P,total,draw,prog,true)}

async function decoderToClip(file,P,prog){
 const dec=new ImageDecoder({data:await file.arrayBuffer(),type:file.type});await dec.tracks.ready;
 const n=dec.tracks.selectedTrack.frameCount,durs=[];let W=0,H=0;
 for(let i=0;i<n;i++){const r=await dec.decode({frameIndex:i});if(!i){W=r.image.displayWidth;H=r.image.displayHeight}durs.push(Math.max(20,r.image.duration?r.image.duration/1000:100));r.image.close()}
 const cum=[];let t=0;durs.forEach(d=>{t+=d;cum.push(t)});
 const total=Math.max(1,Math.floor(Math.min(t,P.maxSec*1000)*P.fps/1000)),[w,h]=fitSize(W,H,P);let last=-1;
 const draw=async(j,ox)=>{const tj=j*1000/P.fps;let idx=cum.findIndex(q=>q>tj);if(idx<0)idx=n-1;
  if(idx!==last){const r=await dec.decode({frameIndex:idx});ox.fillStyle='#000';ox.fillRect(0,0,w,h);dsc(ox,r.image,W,H,w,h);r.image.close();last=idx}};
 try{return await clipBuild(w,h,P,total,draw,prog,true)}finally{dec.close()}}

function addRow(name,kind){const li=document.createElement('li');li.className='urow';
 li.innerHTML='<span class=un></span><div class=pb role=progressbar aria-valuemin=0 aria-valuemax=100 aria-valuenow=0><i></i></div><span class=ust></span>';
 li.querySelector('.un').textContent=name;(kind==='image'?$('#uplI'):$('#upl')).prepend(li);
 const bar=li.querySelector('.pb'),fill=bar.firstChild,st=li.querySelector('.ust');
 return{msg:t=>{st.textContent=t;st.style.color=''},prog:f=>{const p=Math.round(f*100);fill.style.width=p+'%';bar.setAttribute('aria-valuenow',p)},done:t=>{fill.style.width='100%';st.textContent=t;st.style.color='var(--ok)'},fail:t=>{st.textContent=t;st.style.color='var(--bad)'}}}

function sendLive(blob,name,row){return new Promise((ok,no)=>{
 const hold=$('#holdSw').checked?1:0,x=new XMLHttpRequest();
 x.open('POST','/live?size='+blob.size+'&hold='+hold+'&name='+encodeURIComponent(name));
 x.upload.onprogress=e=>{if(e.lengthComputable)row.prog(e.loaded/e.total)};
 x.onload=()=>{let r={};try{r=JSON.parse(x.responseText)}catch(e){}r.ok?ok(r):no(new Error(r.err||'rejected by device'))};
 x.onerror=()=>no(new Error('network error'));x.timeout=180000;x.ontimeout=()=>no(new Error('timeout'));
 const fd=new FormData();fd.append('f',blob,'media.bin');x.send(fd)})}

async function processFile(f,row,kind){
 const t=f.type||'',ext=(f.name.split('.').pop()||'').toLowerCase(),base=f.name.replace(/\.[^.]+$/,'').slice(0,40)||'media',P=preset();
 const isVid=t.startsWith('video/')||/^(mp4|webm|mov|m4v|mkv|ogv|avi)$/.test(ext);
 const isGif=t==='image/gif'||ext==='gif';
 const clipMode=$('#animMode').value==='clip',dest0=(kind==='image'?$('#destI'):$('#dest')).value,dest=(dest0==='ram'&&S&&S.sys&&S.sys.psramOk===false)?'sdplay':dest0;
 BUDGET=dest==='ram'?5300000:Math.max(5300000,Math.min(150000000,((S&&S.sd?S.sd.total-S.sd.used:0)-2097152)));
 let blob,snd=null;
 if(isVid){row.msg('Converting video...');blob=await videoToClip(f,P,row.prog);
  if(dest!=='ram'&&$('#vSnd').checked){try{row.msg('Extracting sound...');snd=await audioFromVideo(f,P.maxSec)}catch(e){snd=null}if(!snd)toast('No sound (no sound track, or the file is bigger than 60 MB): video only',1)}}
 else if(isGif&&clipMode){
  try{row.msg('Converting GIF...');blob=await gifToClip(f,P,row.prog)}
  catch(e){if(f.size>3145728)throw e;row.msg('Clip convert failed, sending original GIF...');blob=f}}
 else if(isGif){if(f.size>3145728)throw new Error('Original GIF >3MB: use smooth clip mode');blob=f}
 else{
  let anim=false;
  if(clipMode&&window.ImageDecoder&&/^image\/(webp|apng|png|avif)$/.test(t)){try{const d=new ImageDecoder({data:await f.arrayBuffer(),type:t});await d.tracks.ready;anim=d.tracks.selectedTrack.frameCount>1;d.close()}catch(e){anim=false}}
  if(anim){row.msg('Converting animation...');blob=await decoderToClip(f,P,row.prog)}
  else{row.msg('Converting image...');blob=await imgToRgb5(f,$('#fit').value)}
 }
 row.msg('Sending '+fmtB(blob.size)+'...');row.prog(0);
 if(dest==='ram'){await sendLive(blob,base,row);row.done('Showing now');toast('Now showing: '+base)}
 else{
  if(isVid){try{if(snd){row.msg('Sending sound '+fmtB(snd.size)+'...');row.prog(0);await sendSd(snd,base,row,false)}else await post('/api/sd/delete?f='+encodeURIComponent(base+'.snd'))}   /* no stale sound from an older upload */
   catch(e){toast('Sound not saved ('+(e&&e.message||e)+') - the video is saved without it',1)}}
  row.msg('Sending video '+fmtB(blob.size)+'...');row.prog(0);
  await sendSd(blob,base,row,dest==='sdplay');row.done(dest==='sdplay'?'Saved to SD, playing':'Saved to SD');toast('Saved to SD: '+base);sdRefresh(true)}}

async function handleFiles(list,kind){const files=[...list];if(!files.length)return;if(busy){toast('Upload already running',1);return}
 busy=true;
 for(const f of files){const row=addRow(f.name,kind);try{await processFile(f,row,kind)}catch(e){row.fail((e&&e.message)||'failed');toast('Failed: '+((e&&e.message)||'error'),1)}}
 busy=false;loadState()}
function wireDz(dz,fi,kind){
 dz.onclick=()=>fi.click();
 fi.onchange=()=>{handleFiles(fi.files,kind);fi.value=''};
 ['dragover','dragenter'].forEach(e=>dz.addEventListener(e,ev=>{ev.preventDefault();dz.classList.add('hot')}));
 ['dragleave','drop'].forEach(e=>dz.addEventListener(e,ev=>{ev.preventDefault();dz.classList.remove('hot')}));
 dz.addEventListener('drop',ev=>handleFiles(ev.dataTransfer.files,kind))}
wireDz($('#dzI'),$('#fiI'),'image');wireDz($('#dz'),$('#fi'),'video');
document.addEventListener('paste',e=>{const k=!$('#t-image').hidden?'image':(!$('#t-video').hidden?'video':null);if(!k)return;const fs=[...(e.clipboardData&&e.clipboardData.files||[])];if(fs.length)handleFiles(fs,k)});

/* ---------- PC remote editor (tabs + 10 buttons per tab) ---------- */
const $R=id=>document.getElementById(id);
/* ===== PC remote editor: tabs + 10 buttons per tab ===== */
(function(){
 const G={Media:'playpause next prev stop mute volup voldown',
  Windows:'lock desktop alttab taskview taskmgr screenshot minimize maximize snap_left snap_right run explorer settings search clipboard emoji newdesk nextdesk prevdesk gamebar project notify voicetype',
  Power:'monitor_off sleep restart shutdown cancel_shutdown closewin',
  Edit:'copy cut paste undo redo selectall save find',
  Browser:'newtab closetab reopentab nexttab prevtab refresh fullscreen back forward zoomin zoomout zoomreset addrbar incognito',
  Keys:'up down left right esc enter space tab backspace delete pgup pgdn home end',
  Slides:'slide_start slide_here blackscreen'};
 const PH={'keys:':'ctrl+shift+n','url:':'https://youtube.com','app:':'notepad','macro:':'copy|paste','goto:':'BROWSER','media:':'all'};
 const KL={'keys:':'Shortcut (e.g. ctrl+shift+n)','url:':'Link (http/https)','app:':'App / file / folder','macro:':'Actions one after another, separated by | (e.g. lock|monitor_off)','goto:':'Folder: name of the profile (tab) to open (Deck UI)','media:':'all = SD slideshow, or a file name e.g. clip.mjv (Deck UI)'};
 const REP=new Set(['volup','voldown','up','down','left','right','pgup','pgdn','zoomin','zoomout','backspace','delete','nexttab','prevtab']);
 const CK=['keys:','url:','app:','macro:','goto:','media:'];
 const ICO={},ICOLOAD=new Set(),PEND=new Set();
 const hexOf=u=>{let s='';for(const v of u)s+=(v<16?'0':'')+v.toString(16);return s};
 const rgbaHex=d=>{const o=new Uint8Array(2048);for(let k=0;k<1024;k++){const r=d.data[k*4],g=d.data[k*4+1],b=d.data[k*4+2],a=d.data[k*4+3];let v=0;if(a>=100){v=((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3);if(!v)v=0x0841}o[k*2]=v&255;o[k*2+1]=v>>8}return hexOf(o)};
 const hexUrl=h=>{const c=document.createElement('canvas');c.width=c.height=32;const x=c.getContext('2d'),im=x.createImageData(32,32);for(let k=0;k<1024;k++){const v=parseInt(h.substr(k*4+2,2)+h.substr(k*4,2),16);if(v){im.data[k*4]=((v>>11)&31)*255/31|0;im.data[k*4+1]=((v>>5)&63)*255/63|0;im.data[k*4+2]=(v&31)*255/31|0;im.data[k*4+3]=255}}x.putImageData(im,0,0);return c.toDataURL()};
 const fileHex=async f=>{const u=URL.createObjectURL(f);try{const img=new Image();img.src=u;await img.decode();const c=document.createElement('canvas');c.width=c.height=32;const x=c.getContext('2d');x.imageSmoothingQuality='high';const W=img.naturalWidth||32,H=img.naturalHeight||32,s=Math.min(32/W,32/H),w=W*s,h=H*s;x.drawImage(img,(32-w)/2,(32-h)/2,w,h);return rgbaHex(x.getImageData(0,0,32,32))}finally{URL.revokeObjectURL(u)}};
 const loadIco=async s=>{if(ICO[s]||ICOLOAD.has(s))return;ICOLOAD.add(s);try{const r=await fetch('/api/icon?s='+s,{cache:'no-store'});const t=(await r.text()).trim();ICO[s]=t.length===4096?{hex:t,url:hexUrl(t)}:{hex:'',url:''}}catch(e){ICO[s]={hex:'',url:''}}ICOLOAD.delete(s);render()};
 const usedSlots=()=>{const u=new Set();tabs.forEach(t=>t.b.forEach(b=>{if(b.i)u.add(b.i)}));return u};
 const freeSlot=()=>{const u=usedSlots();for(let s=1;s<=32;s++)if(!u.has(s))return s;return 0};
 const COLS=['#00fcf8','#00fc00','#f8b400','#f800f8','#f8fc00','#f890f8','#f8fcf8','#58acf8'];
 let acts=[],tabs=[],cur=0,sel=-1,max=8,per=10,dirty=false,loaded=false;
 const aid=id=>acts.findIndex(a=>a.id===id);
 const kindOf=id=>aid(id)>=0?id:(['url:','app:','macro:','goto:','media:'].find(p=>id.startsWith(p))||'keys:');
 const slotUser=s=>s<8?s:(s===9?8:(s===10?9:-1));
 const userSlot=u=>u<8?u:u+1;
 function mark(d){dirty=d;$R('rmState').innerHTML=d?'<span class=rm-dirty>&#9679; Not saved</span>':'Saved';$R('rmSave').disabled=!d;$R('rmRevert').disabled=!d}
 function say(m,bad){const t=$R('rmToast');t.textContent=m;t.style.borderColor=bad?'#ff4d4d':'#2ecc71';t.style.color=bad?'#ff9a9a':'#bde8c8'}
 function fillActs(){const s=$R('rmAct');s.innerHTML='';
  const used=new Set();
  Object.keys(G).forEach(g=>{const og=document.createElement('optgroup');og.label=g;
   G[g].split(' ').forEach(id=>{const k=aid(id);if(k<0)return;used.add(id);const o=document.createElement('option');o.value=id;o.textContent=acts[k].l+(acts[k].d?' (confirm)':'');og.appendChild(o)});
   s.appendChild(og)});
  const rest=acts.filter(a=>!used.has(a.id));
  if(rest.length){const og=document.createElement('optgroup');og.label='Other';rest.forEach(a=>{const o=document.createElement('option');o.value=a.id;o.textContent=a.l;og.appendChild(o)});s.appendChild(og)}
  const og=document.createElement('optgroup');og.label='Custom';
  [['keys:','Custom shortcut...'],['url:','Open link...'],['app:','Launch app / file...'],['macro:','Macro (several actions)...'],['goto:','Folder (open profile)...'],['media:','Play SD media...']].forEach(x=>{const o=document.createElement('option');o.value=x[0];o.textContent=x[1];og.appendChild(o)});
  s.appendChild(og)}
 function renderTabs(){const box=$R('rmTabs');box.innerHTML='';
  tabs.forEach((t,i)=>{const b=document.createElement('button');b.type='button';b.className='rm-tab';b.textContent=t.n||'TAB';b.setAttribute('aria-pressed',i===cur);b.onclick=()=>{cur=i;sel=-1;render()};box.appendChild(b)});
  if(tabs.length<max){const b=document.createElement('button');b.type='button';b.className='rm-tab add';b.textContent='+ Tab';b.setAttribute('aria-label','Add a tab');
   b.onclick=()=>{tabs.push({n:'TAB '+(tabs.length+1),b:[]});cur=tabs.length-1;sel=-1;render();mark(true);$R('rmName').focus();$R('rmName').select()};box.appendChild(b)}}
 function renderGrid(){const g=$R('rmGrid');g.innerHTML='';const t=tabs[cur];
  $R('rmTabName').textContent=t?(t.n+'  '+(cur+1)+'/'+tabs.length):'-';
  $R('rmPrevT').disabled=$R('rmNextT').disabled=tabs.length<2;
  for(let s=0;s<12;s++){const b=document.createElement('button');b.type='button';b.className='rm-cell';
   if(s===8||s===11){b.className+=' nav';b.disabled=true;
    if(tabs.length>1){const n=tabs[(cur+(s===11?1:tabs.length-1))%tabs.length].n;b.textContent=(s===11?'NEXT >':'< PREV');const nb=document.createElement('b');nb.textContent=n;b.appendChild(nb)}
    b.setAttribute('aria-label',(s===11?'Next':'Previous')+' tab button (automatic)');g.appendChild(b);continue}
   const u=slotUser(s);const x=t&&t.b[u];
   if(x){b.textContent=x.l;b.className+=' has';if(x.i){if(ICO[x.i]&&ICO[x.i].url){const im=new Image();im.src=ICO[x.i].url;im.width=im.height=32;im.alt='';im.style.imageRendering='pixelated';b.insertBefore(im,b.firstChild)}else loadIco(x.i)}{const sm=document.createElement('small');sm.textContent=x.id.length>16?x.id.slice(0,15)+'\u2026':x.id;b.appendChild(sm);b.title=x.id}if(x.d)b.className+=' danger';else if(x.c){b.style.borderColor=COLS[x.c&7];b.style.color=COLS[x.c&7]}b.setAttribute('aria-label','Button '+(u+1)+': '+x.l+'. Edit');b.setAttribute('aria-pressed',u===sel);b.onclick=()=>{sel=u;render();$R('rmAct').focus()}}
   else{b.className+=' empty';b.setAttribute('aria-label','Add button at position '+(u+1));
    if(!t||u!==t.b.length){b.onclick=()=>say('Fill the earlier slots first',true);b.setAttribute('aria-disabled','true')}
    else{b.textContent='+';b.onclick=()=>addBtn()}}
   g.appendChild(b)}}
 function renderIns(){const t=tabs[cur];$R('rmTabEd').hidden=!t;if(t&&document.activeElement!==$R('rmName'))$R('rmName').value=t.n;
  $R('rmTabL').disabled=cur===0;$R('rmTabR').disabled=cur>=tabs.length-1;$R('rmTabDel').disabled=tabs.length<=1;
  const x=t&&t.b[sel];$R('rmBtnForm').hidden=!x;$R('rmNone').hidden=!!x;$R('rmBtnTitle').textContent=x?'Button '+(sel+1)+' (position '+(userSlot(sel)+1)+')':'Button';
  if(!x)return;const k=kindOf(x.id);$R('rmAct').value=k;
  const c=!(aid(x.id)>=0);$R('rmCustomW').hidden=!c;
  if(c){$R('rmCustomL').textContent=KL[k];$R('rmCustom').placeholder=PH[k];if(document.activeElement!==$R('rmCustom'))$R('rmCustom').value=x.id.slice(k.length)}
  if(document.activeElement!==$R('rmLabel'))$R('rmLabel').value=x.l;$R('rmConfirm').checked=!!x.d;$R('rmRep').checked=!!x.r;$R('rmCol').value=String((+x.c||0)&7);$R('rmIcoSt').textContent=x.i?('Icon in slot '+x.i+(PEND.has(x.i)?' (uploads when you press Save)':'')):'No icon';$R('rmIcoDel').disabled=!x.i;
  $R('rmL').disabled=sel===0;$R('rmR').disabled=sel>=t.b.length-1}
 function render(){if(cur>=tabs.length)cur=tabs.length-1;if(cur<0)cur=0;renderTabs();renderGrid();renderIns()}
 function addBtn(){const t=tabs[cur];if(!t||t.b.length>=per)return;const a=acts[0];t.b.push({id:a.id,l:a.l.slice(0,12),d:a.d,r:REP.has(a.id),c:0,i:0});sel=t.b.length-1;render();mark(true);say('New button: choose an action');$R('rmAct').focus()}
 const B=()=>tabs[cur]&&tabs[cur].b[sel];
 $R('rmAct').onchange=e=>{const x=B();if(!x)return;const v=e.target.value;
  if(PH[v]){x.id=v;x.d=false;if(!x.l||aid(kindOf(x.id))>=0)x.l={'keys:':'KEYS','url:':'LINK','app:':'APP','macro:':'MACRO','goto:':'FOLDER','media:':'MEDIA'}[v];x.r=false}
  else{const a=acts[aid(v)];const old=acts.find(q=>q.l.slice(0,12)===x.l);x.id=v;x.d=a.d;x.r=REP.has(v);if(!x.l||old||PH[kindOf(x.id)]===undefined)x.l=a.l.slice(0,12)}
  render();mark(true);if(PH[v])$R('rmCustom').focus()};
 $R('rmCustom').oninput=e=>{const x=B();if(!x)return;x.id=(CK.find(p=>x.id.startsWith(p))||'keys:')+e.target.value.trim();mark(true)};
 $R('rmLabel').oninput=e=>{const x=B();if(!x)return;x.l=e.target.value;renderGrid();mark(true)};
 $R('rmIcoF').onchange=async e=>{const f=e.target.files[0];e.target.value='';const x=B();if(!f||!x)return;
  try{const h=await fileHex(f);let s=x.i||freeSlot();if(!s){toast('Icon storage is full (32 icons)',true);return}
   ICO[s]={hex:h,url:hexUrl(h)};PEND.add(s);x.i=s;render();mark(true)}catch(err){toast('Could not read that image',true)}};
 $R('rmIcoDel').onclick=()=>{const x=B();if(!x)return;x.i=0;render();mark(true)};
 $R('rmCol').onchange=e=>{const x=B();if(!x)return;x.c=+e.target.value;renderGrid();mark(true)};
 $R('rmRep').onchange=e=>{const x=B();if(!x)return;x.r=e.target.checked;mark(true)};
 $R('rmConfirm').onchange=e=>{const x=B();if(!x)return;x.d=e.target.checked;renderGrid();mark(true)};
 $R('rmName').oninput=e=>{const t=tabs[cur];if(!t)return;t.n=e.target.value.toUpperCase();renderTabs();renderGrid();mark(true)};
 $R('rmTabL').onclick=()=>{if(cur>0){[tabs[cur-1],tabs[cur]]=[tabs[cur],tabs[cur-1]];cur--;render();mark(true)}};
 $R('rmTabR').onclick=()=>{if(cur<tabs.length-1){[tabs[cur+1],tabs[cur]]=[tabs[cur],tabs[cur+1]];cur++;render();mark(true)}};
 $R('rmTabDel').onclick=()=>{if(tabs.length>1&&confirm('Delete tab "'+tabs[cur].n+'"?')){tabs.splice(cur,1);sel=-1;render();mark(true)}};
 $R('rmL').onclick=()=>{const b=tabs[cur].b;if(sel>0){[b[sel-1],b[sel]]=[b[sel],b[sel-1]];sel--;render();mark(true)}};
 $R('rmR').onclick=()=>{const b=tabs[cur].b;if(sel<b.length-1){[b[sel+1],b[sel]]=[b[sel],b[sel+1]];sel++;render();mark(true)}};
 $R('rmDel').onclick=()=>{tabs[cur].b.splice(sel,1);sel=-1;render();mark(true)};
 $R('rmPrevT').onclick=()=>{if(tabs.length>1){cur=(cur+tabs.length-1)%tabs.length;sel=-1;render()}};
 $R('rmNextT').onclick=()=>{if(tabs.length>1){cur=(cur+1)%tabs.length;sel=-1;render()}};
 $R('rmTest').onclick=async()=>{const x=B();if(!x)return;
  if(CK.includes(x.id)){toast('Enter a value first',true);return}
  const r=await post('/api/remote/send?c='+encodeURIComponent(x.id)+'&l='+encodeURIComponent(x.l));
  const ok=r&&r.ok;const m=ok?x.l+' sent to PC':(r&&r.code===2?'Bad token':'PC agent not reachable (agent running? IP/port/token?)');say(m,!ok);toast(m,!ok)};
 $R('rmSave').onclick=async()=>{
  for(const t of tabs){if(!t.n.trim()){toast('Give every tab a name',true);return}
   for(const b of t.b){if(CK.includes(b.id)){toast('A custom button in tab "'+t.n+'" has no value',true);return}}}
  const em=q=>(q&&(q.err||q.error||q.msg))||'device busy / out of RAM';
  {const used=usedSlots();for(const s of [...PEND]){if(!used.has(s)){PEND.delete(s);continue}
    const h=ICO[s].hex;let bad=null,fail=false;
    for(let o=0;o<h.length;o+=1024){   /* 4 small pieces of 512 bytes: tiny requests, the CYD has almost no free RAM */
     let q=null;for(let t=0;t<3&&!(q&&q.ok);t++)q=await api('/api/icon?s='+s+'&o='+(o/2),{method:'POST',headers:{'Content-Type':'text/plain'},body:h.slice(o,o+1024)});
     if(!(q&&q.ok)){bad=q;fail=true;break}}
    if(fail){toast('Icon upload failed (slot '+s+'): '+em(bad),true);return}
    PEND.delete(s)}}
  /* save one tab per request (small), then commit = write everything to flash */
  const T=tabs.map(t=>({n:t.n.trim(),b:t.b.map(b=>({id:b.id,l:b.l,d:!!b.d,r:!!b.r,c:(+b.c||0)&7,i:(+b.i||0)}))}));
  let r=null;
  for(let i=0;i<T.length;i++){r=null;for(let t=0;t<3&&!(r&&r.ok);t++)r=await post('/api/settings',{rmt:{ti:i,tab:T[i]}});
   if(!(r&&r.ok)){toast('Save failed (tab '+T[i].n+'): '+em(r),true);return}}
  r=await post('/api/settings',{rmt:{commit:T.length}});
  if(!(r&&r.ok)){toast('Save failed: '+em(r),true);return}
  toast(r&&r.ok?'Saved - applied on the CYD':'Save failed',!(r&&r.ok));if(r&&r.ok){mark(false);loaded=false;rmLoad()}};
 $R('rmBackup').onclick=async()=>{const used=[...usedSlots()];await Promise.all(used.map(loadIco));const icons={};used.forEach(s=>{if(ICO[s]&&ICO[s].hex)icons[s]=ICO[s].hex});const blob=new Blob([JSON.stringify({v:2,tabs:tabs.map(t=>({n:t.n,b:t.b.map(b=>({id:b.id,l:b.l,d:!!b.d,r:!!b.r,c:(+b.c||0)&7,i:(icons[b.i]?+b.i:0)}))})),icons},null,1)],{type:'application/json'});const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='cyd-remote-layout.json';document.body.appendChild(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(a.href),3000)};
 $R('rmRestore').onclick=()=>$R('rmRestoreF').click();
 $R('rmRestoreF').onchange=async e=>{const f=e.target.files[0];e.target.value='';if(!f)return;
  try{const j=JSON.parse(await f.text());if(!j||!Array.isArray(j.tabs))throw 0;
   const nt=j.tabs.slice(0,max).map(t=>({n:String(t.n||'TAB').slice(0,10).toUpperCase(),b:(Array.isArray(t.b)?t.b:[]).slice(0,per).filter(b=>b&&typeof b.id==='string'&&b.id&&b.id.length<64).map(b=>({id:b.id,l:String(b.l||'').slice(0,12),d:!!b.d,r:!!b.r,c:(+b.c||0)&7,i:(j.icons&&j.icons[b.i]&&+b.i>=1&&+b.i<=32)?+b.i:0}))}));
   if(j.icons)for(const k of Object.keys(j.icons)){const s=+k,h=j.icons[k];if(s>=1&&s<=32&&typeof h==='string'&&h.length===4096){ICO[s]={hex:h,url:hexUrl(h)};PEND.add(s)}}
   if(!nt.length)throw 0;tabs=nt;cur=0;sel=-1;render();mark(true);say('Backup loaded - press Save changes to apply')}catch(err){toast('Not a valid layout file',true)}};
 $R('rmRevert').onclick=()=>{loaded=false;rmLoad()};
 $R('rmReset').onclick=()=>{if(!confirm('Restore the default 6 tabs? (Nothing changes on the CYD until you press Save.)'))return;
  const D=[['MEDIA','playpause prev next stop mute volup voldown fullscreen space left'],['SYSTEM','lock desktop alttab taskview taskmgr screenshot minimize maximize monitor_off explorer'],['BROWSER','newtab closetab reopentab refresh back forward zoomin zoomout addrbar incognito'],['EDIT','copy cut paste undo redo selectall save find enter esc'],['SLIDES','slide_start slide_here blackscreen left right up down pgup pgdn esc'],['POWER','lock sleep restart shutdown cancel_shutdown closewin monitor_off']];
  tabs=D.map(d=>({n:d[0],b:d[1].split(' ').map(id=>{const a=acts[aid(id)];return{id,l:a.l.slice(0,12),d:a.d,r:REP.has(id),c:0,i:0}})}));cur=0;sel=-1;render();mark(true)};
 window.rmFill=function(r){
  acts=r.acts;max=r.maxTabs||8;per=r.per||10;
  if(!$R('rmAct').options.length)fillActs();
  if(!loaded||!dirty){tabs=r.tabs.map(t=>({n:t.n,b:t.b.map(x=>({id:x.id,l:x.l,d:x.d,r:!!x.r,c:(+x.c||0)&7,i:(+x.i||0)}))}));loaded=true;if(!tabs.length)tabs=[{n:'MAIN',b:[]}];render();mark(false)}
  if(r.idle!==undefined)$R('idleMin').value=r.idle;
  $R('rmtTokSt').textContent=r.tok?'(set)':'(not set)'};
 mark(false);
 window.addEventListener('beforeunload',e=>{if(dirty){e.preventDefault();e.returnValue=''}});
})();

async function rmLoad(n){n=n||0;const r=await api('/api/remote/state',undefined,true);if(r&&r.acts&&window.rmFill){window.rmFill(r);return}
 const st=document.getElementById('rmState');if(st)st.textContent=n<4?'Loading remote settings... (retry '+(n+1)+')':'Could not load remote settings - reload the page';
 if(n<4)setTimeout(()=>rmLoad(n+1),1500)}

/* ---------- phone UI remote: same apps as the device home screen ---------- */
const APPS=[['rmt','Deck','\u2328','#38BDF8','#2563EB','remote'],['ctrl','Control','\u{1F39B}','#60A5FA','#4F46E5','dash'],['gal','Reel','\u{1F3AC}','#F472B6','#DB2777','video'],
 ['wifi','Wi-Fi','\u{1F4F6}','#34D399','#059669','sys'],['clk','Standby','\u{1F551}','#FBBF24','#D97706','dash'],['set','Settings','\u2699','#94A3B8','#475569','disp']];
function buildApps(){const g=$('#apps');g.innerHTML='';
 APPS.forEach(a=>{const b=document.createElement('button');b.className='app';b.id='app-'+a[0];b.setAttribute('aria-pressed','false');b.setAttribute('aria-label','Open '+a[1]+' on the device');
  b.innerHTML='<span class=ic style="background:linear-gradient(180deg,'+a[3]+','+a[4]+')">'+a[2]+'</span><small>'+a[1]+'</small>';
  b.onclick=async()=>{const x=await post('/api/ui?cmd=open&app='+a[0]);if(!(x&&x.ok))toast('Could not open '+a[1]+': '+((x&&x.err)||'no answer'),1);if(a[5]&&a[5]!=='dash')tab(a[5]);loadState()};
  g.appendChild(b)})}
const uiCmd=(c,msg)=>async()=>{const x=await post('/api/ui?cmd='+c);toast(x&&x.ok?msg:'Failed: '+((x&&x.err)||'no answer'),!(x&&x.ok));loadState()};
$('#uHome').onclick=uiCmd('home','Home');$('#uLock').onclick=uiCmd('lock','Locked');$('#uSleep').onclick=uiCmd('sleep','Screen off');$('#uWake').onclick=uiCmd('wake','Screen on');
$('#uStop').onclick=uiCmd('stop','Video stopped');$('#uPrev').onclick=uiCmd('mprev','Previous');$('#uNext').onclick=uiCmd('mnext','Next');
let ubt;$('#uBr').addEventListener('input',e=>{$('#uBrv').textContent=Math.round(e.target.value/2.55)+'%';clearTimeout(ubt);ubt=setTimeout(()=>{post('/api/settings',{bright:+e.target.value})},120)});
$('#uBr').addEventListener('change',async e=>{const r=await post('/api/settings',{bright:+e.target.value,save:true});toast('Brightness '+Math.round(e.target.value/2.55)+'%'+(r&&r.ok?' (saved)':' (failed)'),!(r&&r.ok))});
const SCR={lock:'Lock screen',home:'Home',mon:'Monitor',cpu:'CPU',mem:'Memory',net:'Network',tasks:'Tasks',rmt:'Remote',clk:'Clock',gal:'Gallery',set:'Settings',info:'Info',player:'Playing media'};
function phoneState(s){const u=s.ui||{};
 const name=u.asleep?'Screen off':(SCR[u.scr]||u.scr||'');$('#scrNow').textContent=name?'\u2014 on device: '+name:'';$('#scrTxt').textContent=name||'Control Panel';
 APPS.forEach(a=>{const b=$('#app-'+a[0]);if(b)b.setAttribute('aria-pressed',u.scr===a[0])});
 if(document.activeElement!==$('#uBr')){$('#uBr').value=s.bright;$('#uBrv').textContent=Math.round(s.bright/2.55)+'%'}}
buildApps();

/* ---------- start ---------- */
{let h=location.hash.slice(1);if(h==='media')h='image';if(TABS.indexOf(h)>=0)tab(h)}
vqHint();
loadState();
setInterval(()=>{if(!document.hidden)loadState()},4000);
</script></body></html>)UI";
