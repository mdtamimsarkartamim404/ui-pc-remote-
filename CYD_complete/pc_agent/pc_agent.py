#!/usr/bin/env python3
"""
CYD PC agent  -  monitor (/stats) + remote control (/cmd) in ONE program, ONE port.

  GET /ping                      -> {"agent": "pc_agent", ...}          (the CYD uses it to find the port)
  GET /stats                     -> JSON: host, name, cpu, ram, win, wapp, time, date ...
  GET /cmd?c=<action>&l=<label>&k=<token>
                                  -> 200 ok | 403 bad token | 400/404 unknown or failed action

Token: run set_token.bat (or:  python pc_agent.py --set-token) and type the SAME token as in the
CYD web panel (System tab).  With an empty token the dangerous actions (shutdown, restart, sleep,
app:, url:) are refused.

Port 8080 is preferred; if it is busy the next free port (up to +12) is used - the CYD finds it by itself.
Only needs Python 3 and (optional, for /stats) psutil:  pip install psutil
"""
import hmac
import json
import os
import platform
import shutil
import socket
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

try:
    import psutil
except Exception:  # stats still work (reduced) without psutil
    psutil = None

VERSION = "2.0"
HERE = os.path.dirname(os.path.abspath(__file__))
CFG_FILE = os.path.join(HERE, "agent_config.json")
DEFAULT_PORT = 8080
IS_WIN = os.name == "nt"
NO_WINDOW = 0x08000000 if IS_WIN else 0          # CREATE_NO_WINDOW: helper programs never flash a console


def log(msg):
    print(time.strftime("%H:%M:%S") + "  " + msg, flush=True)


# ------------------------------------------------------------------ config
def save_cfg(c):
    with open(CFG_FILE, "w", encoding="utf-8") as f:
        json.dump(c, f, indent=2)


def load_cfg():
    c = {"token": "", "port": DEFAULT_PORT}
    try:
        with open(CFG_FILE, "r", encoding="utf-8") as f:
            c.update(json.load(f))
    except FileNotFoundError:
        try:
            save_cfg(c)
        except OSError:
            pass
    except Exception as e:  # broken json: keep defaults, do not crash
        log("config error (%s) - using defaults" % e)
    c["token"] = str(c.get("token", ""))
    return c


CFG = load_cfg()

# ------------------------------------------------------------------ keyboard
VK = {
    "ctrl": 0x11, "shift": 0x10, "alt": 0x12, "win": 0x5B,
    "enter": 0x0D, "esc": 0x1B, "tab": 0x09, "space": 0x20, "backspace": 0x08,
    "delete": 0x2E, "insert": 0x2D, "up": 0x26, "down": 0x28, "left": 0x25, "right": 0x27,
    "home": 0x24, "end": 0x23, "pgup": 0x21, "pgdn": 0x22,
    "plus": 0xBB, "minus": 0xBD, "comma": 0xBC, "period": 0xBE,
    "playpause": 0xB3, "next": 0xB0, "prev": 0xB1, "stop": 0xB2,
    "mute": 0xAD, "voldown": 0xAE, "volup": 0xAF,
    "printscreen": 0x2C, "capslock": 0x14, "menu": 0x5D,
}
for _n in range(1, 25):
    VK["f%d" % _n] = 0x70 + _n - 1
ALIAS = {
    "control": "ctrl", "return": "enter", "escape": "esc", "windows": "win", "super": "win", "cmd": "win",
    "pageup": "pgup", "pagedown": "pgdn", "del": "delete", "ins": "insert", "bksp": "backspace",
    "spacebar": "space", "prtsc": "printscreen",
}
EXTENDED = {0x26, 0x28, 0x25, 0x27, 0x24, 0x23, 0x21, 0x22, 0x2E, 0x2D, 0x5B, 0x5D}
KEYEVENTF_EXTENDEDKEY = 0x1
KEYEVENTF_KEYUP = 0x2

# action id (as sent by the CYD) -> key combination
COMBOS = {
    # media
    "playpause": ["playpause"], "next": ["next"], "prev": ["prev"], "stop": ["stop"],
    "mute": ["mute"], "volup": ["volup"], "voldown": ["voldown"],
    # windows
    "desktop": ["win", "d"], "alttab": ["alt", "tab"], "taskview": ["win", "tab"],
    "taskmgr": ["ctrl", "shift", "esc"], "screenshot": ["win", "shift", "s"],
    "minimize": ["win", "down"], "maximize": ["win", "up"],
    "snap_left": ["win", "left"], "snap_right": ["win", "right"],
    "run": ["win", "r"], "explorer": ["win", "e"], "settings": ["win", "i"], "search": ["win", "s"],
    "clipboard": ["win", "v"], "emoji": ["win", "period"],
    "newdesk": ["ctrl", "win", "d"], "nextdesk": ["ctrl", "win", "right"], "prevdesk": ["ctrl", "win", "left"],
    "gamebar": ["win", "g"], "project": ["win", "p"], "notify": ["win", "n"], "voicetype": ["win", "h"],
    "closewin": ["alt", "f4"],
    # editing
    "copy": ["ctrl", "c"], "cut": ["ctrl", "x"], "paste": ["ctrl", "v"], "undo": ["ctrl", "z"],
    "redo": ["ctrl", "y"], "selectall": ["ctrl", "a"], "save": ["ctrl", "s"], "find": ["ctrl", "f"],
    # browser
    "newtab": ["ctrl", "t"], "closetab": ["ctrl", "w"], "reopentab": ["ctrl", "shift", "t"],
    "nexttab": ["ctrl", "tab"], "prevtab": ["ctrl", "shift", "tab"], "refresh": ["f5"],
    "fullscreen": ["f11"], "back": ["alt", "left"], "forward": ["alt", "right"],
    "zoomin": ["ctrl", "plus"], "zoomout": ["ctrl", "minus"], "zoomreset": ["ctrl", "0"],
    "addrbar": ["ctrl", "l"], "incognito": ["ctrl", "shift", "n"],
    # navigation keys
    "up": ["up"], "down": ["down"], "left": ["left"], "right": ["right"], "esc": ["esc"], "enter": ["enter"],
    "space": ["space"], "tab": ["tab"], "backspace": ["backspace"], "delete": ["delete"],
    "pgup": ["pgup"], "pgdn": ["pgdn"], "home": ["home"], "end": ["end"],
    # presentations
    "slide_start": ["f5"], "slide_here": ["shift", "f5"], "blackscreen": ["b"],
}
DANGEROUS = {"shutdown", "restart", "sleep"}      # + "app:" and "url:"  (refused while no token is set)


class Keyboard:
    """Sends key presses with keybd_event. `user32` can be replaced for tests."""

    def __init__(self, user32=None):
        self.u = user32
        self.lock = threading.Lock()

    def _vk(self, name):
        n = ALIAS.get(name, name)
        if n in VK:
            return VK[n]
        if len(n) == 1:
            r = self.u.VkKeyScanW(ord(n))
            if r == -1 or r == 0xFFFF:
                raise ValueError("unknown key '%s'" % name)
            return r & 0xFF
        raise ValueError("unknown key '%s'" % name)

    def press(self, names):
        vks = [self._vk(n.strip().lower()) for n in names if n.strip()]
        if not vks:
            raise ValueError("no keys")
        with self.lock:
            for vk in vks:
                self._ev(vk, 0)
                time.sleep(0.012)
            time.sleep(0.02)
            for vk in reversed(vks):
                self._ev(vk, KEYEVENTF_KEYUP)
                time.sleep(0.006)

    def _ev(self, vk, up):
        flags = up | (KEYEVENTF_EXTENDEDKEY if vk in EXTENDED else 0)
        self.u.keybd_event(vk, self.u.MapVirtualKeyW(vk, 0), flags, 0)


def _run(cmd):
    subprocess.Popen(cmd, creationflags=NO_WINDOW, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


class Backend:
    def __init__(self, user32=None):
        self.kb = Keyboard(user32)
        self.u = user32

    def lock(self):
        self.u.LockWorkStation()

    def monitor_off(self):
        self.u.SendMessageW(0xFFFF, 0x0112, 0xF170, 2)         # WM_SYSCOMMAND, SC_MONITORPOWER, off

    def sleep(self):
        _run(["rundll32.exe", "powrprof.dll,SetSuspendState", "0,1,0"])

    def restart(self):
        _run(["shutdown", "/r", "/t", "5"])

    def shutdown(self):
        _run(["shutdown", "/s", "/t", "5"])

    def cancel_shutdown(self):
        _run(["shutdown", "/a"])

    def open_url(self, url):
        if IS_WIN:
            os.startfile(url)                               # noqa (Windows only)
        else:
            import webbrowser
            webbrowser.open(url)

    def open_app(self, name):
        if IS_WIN:
            os.startfile(name)                              # noqa (Windows only)
        else:
            subprocess.Popen([name])


class DryBackend(Backend):
    """Used on non-Windows systems (testing): only logs what it would do."""

    def __init__(self):
        self.kb = Keyboard(None)
        self.kb.press = lambda names: log("(dry-run) keys " + "+".join(names))
        self.u = None

    def lock(self): log("(dry-run) lock")
    def monitor_off(self): log("(dry-run) monitor off")
    def sleep(self): log("(dry-run) sleep")
    def restart(self): log("(dry-run) restart")
    def shutdown(self): log("(dry-run) shutdown")
    def cancel_shutdown(self): log("(dry-run) cancel shutdown")
    def open_url(self, url): log("(dry-run) open url " + url)
    def open_app(self, name): log("(dry-run) open app " + name)


def make_backend():
    if IS_WIN:
        import ctypes
        u = ctypes.windll.user32
        u.VkKeyScanW.restype = ctypes.c_short
        return Backend(u)
    return DryBackend()


BACKEND = make_backend()
SIMPLE = {
    "lock": lambda b: b.lock(), "monitor_off": lambda b: b.monitor_off(), "sleep": lambda b: b.sleep(),
    "restart": lambda b: b.restart(), "shutdown": lambda b: b.shutdown(), "cancel_shutdown": lambda b: b.cancel_shutdown(),
}


def execute(action, token_set, backend=None):
    """Returns (http_status, text)."""
    b = backend or BACKEND
    a = action.strip()
    if not a:
        return 400, "empty action"
    try:
        if a.startswith("keys:"):
            b.kb.press([k for k in a[5:].lower().split("+") if k.strip()])
            return 200, "ok"
        if a.startswith("url:"):
            if not token_set:
                return 403, "set a token first (set_token.bat)"
            url = a[4:].strip()
            if not url.lower().startswith(("http://", "https://")):
                return 400, "only http/https links"
            b.open_url(url)
            return 200, "ok"
        if a.startswith("app:"):
            if not token_set:
                return 403, "set a token first (set_token.bat)"
            name = a[4:].strip()
            if not name:
                return 400, "no app"
            b.open_app(name)
            return 200, "ok"
        if a in DANGEROUS and not token_set:
            return 403, "set a token first (set_token.bat)"
        if a in SIMPLE:
            SIMPLE[a](b)
            return 200, "ok"
        if a in COMBOS:
            b.kb.press(COMBOS[a])
            return 200, "ok"
    except Exception as e:                                   # a failing action must never kill the server
        return 500, "failed: %s" % e
    return 404, "unknown action"


# ------------------------------------------------------------------ stats
_net = {"t": time.time(), "s": 0, "r": 0, "up": 0.0, "down": 0.0}
_gpu = {"t": 0.0, "v": None}
if psutil:
    try:
        _io = psutil.net_io_counters()
        _net["s"], _net["r"] = _io.bytes_sent, _io.bytes_recv
        psutil.cpu_percent(interval=None)
    except Exception:
        pass


def net_rates():                                             # Mbit/s since the last call
    if not psutil:
        return 0.0, 0.0
    now = time.time()
    dt = now - _net["t"]
    if dt >= 0.8:
        try:
            io = psutil.net_io_counters()
            _net["up"] = max(0.0, (io.bytes_sent - _net["s"]) * 8 / dt / 1e6)
            _net["down"] = max(0.0, (io.bytes_recv - _net["r"]) * 8 / dt / 1e6)
            _net["s"], _net["r"], _net["t"] = io.bytes_sent, io.bytes_recv, now
        except Exception:
            pass
    return _net["up"], _net["down"]


def gpu_info():                                              # NVIDIA only, cached 5 s
    if time.time() - _gpu["t"] < 5:
        return _gpu["v"]
    _gpu["t"] = time.time()
    _gpu["v"] = None
    exe = shutil.which("nvidia-smi")
    if exe:
        try:
            out = subprocess.check_output(
                [exe, "--query-gpu=utilization.gpu,temperature.gpu,memory.used,memory.total", "--format=csv,noheader,nounits"],
                timeout=2, creationflags=NO_WINDOW, text=True).strip().splitlines()[0]
            _gpu["v"] = [float(x) for x in out.split(",")]
        except Exception:
            pass
    return _gpu["v"]


def ascii_clip(s, n):                                        # the CYD font is ASCII only
    return (s or "").encode("ascii", "ignore").decode("ascii").strip()[:n]


def active_window():
    if not IS_WIN:
        return "", ""
    try:
        import ctypes
        u = ctypes.windll.user32
        h = u.GetForegroundWindow()
        n = u.GetWindowTextLengthW(h)
        buf = ctypes.create_unicode_buffer(n + 1)
        u.GetWindowTextW(h, buf, n + 1)
        pid = ctypes.c_ulong()
        u.GetWindowThreadProcessId(h, ctypes.byref(pid))
        app = ""
        if psutil and pid.value:
            try:
                app = psutil.Process(pid.value).name()
            except Exception:
                pass
        if app.lower().endswith(".exe"):
            app = app[:-4]
        return ascii_clip(buf.value, 50), ascii_clip(app, 23)
    except Exception:
        return "", ""


def stats():
    d = {
        "host": ascii_clip(socket.gethostname(), 23),
        "name": ascii_clip("%s %s" % (platform.system(), platform.release()), 39),
        "cpu": 0.0, "freq": 0.0, "temp": -1.0, "ram": 0.0, "ram_used": 0.0, "ram_total": 0.0,
        "disk": 0.0, "up": 0.0, "down": 0.0, "uptime": 0, "nproc": 0,
        "gpu": -1.0, "gpu_temp": -1.0, "gpu_mem_used": 0.0, "gpu_mem_total": 0.0, "dr": 0.0, "dw": 0.0,
    }
    if psutil:
        try:
            d["cpu"] = round(psutil.cpu_percent(interval=None), 1)
            f = psutil.cpu_freq()
            d["freq"] = round((f.current if f else 0) / 1000.0, 2)                 # GHz
            m = psutil.virtual_memory()
            d["ram"] = round(m.percent, 1)
            d["ram_used"] = round(m.used / 2**30, 1)                               # GB
            d["ram_total"] = round(m.total / 2**30, 1)
            d["disk"] = round(psutil.disk_usage(os.path.abspath(os.sep)).percent, 1)
            d["up"], d["down"] = [round(x, 2) for x in net_rates()]                # Mbit/s
            d["uptime"] = int(time.time() - psutil.boot_time())
            d["nproc"] = len(psutil.pids())
            try:
                t = psutil.sensors_temperatures()
                for arr in t.values():
                    if arr:
                        d["temp"] = round(arr[0].current, 1)
                        break
            except Exception:
                pass
        except Exception:
            pass
    g = gpu_info()
    if g:
        d["gpu"], d["gpu_temp"], d["gpu_mem_used"], d["gpu_mem_total"] = g[0], g[1], g[2], g[3]
    d["win"], d["wapp"] = active_window()
    d["time"] = time.strftime("%H:%M:%S")
    d["date"] = time.strftime("%d %b %Y")
    return d


# ------------------------------------------------------------------ http
class Handler(BaseHTTPRequestHandler):
    server_version = "pc_agent/" + VERSION

    def log_message(self, *a):                                # we log ourselves
        pass

    def _send(self, code, body, ctype="text/plain; charset=utf-8"):
        b = body.encode("utf-8") if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(b)))
        self.send_header("Connection", "close")
        self.end_headers()
        try:
            self.wfile.write(b)
        except OSError:
            pass

    def do_GET(self):
        u = urlparse(self.path)
        q = parse_qs(u.query, keep_blank_values=True)
        ip = self.client_address[0]
        if u.path == "/ping":
            self._send(200, json.dumps({"agent": "pc_agent", "version": VERSION, "host": socket.gethostname()}), "application/json")
        elif u.path == "/stats":
            self._send(200, json.dumps(stats()), "application/json")
        elif u.path == "/cmd":
            c = q.get("c", [""])[0]
            lab = q.get("l", [""])[0]
            k = q.get("k", [""])[0]
            tok = CFG["token"]
            if tok and not hmac.compare_digest(k.encode("utf-8"), tok.encode("utf-8")):
                log("DENIED (bad token) from %s" % ip)
                self._send(403, "bad token")
                return
            code, text = execute(c, bool(tok))
            log("%-6s %s%s from %s%s" % ("OK" if code == 200 else "ERR %d" % code, c,
                                         (" [" + lab + "]") if lab else "", ip, "" if code == 200 else "  (" + text + ")"))
            self._send(code, text)
        elif u.path == "/":
            self._send(200, "pc_agent %s running" % VERSION)
        else:
            self._send(404, "not found")


def main():
    if "--set-token" in sys.argv:
        t = input("Token (the SAME as in the CYD web panel > System; empty = no token): ").strip()
        CFG["token"] = t
        save_cfg(CFG)
        print("Saved. Restart the agent (stop_pc_agent.bat, then pc_agent_start.vbs).")
        return
    if IS_WIN:
        ThreadingHTTPServer.allow_reuse_address = False       # Windows would otherwise allow two agents on one port
    base = int(CFG.get("port", DEFAULT_PORT))
    srv = None
    for p in range(base, base + 13):
        try:
            srv = ThreadingHTTPServer(("0.0.0.0", p), Handler)
            break
        except OSError:
            continue
    if srv is None:
        log("no free port between %d and %d" % (base, base + 12))
        sys.exit(1)
    log("pc_agent %s listening on port %d  (token %s)" % (VERSION, srv.server_address[1], "SET" if CFG["token"] else "NOT SET - dangerous actions are refused"))
    if not psutil:
        log("note: psutil is not installed - /stats is reduced (pip install psutil)")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
