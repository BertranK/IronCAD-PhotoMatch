import argparse
import json
import os
from pathlib import Path
import threading
import traceback
import copy
import math
import time
from preferences import Preferences, enable_per_monitor_dpi, initial_window_size, apply_titlebar_theme, follow_window_bounds

from paths import ROOT, RUNTIME
RUNTIME.mkdir(parents=True, exist_ok=True)
os.environ["TEMP"] = os.environ["TMP"] = str(RUNTIME)

if __name__ == "__main__":
    enable_per_monitor_dpi()
import webview
from bridge import Bridge, available_hosts
from project import IMAGE_FILE_PATTERNS, load_image, make_project, validate_pixel
from instance import GuiInstance


class Api:
    def __init__(self, host=None):
        self._bridge = Bridge(host)
        self._window = None
        self._lock = threading.RLock()
        self._state = None
        self._image = None
        self._points = {}
        self._review = None
        self._fit = None
        self._key = None
        self._preferences = Preferences(RUNTIME)
        self._native_theme = None
        self._preview_session = None
        self._adjustment_fit = None
        self._measured_view = None
        self._principal_estimation = False

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
            if self._key and self._points and (key[0] != self._key[0] or not state.get('points')):
                self._review = copy.deepcopy(self._review or self._state)
            self._key = key
            self._fit = None
            self._adjustment_fit = None
        if self._fit and self._fit['host_points'] != (self._review or state).get('points'):
            self._fit = None
        if self._fit and self._measured_view and self._measured_view != [state.get('camera'), state.get('viewport')]:
            for name in list(self._fit):
                if name.startswith(('screen_', 'sdk_raster_', 'integer_to_photo_')): self._fit.pop(name)
            if self._fit.get('source') == 'manual':
                self._fit['max_error_px'] = None
                self._fit['point_errors_px'] = []
                self._fit['predicted_image_px'] = []
                self._fit['precision_passed'] = False
        self._state = copy.deepcopy(state)
        return {"ok": True, "state": state, "image_points": self._points.copy(), "review": self._review, "fit": self._fit}

    def call(self, command, session="", args=None):
        try:
            with self._lock:
                if command not in {"status", "capture", "pick", "stop_pick", "apply", "measure", "photo", "photo_opacity", "background", "restore", "rebind_point"}:
                    raise ValueError("지원하지 않는 작업입니다.")
                if self._review and command not in {"status", "restore"}:
                    raise ValueError("모델 연결을 확인하세요.")
                if command in {"photo", "background"}:
                    if not self._image:
                        raise ValueError("사진을 먼저 여세요.")
                    args = {**(args or {}), "path": self._image["overlay_path"]}
                result = self._accept(self._bridge.call(command, session, args))
                if command == 'apply': self._fit = None; result['fit'] = None
                if command == "restore" and not result["state"].get("restored"):
                    raise ValueError("원래 보기 복원을 확인하지 못했습니다.")
                return result
        except Exception as error:
            return {"ok": False, "error": str(error)}

    def open_image(self):
        try:
            paths = self._window.create_file_dialog(webview.FileDialog.OPEN, allow_multiple=False,
                                                     file_types=(("사진" if self._preferences.snapshot()["language"] == "ko" else "Photos") + " (" + IMAGE_FILE_PATTERNS + ")",))
            if not paths:
                return {"ok": True, "cancelled": True}
            image = load_image(paths[0])
            with self._lock:
                if self._image or self._preview_session or (self._state or {}).get('captured'):
                    closed = self.close_image()
                    if not closed['ok']: return closed
                self._image = image
                self._review = None
                self._fit = None
                self._points.clear()
            return {"ok": True, "image": image}
        except Exception as error:
            return {"ok": False, "error": str(error)}

    def delete_point(self, session, capture_id, point_id):
        try:
            with self._lock:
                state = self._bridge.call('status'); self._accept(state)
                if (session, capture_id) != self._key:
                    raise ValueError('문서가 바뀌었습니다. 모델 점을 다시 선택하세요.')
                points = (self._review or state)['points']
                if point_id not in {p['id'] for p in points}:
                    raise ValueError('모델의 대응점이 바뀌었습니다.')
                if state.get('adjusting_camera'):
                    raise ValueError('카메라 조정을 먼저 저장하거나 취소하세요.')
                if self._review:
                    self._review['points'] = [p for p in points if p['id'] != point_id]
                else:
                    if not state.get('point_delete_supported'):
                        raise ValueError('새 IronCAD 연결 모듈이 필요합니다.')
                    state = self._bridge.call('delete_point', session, {'id': point_id, 'capture_id': capture_id})
                self._points.pop(point_id, None)
                self._fit = self._adjustment_fit = self._measured_view = self._preview_session = None
                return self._accept(state)
        except Exception as error:
            return {'ok': False, 'error': str(error)}

    def clear_points(self, session, capture_id):
        try:
            with self._lock:
                state = self._bridge.call('status'); self._accept(state)
                if (session, capture_id) != self._key:
                    raise ValueError('문서가 바뀌었습니다. 모델 점을 다시 선택하세요.')
                if state.get('adjusting_camera'):
                    raise ValueError('카메라 조정을 먼저 저장하거나 취소하세요.')
                if not self._review and self._preview_session == state['session']:
                    state = self._bridge.call('restore', session)
                    if state.get('captured') and not state.get('restored'):
                        raise ValueError('원래 보기 복원을 확인하지 못했습니다.')
                self._points.clear(); self._fit = None
                self._preview_session = None; self._adjustment_fit = None; self._measured_view = None
                return self._accept(state)
        except Exception as error:
            return {'ok': False, 'error': str(error)}

    def close_image(self):
        try:
            with self._lock:
                state = self._bridge.call('status')
                self._accept(state)
                if self._preview_session == state['session'] or (state.get('point_markers_supported') and not self._review):
                    state = self._bridge.call('close_photo' if state.get('photo_workflow_version') else 'restore', state['session'])
                    if state.get('captured') and not state.get('restored'):
                        raise ValueError('원래 보기 복원을 확인하지 못했습니다.')
                self._image = None; self._points.clear(); self._review = None; self._fit = None
                self._preview_session = None; self._adjustment_fit = None; self._measured_view = None
                return {**self._accept(state), 'image': None}
        except Exception as error:
            return {'ok': False, 'error': str(error)}

    def replace_model(self, session, capture_id):
        try:
            with self._lock:
                state = self._bridge.call('status'); self._accept(state)
                if (session, capture_id) != self._key:
                    raise ValueError('문서가 바뀌었습니다. 모델 점을 다시 선택하세요.')
                if not self._image or not self._points:
                    raise ValueError('사진과 대응점을 먼저 여세요.')
                if not state.get('document'): raise ValueError('새 모델 문서를 먼저 여세요.')
                if not state.get('replace_model_supported'): raise ValueError('새 IronCAD 연결 모듈이 필요합니다.')
                if state.get('adjusting_camera'): raise ValueError('카메라 조정을 먼저 저장하거나 취소하세요.')
                ids = [p['id'] for p in (self._review or state)['points']]
                state = self._bridge.call('replace_model', state['session'], {'ids': ids})
                self._review = None
                self._key = (state['session'], state['capture_id'])
                self._fit = self._adjustment_fit = self._measured_view = self._preview_session = None
                return self._accept(state)
        except Exception as error:
            return {'ok': False, 'error': str(error)}

    def _reconnect_points(self, session, saved):
        # Reconnect consumes references only; saved measurement/log history can
        # exceed the pipe's request limit even with a small number of points.
        fields = {'id', 'object_id', 'vertex_id', 'api_coordinates',
                  'transformed_coordinates', 'transform', 'binding_status'}
        host = {'document': saved['document'],
                'points': [{k:v for k,v in point.items() if k in fields} for point in saved['points']]}
        return self._bridge.call('reconnect', session, {'host': host})

    def reconnect_model(self):
        try:
            with self._lock:
                state = self._bridge.call('status'); self._accept(state)
                if not self._review: return self._accept(state)
                if self._review.get('document') != state.get('document'): raise ValueError('저장된 모델 문서를 먼저 여세요.')
                state = self._reconnect_points(state['session'], self._review)
                self._review = None
                self._key = (state['session'], state['capture_id'])
                return self._accept(state)
        except Exception as error:
            return {'ok': False, 'error': str(error)}

    def set_point(self, session, capture_id, point_id, x, y):
        try:
            with self._lock:
                state = self._bridge.call("status")
                self._accept(state)
                if (session, capture_id) != self._key:
                    raise ValueError("문서가 바뀌었습니다. 모델 점을 다시 선택하세요.")
                if point_id not in {p["id"] for p in (self._review or state)["points"]}:
                    raise ValueError("모델 점을 먼저 선택하세요.")
                if not self._image:
                    raise ValueError("사진을 먼저 여세요.")
                self._points[point_id] = validate_pixel(x, y, self._image)
                self._fit = None
                return {"ok": True, "image_points": self._points.copy()}
        except Exception as error:
            return {"ok": False, "error": str(error)}

    def open_project(self):
        try:
            paths = self._window.create_file_dialog(webview.FileDialog.OPEN, allow_multiple=False,
                                                     file_types=("PhotoMatch (*.json)",))
            if not paths:
                return {"ok": True, "cancelled": True}
            return self._load_project(paths[0])
        except Exception as error:
            return {"ok": False, "error": str(error)}

    def _load_project(self, path):
        data = json.loads(Path(path).read_text(encoding="utf-8"))
        if data.get("schema_version") != 2:
            raise ValueError("PhotoMatch 결과 파일을 선택하세요.")
        image = load_image(data["image"]["path"])
        if image["sha256"] != data["image"]["sha256"]:
            raise ValueError("저장한 사진과 현재 파일이 다릅니다.")
        with self._lock:
            state = self._bridge.call("status")
            saved = data["host"]
            scene_path = data.get('scene_path')
            if scene_path:
                scene_path = Path(scene_path)
                if not scene_path.is_absolute(): scene_path = Path(path).parent / scene_path
                if scene_path.suffix.lower() != '.ics' or not scene_path.is_file():
                    raise ValueError('Saved IronCAD scene is missing.')
                if not state.get('scene_project_supported'):
                    raise ValueError('Reload the updated IronCAD connection to open the saved scene.')
                if str(scene_path).casefold() != str(state.get('document')).casefold():
                    state = self._scene_call('open_scene', state['session'], {'path': str(scene_path)})
            same_session = (saved["session"], saved["capture_id"]) == (state["session"], state["capture_id"])
            def references(points):
                return [{k:v for k,v in p.items() if k != 'binding_status'} for p in points]
            if same_session and references(saved["points"]) != references(state["points"]):
                raise ValueError("모델의 대응점이 바뀌었습니다.")
            points = {p["id"]: validate_pixel(*p["image_px"], image) for p in data["correspondences"]}
            if len(points) != len(data["correspondences"]) or not set(points) <= {p["id"] for p in saved["points"]}:
                raise ValueError("모델의 대응점이 바뀌었습니다.")
            if not same_session and state.get("saved_point_reconnect") and state.get("document") == saved.get("document"):
                state = self._reconnect_points(state["session"], saved)
                same_session = True
            self._accept(state)
            self._image, self._points = image, points
            self._review = None if same_session else saved
            self._fit = None
            if same_session and data.get('fit', {}).get('source') == 'manual':
                self._fit = copy.deepcopy(data['fit'])
                self._fit.update(host_points=copy.deepcopy(state['points']), max_error_px=None,
                                 point_errors_px=[], predicted_image_px=[], precision_passed=False)
                for name in list(self._fit):
                    if name.startswith(('screen_', 'sdk_raster_', 'integer_to_photo_')): self._fit.pop(name)
                self._preview_session = state['session']
                self._measured_view = None
            return {**self._accept(state), "image": image}

    def set_principal_estimation(self, enabled):
        with self._lock:
            self._principal_estimation = bool(enabled)
            self._fit = None; self._measured_view = None
            return self._accept(self._bridge.call('status'))

    def fit_points(self, estimate_principal=None):
        try:
            from solver import fit_camera
            with self._lock:
                state = self._bridge.call('status'); self._accept(state)
                if not self._image: raise ValueError("사진을 먼저 여세요.")
                if state.get('adjusting_camera'): raise ValueError('카메라 조정을 먼저 저장하거나 취소하세요.')
                if estimate_principal is None: estimate_principal = self._principal_estimation
                image = self._image
                host_points = copy.deepcopy((self._review or state)['points'])
                matched = [p for p in host_points if p['id'] in self._points and p.get('binding_status', 'connected') == 'connected']
                if any(p['id'] in self._points and p.get('binding_status') == 'needs_reconnection' for p in host_points):
                    raise ValueError('연결이 필요한 모델 점을 다시 선택하세요.')
                pixels = copy.deepcopy(self._points); key = self._key
                camera = (state.get('camera') or (self._review or state).get('original_camera') or {}).get('pose_and_field')
            result = fit_camera([p['transformed_coordinates'] for p in matched],
                                [pixels[p['id']] for p in matched], image['width'], image['height'], camera, estimate_principal=bool(estimate_principal))
            with self._lock:
                fresh = self._bridge.call('status'); self._accept(fresh)
                if key != self._key or image is not self._image or pixels != self._points or host_points != (self._review or fresh)['points']:
                    raise ValueError("모델의 대응점이 바뀌었습니다.")
                self._fit = {**result, 'ids': [p['id'] for p in matched], 'host_points': host_points}
                self._measured_view = None
                return self._accept(fresh)
        except Exception as error:
            return {'ok': False, 'error': str(error)}

    def preview_fit(self):
        try:
            with self._lock:
                state = self._bridge.call('status'); self._accept(state)
                if self._review: raise ValueError("모델 연결을 확인하세요.")
                if state.get('adjusting_camera'): raise ValueError('카메라 조정을 먼저 저장하거나 취소하세요.')
                if not self._fit or not self._fit['stable']: raise ValueError("카메라를 먼저 계산하세요.")
                result = self._fit
                session = state['session']
                if result.get('estimate_principal') and not state.get('photo_workflow_version'):
                    raise ValueError('주점 이동을 지원하는 IronCAD 연결 모듈이 필요합니다.')
                if result.get('source') == 'manual': raise ValueError('저장한 수동 카메라는 카메라 조정에서 사용하세요.')
                self._preview_session = session
                self._bridge.call('apply', session, result['camera'])
                def measure():
                    current = self._bridge.call('measure', session)
                    for _ in range(20):
                        if not current.get('measuring'): return current
                        time.sleep(.1); current = self._bridge.call('status')
                    raise ValueError("투영 측정이 완료되지 않았습니다.")
                measure()
                self._bridge.call('photo', session, {'path': self._image['overlay_path'], 'focal_px': result['focal_px'],
                                                   'principal_px': result.get('principal_px', [self._image['width']/2, self._image['height']/2])})
                state = measure()
                rect = state.get('photo_drawn_rectangle_physical', state['photo_rectangle_physical'])
                measurement = state['measurements'][-1]
                rows = {p['id']: p for p in measurement['picked_point_projections']}
                projections = {id: p['transformed_as_world_px'] for id, p in rows.items()}
                floating_verified = measurement.get('floating_world_frame_verified') is True
                if state.get('projection_coordinate_rule') != 'sdk_pixel_endpoints_truncate_then_physical_scale':
                    raise ValueError('Unsupported IronCAD projection coordinate rule')
                pixel_size = [p/r for p, r in zip(state['viewport'], state['photo_render_size'])]
                continuous, errors, floating = [], [], []
                for id in result['ids']:
                    expected = [rect[0]+self._points[id][0]*rect[2]/self._image['width'],
                                rect[1]+self._points[id][1]*rect[3]/self._image['height']]
                    continuous.append(math.dist(projections[id], expected))
                    if floating_verified:
                        value = rows[id]['model_double_physical_px']
                        if not all(math.isfinite(x) for x in value):
                            raise ValueError('Invalid floating projection')
                        floating.append(math.dist(value, expected))
                    # Compare like-for-like with the SDK's integer render grid,
                    # then convert both positions to physical pixels.
                    raster = [math.trunc(x/step)*step for x, step in zip(expected, pixel_size)]
                    errors.append(math.dist(projections[id], raster))
                result['sdk_raster_max_error_px'] = max(errors)
                result['sdk_raster_passed'] = max(errors) <= 1
                result['screen_error_coordinate_rule'] = state['projection_coordinate_rule']
                result['screen_image_rectangle'] = rect
                result['screen_image_rectangle_verified'] = 'photo_drawn_rectangle_physical' in state
                result['integer_to_photo_max_error_px'] = max(continuous)
                # Only the native frame check authorizes floating model output
                # as world output. Raster agreement alone never grants a pass.
                result['screen_measurement_verified'] = floating_verified and result['screen_image_rectangle_verified']
                result['screen_max_error_px'] = max(floating if floating_verified else continuous)
                if floating_verified:
                    result['screen_error_coordinate_rule'] = 'frame_verified_floating_world_physical_pixels'
                result['screen_passed'] = result['screen_measurement_verified'] and result['screen_max_error_px'] <= 1
                self._measured_view = copy.deepcopy([state.get('camera'), state.get('viewport')])
                return self._accept(state)
        except Exception as error:
            return {'ok': False, 'error': str(error)}

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
                if self._review:
                    raise ValueError('Connect the saved points to the scene before saving.')
                if not state.get('scene_project_supported'):
                    raise ValueError('Reload the updated IronCAD connection before saving.')
                scene_path = Path(state.get('document') or '')
                args = {}
                if not scene_path.is_absolute():
                    scene_path = Path(path).with_suffix('.ics')
                    if scene_path.exists(): raise ValueError('A scene already exists at that name. Choose a different result filename.')
                    args['path'] = str(scene_path)
                state = self._scene_call('save_scene', state['session'], args)
                self._accept(state)
                data = make_project(self._review or state, self._image, self._points)
                data['scene_path'] = str(scene_path)
                if self._fit: data['fit'] = self._fit
                Path(path).write_text(json.dumps(data, ensure_ascii=False, indent=2, allow_nan=False), encoding="utf-8")
            return {"ok": True, "path": str(path)}
        except Exception as error:
            return {"ok": False, "error": str(error)}

    def _scene_call(self, command, session, args):
        timeout = self._bridge.timeout_ms
        try:
            self._bridge.timeout_ms = 60000
            return self._bridge.call(command, session, args)
        finally:
            self._bridge.timeout_ms = timeout

    def adjust_camera(self, action):
        try:
            with self._lock:
                state = self._bridge.call('status'); self._accept(state)
                if self._review or not self._image or self._preview_session != state['session']:
                    raise ValueError('사진 미리보기를 먼저 실행하세요.')
                if not state.get('photo_workflow_version'): raise ValueError('새 IronCAD 연결 모듈이 필요합니다.')
                if action not in {'begin', 'save', 'cancel', 'use'}: raise ValueError('지원하지 않는 작업입니다.')
                previous = copy.deepcopy(self._fit)
                args = {'action': action}
                if action == 'use' and self._fit and self._fit.get('source') == 'manual':
                    args.update(camera_state=self._fit['manual_camera_state'], path=self._image['overlay_path'],
                                focal_px=self._fit['focal_px'], principal_px=self._fit['principal_px'])
                state = self._bridge.call('adjust_camera', state['session'], args)
                self._measured_view = None
                if action == 'begin':
                    self._adjustment_fit = previous; self._fit = None
                elif action == 'cancel':
                    self._fit = self._adjustment_fit; self._adjustment_fit = None
                    if self._fit:
                        for key in list(self._fit):
                            if key.startswith(('screen_', 'sdk_raster_', 'integer_to_photo_')): self._fit.pop(key)
                        if self._fit.get('source') == 'manual':
                            self._fit.update(max_error_px=None, point_errors_px=[], predicted_image_px=[], precision_passed=False)
                            self._fit.pop('residual_source', None)
                else:
                    # Store the actual full SDK camera separately from the
                    # solver. Reproject against the unchanged photo markers.
                    camera = copy.deepcopy(state['manual_camera'])
                    self._fit = {'source': 'manual', 'manual_camera_state': camera,
                                 'camera': camera['pose_and_field'], 'host_points': copy.deepcopy(state['points']),
                                 'focal_px': state['image_focal_px'], 'principal_px': state['photo_principal_px'],
                                 'ids': [p['id'] for p in state['points'] if p['id'] in self._points],
                                 'stable': True, 'precision_passed': False, 'max_error_px': None,
                                 'point_errors_px': [], 'predicted_image_px': []}
                    self._adjustment_fit = None
                    state = self._bridge.call('measure', state['session'])
                    for _ in range(20):
                        if not state.get('measuring'): break
                        time.sleep(.1); state = self._bridge.call('status')
                    if not state.get('measuring') and state.get('measurements'):
                        measurement = state['measurements'][-1]
                        rect = state.get('photo_drawn_rectangle_physical')
                        if measurement.get('floating_world_frame_verified') and rect:
                            rows = {p['id']: p for p in measurement['picked_point_projections']}
                            predicted = [[(rows[id]['model_double_physical_px'][0]-rect[0])*self._image['width']/rect[2],
                                          (rows[id]['model_double_physical_px'][1]-rect[1])*self._image['height']/rect[3]] for id in self._fit['ids']]
                            errors = [math.dist(p, self._points[id]) for id,p in zip(self._fit['ids'], predicted)]
                            if errors and all(math.isfinite(e) for e in errors):
                                self._fit.update(predicted_image_px=predicted, point_errors_px=errors, max_error_px=max(errors),
                                                 residual_source='current_frame_verified_floating_projection')
                    self._measured_view = copy.deepcopy([state.get('camera'), state.get('viewport')])
                return self._accept(state)
        except Exception as error:
            return {'ok': False, 'error': str(error)}

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
    parser.add_argument("--smoke-test", type=Path, help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.smoke_test:
        from bundle_smoke import run
        run(args.smoke_test)
        return
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
            follow_window_bounds(window)
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
