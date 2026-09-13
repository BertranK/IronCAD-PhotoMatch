import argparse
import json
import os
from pathlib import Path
import threading
import traceback
from preferences import Preferences, enable_per_monitor_dpi, initial_window_size, apply_titlebar_theme

ROOT = Path(__file__).resolve().parent
RUNTIME = ROOT / ".runtime"
RUNTIME.mkdir(exist_ok=True)
os.environ["TEMP"] = os.environ["TMP"] = str(RUNTIME)

if __name__ == "__main__":
    enable_per_monitor_dpi()
import webview
from bridge import Bridge, available_hosts
from project import load_image, make_project, validate_pixel
from instance import GuiInstance


class Api:
    def __init__(self, host=None):
        self._bridge = Bridge(host)
        self._window = None
        self._lock = threading.RLock()
        self._state = None
        self._image = None
        self._points = {}
        self._key = None
        self._preferences = Preferences(RUNTIME)
        self._native_theme = None

    def preferences(self):
        with self._lock:
            value = self._preferences.snapshot()
            if self._window and self._window.native is not None and self._native_theme != value["theme"]:
                apply_titlebar_theme(self._window, value["theme"])
                self._native_theme = value["theme"]
            return value

    def set_language(self, selection):
        with self._lock:
            return self._preferences.set_language(selection)

    def _accept(self, state):
        key = (state["session"], state["capture_id"])
        if key != self._key:
            self._points.clear()
            self._key = key
        self._state = state
        return {"ok": True, "state": state, "image_points": self._points.copy()}

    def call(self, command, session="", args=None):
        try:
            with self._lock:
                if command not in {"status", "capture", "pick", "stop_pick", "apply", "measure", "photo", "background", "restore"}:
                    raise ValueError("지원하지 않는 작업입니다.")
                if command in {"photo", "background"}:
                    if not self._image:
                        raise ValueError("사진을 먼저 여세요.")
                    args = {**(args or {}), "path": self._image["overlay_path"]}
                result = self._accept(self._bridge.call(command, session, args))
                if command == "restore" and not result["state"].get("restored"):
                    raise ValueError("원래 보기 복원을 확인하지 못했습니다.")
                return result
        except Exception as error:
            return {"ok": False, "error": str(error)}

    def open_image(self):
        try:
            paths = self._window.create_file_dialog(webview.FileDialog.OPEN, allow_multiple=False,
                                                     file_types=(("사진" if self._preferences.snapshot()["language"] == "ko" else "Photos") + " (*.png;*.jpg;*.jpeg;*.bmp;*.avif)",))
            if not paths:
                return {"ok": True, "cancelled": True}
            image = load_image(paths[0])
            with self._lock:
                self._image = image
                self._points.clear()
            return {"ok": True, "image": image}
        except Exception as error:
            return {"ok": False, "error": str(error)}

    def set_point(self, session, capture_id, point_id, x, y):
        try:
            with self._lock:
                state = self._bridge.call("status")
                self._accept(state)
                if (session, capture_id) != self._key or not state["captured"]:
                    raise ValueError("문서가 바뀌었습니다. 모델 점을 다시 선택하세요.")
                if point_id not in {p["id"] for p in state["points"]}:
                    raise ValueError("모델 점을 먼저 선택하세요.")
                if not self._image:
                    raise ValueError("사진을 먼저 여세요.")
                self._points[point_id] = validate_pixel(x, y, self._image)
                return {"ok": True, "image_points": self._points.copy()}
        except Exception as error:
            return {"ok": False, "error": str(error)}

    def save_project(self, session, capture_id):
        try:
            paths = self._window.create_file_dialog(webview.FileDialog.SAVE, save_filename="PhotoMatch.json", file_types=("PhotoMatch (*.json)",))
            if not paths:
                return {"ok": True, "cancelled": True}
            path = paths if isinstance(paths, str) else paths[0]
            with self._lock:
                state = self._bridge.call("status")
                self._accept(state)
                if (session, capture_id) != self._key:
                    raise ValueError("문서가 바뀌었습니다. 상태를 확인한 뒤 저장하세요.")
                data = make_project(state, self._image, self._points)
                Path(path).write_text(json.dumps(data, ensure_ascii=False, indent=2, allow_nan=False), encoding="utf-8")
            return {"ok": True, "path": str(path)}
        except Exception as error:
            return {"ok": False, "error": str(error)}

    def _closing(self):
        # A failed restore keeps the GUI available to retry; never close IronCAD.
        if self._state and self._state.get("captured"):
            if self._bridge.host not in available_hosts():
                return True
            result = self.call("restore", self._state["session"])
            if not result["ok"]:
                try:
                    self._window.evaluate_js("window.closeError("+json.dumps(result["error"])+")")
                except Exception:
                    pass
                return False

    def _activate_host(self, host):
        with self._lock:
            if host is None or host == self._bridge.host:
                return None
            target = Bridge(host)
            state = target.call("status")
            if self._bridge.host in available_hosts():
                previous = self._bridge.call("status")
                if previous.get("captured"):
                    restored = self._bridge.call("restore", previous["session"])
                    if not restored.get("restored"):
                        raise ValueError("원래 보기 복원을 확인하지 못했습니다.")
            self._bridge = target
            return self._accept(state)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", type=int)
    args = parser.parse_args()
    instance = GuiInstance(RUNTIME)
    if not instance.acquire(args.host):
        return
    try:
        api = Api(args.host)
        width, height = initial_window_size()
        theme = api._preferences.snapshot()["theme"]
        window = webview.create_window("IronCAD PhotoMatch", str(ROOT / "web" / "index.html"), js_api=api,
                                      width=width, height=height, min_size=(600, 320), background_color="#101416" if theme == "dark" else "#f5f7f8")
        api._window = window
        window.events.closing += api._closing
        minimized = threading.Event()
        window.events.minimized += minimized.set
        window.events.restored += minimized.clear
        window.events.maximized += minimized.clear
        def activate_requests():
            window.events.loaded.wait()
            while not window.events.closed.wait(.25):
                try:
                    request = instance.poll()
                    if request is None:
                        continue
                    if minimized.is_set():
                        window.restore()
                    window.show()
                    result = api._activate_host(request["host"])
                    if result:
                        window.evaluate_js("window.hostActivated(" + json.dumps(result) + ")")
                except Exception as error:
                    window.evaluate_js("window.activationError(" + json.dumps(str(error)) + ")")
        webview.start(activate_requests, gui="edgechromium", storage_path=str(RUNTIME), private_mode=False)
    finally:
        instance.close()


if __name__ == "__main__":
    try:
        main()
    except Exception:
        (RUNTIME / "startup-error.log").write_text(traceback.format_exc(), encoding="utf-8")
        raise
