"""Exercise screen-edge placement and safe release through the real controller.

Uses a separate configuration containing six harmless launch-probe targets. Tests
all four work-area edges and corners on the monitor under the original pointer.
Restores the physical cursor even on failure and never changes system settings.
Do not run while another Copilot Reel controller is resident or move the pointer
while this test is running.
"""
import ctypes
from ctypes import wintypes as w
import json
import math
import os
from pathlib import Path
import re
import subprocess
import time


class MonitorInfo(ctypes.Structure):
    _fields_ = [("cbSize", w.DWORD), ("rcMonitor", w.RECT),
                ("rcWork", w.RECT), ("dwFlags", w.DWORD)]


class MouseInput(ctypes.Structure):
    _fields_ = [("dx", w.LONG), ("dy", w.LONG), ("mouseData", w.DWORD),
                ("dwFlags", w.DWORD), ("time", w.DWORD), ("dwExtraInfo", w.WPARAM)]


class InputPayload(ctypes.Union):
    _fields_ = [("mouse", MouseInput)]


class Input(ctypes.Structure):
    _fields_ = [("type", w.DWORD), ("payload", InputPayload)]


root = Path(__file__).resolve().parents[1]
exe = root / 'build/Release/CopilotReel.exe'
probe = root / 'build/Release/launch_probe.exe'
assert exe.exists() and probe.exists(), 'Build both CopilotReel and launch_probe first.'
data = root / 'artifacts/qa/edges'
data.mkdir(parents=True, exist_ok=True)
marker = data / 'launch-probe.txt'
log = data / 'run.log'
for path in (marker, log, data / 'preferences.ini'):
    if path.exists():
        path.unlink()
target = str(probe).replace('\\', '\\\\')
config = '; isolated edge integration configuration\r\n[CopilotReel]\r\nVersion=1\r\nCount=6\r\n'
for index in range(6):
    config += (f'[App{index}]\r\nName=Probe {index}\r\nKind=shell\r\nTarget={target}\r\n'
               'Arguments=\r\nWorkingDirectory=\r\n')
(data / 'apps.ini').write_text(config, encoding='utf-16', newline='')
env = os.environ.copy()
env['COPILOT_REEL_DATA_DIR'] = str(data)
flags = 0x08000000
u = ctypes.WinDLL('user32', use_last_error=True)
u.SetProcessDpiAwarenessContext.argtypes = [ctypes.c_void_p]
u.SetProcessDpiAwarenessContext.restype = w.BOOL
u.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
u.FindWindowW.argtypes = [w.LPCWSTR, w.LPCWSTR]
u.FindWindowW.restype = w.HWND
u.GetCursorPos.argtypes = [ctypes.POINTER(w.POINT)]
u.GetCursorPos.restype = w.BOOL
u.SetCursorPos.argtypes = [ctypes.c_int, ctypes.c_int]
u.SetCursorPos.restype = w.BOOL
u.MonitorFromPoint.argtypes = [w.POINT, w.DWORD]
u.MonitorFromPoint.restype = w.HANDLE
u.GetMonitorInfoW.argtypes = [w.HANDLE, ctypes.POINTER(MonitorInfo)]
u.GetMonitorInfoW.restype = w.BOOL
u.GetWindowRect.argtypes = [w.HWND, ctypes.POINTER(w.RECT)]
u.GetWindowRect.restype = w.BOOL
u.IsWindowVisible.argtypes = [w.HWND]
u.IsWindowVisible.restype = w.BOOL
u.GetDpiForWindow.argtypes = [w.HWND]
u.GetDpiForWindow.restype = w.UINT
u.GetDlgItem.argtypes = [w.HWND, ctypes.c_int]
u.GetDlgItem.restype = w.HWND
u.GetWindowTextW.argtypes = [w.HWND, w.LPWSTR, ctypes.c_int]
u.GetWindowTextW.restype = ctypes.c_int
u.SendMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
u.SendMessageW.restype = w.LPARAM
u.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
u.PostMessageW.restype = w.BOOL
u.SendInput.argtypes = [w.UINT, ctypes.POINTER(Input), ctypes.c_int]
u.SendInput.restype = w.UINT
assert not u.FindWindowW('CopilotReel.Controller', 'Copilot Reel'), \
    'Another instance is resident; stop it before this isolated test.'
start = w.POINT()
assert u.GetCursorPos(ctypes.byref(start))
monitor = u.MonitorFromPoint(start, 2)
info = MonitorInfo(cbSize=ctypes.sizeof(MonitorInfo))
assert u.GetMonitorInfoW(monitor, ctypes.byref(info))
work = info.rcWork
center = ((work.left + work.right) // 2, (work.top + work.bottom) // 2)
left, top, right, bottom = work.left + 2, work.top + 2, work.right - 3, work.bottom - 3
locations = {
    'left': (left, center[1]), 'right': (right, center[1]),
    'top': (center[0], top), 'bottom': (center[0], bottom),
    'top-left': (left, top), 'top-right': (right, top),
    'bottom-left': (left, bottom), 'bottom-right': (right, bottom),
}
opening_pattern = re.compile(
    r'Hold started; pointer=(-?\d+),(-?\d+); window=(-?\d+),(-?\d+),(-?\d+),(-?\d+); awaitingCenter=(true|false)')
results = []


def wait(predicate, description, timeout=5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(.03)
    raise AssertionError('Timed out waiting for ' + description)


def read_log():
    return log.read_text(encoding='utf-8') if log.exists() else ''


def launch_count():
    return marker.read_text(encoding='utf-8').count('launched') if marker.exists() else 0


def preferences_text():
    content = (data / 'preferences.ini').read_bytes()
    return content.decode('utf-16' if content.startswith(b'\xff\xfe') else 'utf-8-sig')


def command(argument):
    subprocess.run([str(exe), argument], env=env, creationflags=flags, timeout=8, check=True)


def cursor():
    point = w.POINT()
    assert u.GetCursorPos(ctypes.byref(point))
    return point.x, point.y


def move(point):
    assert u.SetCursorPos(round(point[0]), round(point[1])), ctypes.get_last_error()
    # SetCursorPos alone does not guarantee a Raw Input event. A zero-distance
    # native mouse input preserves the exact physical position and exercises the
    # same input delivery path as a real mouse while the reel is nonactivating.
    event = Input(type=0, payload=InputPayload(mouse=MouseInput(dwFlags=0x0001)))
    assert u.SendInput(1, ctypes.byref(event), ctypes.sizeof(Input)) == 1, ctypes.get_last_error()
    time.sleep(.12)


def open_at(point, hwnd):
    move(point)
    previous_length = len(read_log())
    command('--hold-start')
    match = wait(lambda: opening_pattern.search(read_log()[previous_length:]), 'edge placement log')
    wait(lambda: u.IsWindowVisible(hwnd), 'visible edge reel')
    pointer_x, pointer_y, l, t, r, b = map(int, match.groups()[:6])
    assert (pointer_x, pointer_y) == point, 'The captured pointer changed unexpectedly.'
    assert cursor() == point, 'Opening the reel must not warp the pointer.'
    assert work.left <= l < r <= work.right and work.top <= t < b <= work.bottom, \
        'The complete reel escaped the monitor work area.'
    actual = w.RECT()
    assert u.GetWindowRect(hwnd, ctypes.byref(actual))
    assert (actual.left, actual.top, actual.right, actual.bottom) == (l, t, r, b)
    size = r - l
    assert b - t == size
    expected = (max(work.left, min(point[0] - size // 2, work.right - size)),
                max(work.top, min(point[1] - size // 2, work.bottom - size)))
    assert (l, t) == expected, 'Placement changed more than necessary.'
    assert match.group(7) == 'true', 'An edge gesture must start behind the center-entry guard.'
    return (l, t, r, b), ((l + r) / 2, (t + b) / 2), u.GetDpiForWindow(hwnd)


def release(hwnd, expected_count):
    command('--hold-stop')
    wait(lambda: not u.IsWindowVisible(hwnd), 'hidden reel after release')
    if expected_count:
        wait(lambda: launch_count() >= expected_count, 'expected harmless probe launch')
    time.sleep(.15)
    assert launch_count() == expected_count, 'Release dispatched an unexpected number of launches.'


def test_osd(expected_count):
    command('--settings')
    window = wait(lambda: u.FindWindowW('CopilotReel.Osd', None), 'native OSD window')

    def control(identifier):
        return u.GetDlgItem(window, identifier)

    def visible(identifier):
        child = control(identifier)
        return child if child and u.IsWindowVisible(child) else None

    def click(identifier):
        child = wait(lambda: visible(identifier), f'OSD button {identifier}')
        assert u.PostMessageW(child, 0x00F5, 0, 0)

    def title():
        buffer = ctypes.create_unicode_buffer(512)
        u.GetWindowTextW(window, buffer, len(buffer))
        return buffer.value

    def switch_language(index):
        child = wait(lambda: visible(119), 'native language control')
        u.SendMessageW(child, 0x014E, index, 0)
        assert u.PostMessageW(window, 0x0111, 119 | (1 << 16), child)

    def edit(identifier, value):
        child = wait(lambda: visible(identifier), f'native edit control {identifier}')
        text = ctypes.create_unicode_buffer(value)
        # WM_SETTEXT is marshaled across process boundaries and runs the native
        # edit procedure, including EN_CHANGE. SetWindowTextW is insufficient
        # for another process's child-control behavior.
        assert u.SendMessageW(child, 0x000C, 0, ctypes.cast(text, ctypes.c_void_p).value)

    wait(lambda: 'Quick menu' in title(), 'English OSD home')
    assert not visible(119) and not visible(116), 'Language and technical settings must stay off the home page.'
    click(104)
    wait(lambda: 'Settings' in title() and visible(119), 'second-level Settings page')
    assert u.SendMessageW(control(119), 0x0147, 0, 0) == 0, 'A clean profile must start in English.'
    switch_language(1)
    wait(lambda: '\u8bbe\u7f6e' in title(), 'Chinese Settings title')
    assert 'zh-CN' in preferences_text()
    switch_language(0)
    wait(lambda: 'Settings' in title(), 'English Settings title')
    assert 'en-US' in preferences_text()
    click(100)
    wait(lambda: visible(102), 'OSD home after Back')
    click(102)
    wait(lambda: 'Apps' in title() and visible(105), 'second-level Apps page')
    assert u.SendMessageW(control(105), 0x018B, 0, 0) == 6, 'All six isolated probe targets must be listed.'
    u.SendMessageW(control(105), 0x0186, 0, 0)
    click(107)
    wait(lambda: visible(112) and visible(113), 'app editor')
    assert not visible(116) and not visible(117) and not visible(118), \
        'Technical launch fields must stay in the Advanced page.'
    click(115)
    wait(lambda: 'Advanced' in title() and visible(116) and visible(117) and visible(118), 'Advanced page')
    click(122)
    wait(lambda: visible(112) and visible(113), 'app editor after Advanced Done')
    edit(112, 'Renamed probe')
    click(111)
    wait(lambda: visible(105), 'Apps page after saving a renamed entry')
    assert 'Name=Renamed probe' in (data / 'apps.ini').read_text(encoding='utf-16')
    assert u.SendMessageW(control(105), 0x018B, 0, 0) == 6
    click(106)
    wait(lambda: visible(112) and visible(113), 'new app editor')
    edit(112, 'Temporary probe')
    edit(113, str(probe))
    click(111)
    wait(lambda: visible(105), 'Apps page after adding an entry')
    assert u.SendMessageW(control(105), 0x018B, 0, 0) == 7
    stored = (data / 'apps.ini').read_text(encoding='utf-16')
    assert 'Count=7' in stored and 'Name=Temporary probe' in stored
    click(100)
    wait(lambda: visible(102), 'OSD home after Apps Back')
    click(101)
    wait(lambda: not u.IsWindowVisible(window), 'closed OSD')
    assert launch_count() == expected_count, 'OSD navigation must not launch an application.'
    return ['second-level Settings and Apps pages', 'English default and native bilingual switch',
            'advanced launch fields separated from the app editor', 'rename and add persist through native controls',
            'back navigation without configuration loss']


p = subprocess.Popen([str(exe), '--background'], env=env, creationflags=flags)
try:
    hwnd = wait(lambda: u.FindWindowW('CopilotReel.Controller', 'Copilot Reel'), 'controller window')
    wait(lambda: 'Ready;' in read_log(), 'resident readiness')
    assert 'Configuration validation failed' not in read_log(), 'The isolated probe configuration was rejected.'
    expected_count = 0
    for name, point in locations.items():
        bounds, visual_center, dpi = open_at(point, hwnd)
        release(hwnd, expected_count)
        # Crossing a visible app outside the center must not arm an edge gesture.
        bounds, visual_center, dpi = open_at(point, hwnd)
        dx, dy = point[0] - visual_center[0], point[1] - visual_center[1]
        distance = math.hypot(dx, dy)
        radius = 112 * dpi / 96
        before_center = (visual_center[0] + radius * dx / distance,
                         visual_center[1] + radius * dy / distance)
        move(before_center)
        release(hwnd, expected_count)
        # The same path can arm only after an explicit center entry.
        bounds, visual_center, dpi = open_at(point, hwnd)
        move(before_center)
        before_entry = len(read_log())
        move(visual_center)
        wait(lambda: 'Center entered; direction selection armed' in read_log()[before_entry:],
             'explicit center entry before selection')
        move((visual_center[0], visual_center[1] - 112 * dpi / 96))
        expected_count += 1
        release(hwnd, expected_count)
        command('--hold-stop')
        time.sleep(.12)
        assert launch_count() == expected_count, 'Duplicate release launched twice.'
        results.append({'position': name, 'pointer': list(point), 'window': list(bounds),
                        'noWarp': True, 'immediateReleaseCanceled': True,
                        'preCenterReleaseCanceled': True, 'centerThenSelectionLaunchedOnce': True})
    # Keep completed edge evidence even when a later OSD assertion fails.
    edge_result = {'edgesPassed': True, 'workArea': [work.left, work.top, work.right, work.bottom],
                   'cases': results, 'probeLaunches': expected_count, 'osdStatus': 'pending'}
    (data / 'results.json').write_text(json.dumps(edge_result, indent=2), encoding='utf-8')
    osd_results = test_osd(expected_count)
    command('--quit')
    assert p.wait(timeout=5) == 0
    result = {'passed': True, 'workArea': [work.left, work.top, work.right, work.bottom],
              'cases': results, 'probeLaunches': expected_count, 'osd': osd_results,
              'note': 'Small-work-area fallback requires a suitable real display or placement unit tests.'}
    (data / 'results.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result))
finally:
    try:
        if p.poll() is None:
            command('--quit')
            p.wait(timeout=5)
    finally:
        u.SetCursorPos(start.x, start.y)
