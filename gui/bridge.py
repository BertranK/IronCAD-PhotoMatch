"""Bounded, one-request-per-connection JSON transport to the IronCAD host thread."""
import ctypes as ct
from ctypes import wintypes as wt
import json
import os
import threading
import time
import uuid


class BridgeError(RuntimeError):
    pass


class Overlapped(ct.Structure):
    _fields_ = [("Internal", ct.c_size_t), ("InternalHigh", ct.c_size_t),
                ("Offset", wt.DWORD), ("OffsetHigh", wt.DWORD), ("hEvent", wt.HANDLE)]


kernel = ct.WinDLL("kernel32", use_last_error=True)
for name, restype, args in [
    ("CreateFileW", wt.HANDLE, [wt.LPCWSTR, wt.DWORD, wt.DWORD, ct.c_void_p, wt.DWORD, wt.DWORD, wt.HANDLE]),
    ("CreateEventW", wt.HANDLE, [ct.c_void_p, wt.BOOL, wt.BOOL, wt.LPCWSTR]),
    ("WaitNamedPipeW", wt.BOOL, [wt.LPCWSTR, wt.DWORD]),
    ("SetNamedPipeHandleState", wt.BOOL, [wt.HANDLE, ct.c_void_p, ct.c_void_p, ct.c_void_p]),
    ("ReadFile", wt.BOOL, [wt.HANDLE, ct.c_void_p, wt.DWORD, ct.c_void_p, ct.POINTER(Overlapped)]),
    ("WriteFile", wt.BOOL, [wt.HANDLE, ct.c_void_p, wt.DWORD, ct.c_void_p, ct.POINTER(Overlapped)]),
    ("GetOverlappedResult", wt.BOOL, [wt.HANDLE, ct.POINTER(Overlapped), ct.POINTER(wt.DWORD), wt.BOOL]),
    ("WaitForSingleObject", wt.DWORD, [wt.HANDLE, wt.DWORD]),
    ("CancelIoEx", wt.BOOL, [wt.HANDLE, ct.POINTER(Overlapped)]),
    ("CloseHandle", wt.BOOL, [wt.HANDLE]),
]:
    fn = getattr(kernel, name)
    fn.restype, fn.argtypes = restype, args


def available_hosts():
    return sorted(int(name.removeprefix("PhotoMatchProto-"))
                  for name in os.listdir("\\\\.\\pipe\\")
                  if name.startswith("PhotoMatchProto-") and name.removeprefix("PhotoMatchProto-").isdigit())


class Bridge:
    def __init__(self, host=None, timeout_ms=5000):
        self.host = host
        self.timeout_ms = timeout_ms
        self._lock = threading.Lock()

    def _io(self, handle, buffer, size, write=False):
        event = kernel.CreateEventW(None, True, False, None)
        if not event:
            raise ct.WinError(ct.get_last_error())
        pending = Overlapped(hEvent=event)
        count = wt.DWORD()
        try:
            fn = kernel.WriteFile if write else kernel.ReadFile
            if not fn(handle, buffer, size, None, ct.byref(pending)) and ct.get_last_error() != 997:
                raise ct.WinError(ct.get_last_error())
            if kernel.WaitForSingleObject(event, self.timeout_ms) != 0:
                kernel.CancelIoEx(handle, ct.byref(pending))
                kernel.GetOverlappedResult(handle, ct.byref(pending), ct.byref(count), True)
                raise BridgeError("IronCAD 응답 대기 시간이 지났습니다. 상태를 확인한 뒤 다시 시도하세요.")
            if not kernel.GetOverlappedResult(handle, ct.byref(pending), ct.byref(count), False):
                raise ct.WinError(ct.get_last_error())
            return count.value
        finally:
            kernel.CloseHandle(event)

    def call(self, command, session="", args=None):
        with self._lock:
            if self.host is None:
                hosts = available_hosts()
                if len(hosts) != 1:
                    raise BridgeError("IronCAD에서 PhotoMatch를 켜세요." if not hosts else "여러 IronCAD가 열려 있습니다. 해당 IronCAD의 PhotoMatch 버튼으로 실행하세요.")
                self.host = hosts[0]
            name = "\\\\.\\pipe\\PhotoMatchProto-" + str(self.host)
            request = {"id": uuid.uuid4().hex, "command": command, "session": session,
                       "args": args or {}, "deadline_ms": int(time.time()*1000)+self.timeout_ms}
            encoded = json.dumps(request, ensure_ascii=False, allow_nan=False).encode("utf-8")
            if len(encoded) > 65536:
                raise BridgeError("명령 크기가 너무 큽니다.")
            if not kernel.WaitNamedPipeW(name, self.timeout_ms):
                raise BridgeError("IronCAD 연결을 확인하세요.")
            handle = kernel.CreateFileW(name, 0xC0000000, 0, None, 3, 0x40000000, None)
            if handle == ct.c_void_p(-1).value:
                raise BridgeError("IronCAD에 연결할 수 없습니다.")
            try:
                mode = wt.DWORD(2)
                if not kernel.SetNamedPipeHandleState(handle, ct.byref(mode), None, None):
                    raise ct.WinError(ct.get_last_error())
                self._io(handle, ct.create_string_buffer(encoded), len(encoded), write=True)
                buffer = ct.create_string_buffer(4*1024*1024)
                count = self._io(handle, buffer, len(buffer))
                response = json.loads(buffer.raw[:count])
                if response.get("id") != request["id"]:
                    raise BridgeError("응답을 확인할 수 없습니다. 상태를 다시 읽으세요.")
                if not response.get("ok"):
                    raise BridgeError(response.get("error", "IronCAD 작업에 실패했습니다."))
                return response["state"]
            finally:
                kernel.CloseHandle(handle)
