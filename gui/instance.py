"""One GUI per Windows user/install; later launches only request activation."""
import hashlib
import json
import msvcrt
import os
from pathlib import Path
import time
import uuid


class GuiInstance:
    def __init__(self, runtime):
        user = hashlib.sha256(str(Path.home()).encode()).hexdigest()[:16]
        self.directory = Path(runtime) / ("instance-" + user)
        self.directory.mkdir(parents=True, exist_ok=True)
        self.request = self.directory / "activate.json"
        self.started = time.time()
        self.lock = None

    def acquire(self, host=None):
        handle = (self.directory / "gui.lock").open("a+b")
        if handle.seek(0, 2) == 0:
            handle.write(b"0")
            handle.flush()
        handle.seek(0)
        try:
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
        except OSError:
            handle.close()
            pending = self.directory / (uuid.uuid4().hex + ".tmp")
            pending.write_text(json.dumps({"host": host, "time": time.time()}), encoding="utf-8")
            os.replace(pending, self.request)
            return False
        self.lock = handle
        return True

    def poll(self):
        pending = self.directory / (uuid.uuid4().hex + ".processing")
        try:
            os.replace(self.request, pending)
        except FileNotFoundError:
            return None
        try:
            request = json.loads(pending.read_text(encoding="utf-8"))
            if request["time"] >= self.started:
                return request
        finally:
            pending.unlink()
        return None

    def close(self):
        if self.lock:
            self.lock.close()  # Windows releases the byte lock even after a crash.
            self.lock = None
