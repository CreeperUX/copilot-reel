"""Verify ordinary launch uses the pointer's work area without moving the pointer.

The complete reel is clamped into the current monitor's work area. When clamping
puts the pointer outside the center dead zone, the reel must wait for center entry.
Run only when no other Copilot Reel controller is resident.
"""
from pathlib import Path
from ctypes import wintypes as w
import ctypes
import json
import os
import re
import subprocess
import time


class MonitorInfo(ctypes.Structure):
    _fields_ = [("cbSize", w.DWORD), ("rcMonitor", w.RECT),
                ("rcWork", w.RECT), ("dwFlags", w.DWORD)]


root = Path(__file__).resolve().parents[1]
data = root / 'artifacts/qa/placement'
data.mkdir(parents=True, exist_ok=True)
log = data / 'run.log'
if log.exists():
    log.unlink()
u = ctypes.WinDLL('user32', use_last_error=True)
u.SetProcessDpiAwarenessContext.argtypes = [ctypes.c_void_p]
u.SetProcessDpiAwarenessContext.restype = w.BOOL
u.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
u.FindWindowW.argtypes = [w.LPCWSTR, w.LPCWSTR]
u.FindWindowW.restype = w.HWND
u.GetCursorPos.argtypes = [ctypes.POINTER(w.POINT)]
u.GetCursorPos.restype = w.BOOL
u.MonitorFromPoint.argtypes = [w.POINT, w.DWORD]
u.MonitorFromPoint.restype = w.HANDLE
u.GetMonitorInfoW.argtypes = [w.HANDLE, ctypes.POINTER(MonitorInfo)]
u.GetMonitorInfoW.restype = w.BOOL
u.GetDpiForWindow.argtypes = [w.HWND]
u.GetDpiForWindow.restype = w.UINT
assert not u.FindWindowW('CopilotReel.Controller', 'Copilot Reel'), 'Exit the resident instance first.'
env = os.environ.copy()
env['COPILOT_REEL_DATA_DIR'] = str(data)
exe = str(root / 'build/Release/CopilotReel.exe')
p = subprocess.Popen([exe], env=env, creationflags=0x08000000)
try:
    found = None
    for _ in range(150):
        if log.exists():
            found = re.search(
                r'pointer=(-?\d+),(-?\d+); window=(-?\d+),(-?\d+),(-?\d+),(-?\d+); awaitingCenter=(true|false)',
                log.read_text(encoding='utf-8'))
            if found:
                break
        time.sleep(.03)
    assert found, 'Default launch did not log a pointer-relative, work-area-clamped preview.'
    x, y, left, top, right, bottom = map(int, found.groups()[:6])
    awaiting_center = found.group(7) == 'true'
    info = MonitorInfo(cbSize=ctypes.sizeof(MonitorInfo))
    monitor = u.MonitorFromPoint(w.POINT(x, y), 2)
    assert u.GetMonitorInfoW(monitor, ctypes.byref(info)), ctypes.get_last_error()
    work = info.rcWork
    assert work.left <= left < right <= work.right and work.top <= top < bottom <= work.bottom, \
        'The complete reel must stay inside the captured pointer monitor work area.'
    size = right - left
    assert bottom - top == size, 'The reel canvas must remain square.'
    expected_left = max(work.left, min(x - size // 2, work.right - size))
    expected_top = max(work.top, min(y - size // 2, work.bottom - size))
    assert (left, top) == (expected_left, expected_top), 'Placement must apply the minimum complete-canvas translation.'
    center_x, center_y = (left + right) / 2, (top + bottom) / 2
    dpi = u.GetDpiForWindow(u.FindWindowW('CopilotReel.Controller', 'Copilot Reel'))
    expected_guard = (x - center_x) ** 2 + (y - center_y) ** 2 > (44 * dpi / 96) ** 2
    assert awaiting_center == expected_guard, 'Clamped placement must agree with the center-entry guard.'
    if (left, top) == (x - size // 2, y - size // 2):
        assert abs(center_x - x) <= 1 and abs(center_y - y) <= 1, 'Unclamped placement must remain pointer centered.'
    current = w.POINT()
    assert u.GetCursorPos(ctypes.byref(current))
    assert (current.x, current.y) == (x, y), 'Opening the reel must not warp the cursor; do not move it during this test.'
    result = {'passed': True, 'pointer': [x, y], 'window': [left, top, right, bottom],
              'workArea': [work.left, work.top, work.right, work.bottom],
              'awaitingCenter': awaiting_center, 'cursorWarpedByTest': False}
    (data / 'results.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result))
finally:
    if p.poll() is None:
        subprocess.run([exe, '--quit'], env=env, creationflags=0x08000000, timeout=8, check=True)
        p.wait(timeout=5)
