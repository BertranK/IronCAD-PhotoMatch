"""Check the frozen GUI without connecting to or changing an IronCAD document."""
import json
from pathlib import Path


def run(report):
    import cv2
    import numpy
    import scipy.optimize
    import webview
    from PIL import Image
    from paths import ROOT, RUNTIME
    result = {'ok': False}
    window = webview.create_window('PhotoMatch package check', str(ROOT/'web/index.html'), hidden=True)

    def check():
        try:
            if not window.events.loaded.wait(30):
                raise RuntimeError('WebView2 did not load')
            if not window.evaluate_js("Boolean(document.getElementById('photoCanvas') && window.PhotoCoordinates && window.PhotoLanguage)"):
                raise RuntimeError('GUI assets did not load')
            RUNTIME.mkdir(parents=True, exist_ok=True)
            probe=RUNTIME/'package-check.png'
            Image.new('RGB',(2,2)).save(probe)
            with Image.open(probe) as im:
                im.load()
            probe.unlink()
            result.update(ok=True, opencv=cv2.__version__, numpy=numpy.__version__)
        except Exception as error:
            result['error']=str(error)
        finally:
            Path(report).write_text(json.dumps(result),encoding='utf-8')
            window.destroy()
    webview.start(check, gui='edgechromium')
