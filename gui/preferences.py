"""Windows display preferences and a per-user language override."""
import ctypes as ct
from ctypes import wintypes as wt
import hashlib
import json
import os
from pathlib import Path
import winreg


def enable_per_monitor_dpi():
    user32 = ct.WinDLL("user32", use_last_error=True)
    try:
        user32.SetProcessDpiAwarenessContext.argtypes = [ct.c_void_p]
        user32.SetProcessDpiAwarenessContext(ct.c_void_p(-4))  # Per Monitor V2, before any window.
    except AttributeError:
        user32.SetProcessDPIAware()


def windows_theme():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize") as key:
            return "light" if winreg.QueryValueEx(key, "AppsUseLightTheme")[0] else "dark"
    except OSError:
        return "light"


def windows_language():
    kernel = ct.WinDLL("kernel32")
    kernel.GetUserDefaultUILanguage.restype = wt.WORD
    language = kernel.GetUserDefaultUILanguage()
    name = ct.create_unicode_buffer(85)
    if kernel.LCIDToLocaleName(language, name, len(name), 0):
        return name.value
    return "en-US"


def initial_window_size():
    user32 = ct.WinDLL("user32")
    work = wt.RECT()
    user32.SystemParametersInfoW(48, 0, ct.byref(work), 0)
    dpi = user32.GetDpiForSystem() if hasattr(user32, "GetDpiForSystem") else 96
    scale = max(1, dpi / 96)
    return (max(600, min(1320, int((work.right-work.left)/scale)-32)),
            max(320, min(860, int((work.bottom-work.top)/scale)-48)))


def apply_titlebar_theme(window, theme):
    """Called after the native handle exists; DWM draws the standard Windows title bar."""
    if window is None or window.native is None:
        return
    handle = int(window.native.Handle.ToInt64())
    value = wt.BOOL(theme == "dark")
    dwm = ct.WinDLL("dwmapi")
    dwm.DwmSetWindowAttribute.argtypes = [wt.HWND, wt.DWORD, ct.c_void_p, wt.DWORD]
    # Unsupported systems retain the standard OS title bar.
    dwm.DwmSetWindowAttribute(handle, 20, ct.byref(value), ct.sizeof(value))


def follow_window_bounds(window):
    """Repair the embedded child bounds after Windows restores the form."""
    from System import Action
    from System.Drawing import Point
    from System.Windows.Forms import FormWindowState

    form = window.native
    user32 = ct.WinDLL("user32", use_last_error=True)
    user32.SetWindowPos.argtypes = [wt.HWND, wt.HWND, ct.c_int, ct.c_int,
                                   ct.c_int, ct.c_int, wt.UINT]
    user32.SetWindowPos.restype = wt.BOOL

    def attach():
        browser = form.browser.webview

        def align():
            if form.IsDisposed or form.WindowState == FormWindowState.Minimized:
                return
            # WinForms can retain correct managed Bounds while the native child
            # stays displaced by the minimized form's off-screen coordinates.
            if browser.PointToScreen(Point(0, 0)) != form.PointToScreen(browser.Location):
                bounds = browser.Bounds
                user32.SetWindowPos(int(browser.Handle.ToInt64()), None,
                                    bounds.X, bounds.Y, bounds.Width, bounds.Height,
                                    0x0014)  # NOZORDER | NOACTIVATE

        def resized(sender, event):
            if form.WindowState != FormWindowState.Minimized:
                form.BeginInvoke(Action(align))

        form.Resize += resized
        form.Move += resized
        align()

    form.Invoke(Action(attach))


class Preferences:
    def __init__(self, runtime):
        user = hashlib.sha256(str(Path.home()).encode()).hexdigest()[:16]
        self.path = Path(runtime) / ("instance-" + user) / "preferences.json"
        self.selection = "en"
        try:
            selection = json.loads(self.path.read_text(encoding="utf-8"))["language"]
            if selection in {"system", "ko", "en"}:
                self.selection = selection
        except (OSError, ValueError, KeyError, TypeError):
            pass

    def snapshot(self):
        language = windows_language()
        return {"theme": windows_theme(), "selection": self.selection, "system_language": language,
                "language": ("ko" if language.lower().startswith("ko") else "en") if self.selection == "system" else self.selection}

    def set_language(self, selection):
        if selection not in {"system", "ko", "en"}:
            raise ValueError("Unsupported language")
        self.path.parent.mkdir(parents=True, exist_ok=True)
        pending = self.path.with_suffix(".tmp")
        pending.write_text(json.dumps({"language": selection}), encoding="utf-8")
        os.replace(pending, self.path)
        self.selection = selection
        return self.snapshot()
