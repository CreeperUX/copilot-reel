"""Verify second-level OSD language switching and persistence after restart.

Uses an isolated preferences directory and only native control messages. Does not
move the pointer or change the user's normal profile. Requires no other resident
Copilot Reel controller.
"""
import ctypes
from ctypes import wintypes as w
import json
import os
import subprocess
import time
from pathlib import Path


root = Path(__file__).resolve().parents[1]
exe = root / 'build/Release/CopilotReel.exe'
data = root / 'artifacts/qa/localization'
data.mkdir(parents=True, exist_ok=True)
preferences = data / 'preferences.ini'
env = os.environ.copy()
env['COPILOT_REEL_DATA_DIR'] = str(data)
flags = 0x08000000
u = ctypes.WinDLL('user32', use_last_error=True)
u.FindWindowW.argtypes = [w.LPCWSTR, w.LPCWSTR]
u.FindWindowW.restype = w.HWND
u.GetDlgItem.argtypes = [w.HWND, ctypes.c_int]
u.GetDlgItem.restype = w.HWND
u.GetWindowTextW.argtypes = [w.HWND, w.LPWSTR, ctypes.c_int]
u.GetWindowTextW.restype = ctypes.c_int
u.SendMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
u.SendMessageW.restype = w.LPARAM
u.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
u.PostMessageW.restype = w.BOOL
u.IsWindowVisible.argtypes = [w.HWND]
u.IsWindowVisible.restype = w.BOOL
assert not u.FindWindowW('CopilotReel.Controller', 'Copilot Reel'), 'Exit the resident instance first.'
if preferences.exists():
    preferences.unlink()


def wait(predicate, description, timeout=5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(.03)
    raise AssertionError('Timed out waiting for ' + description)


def title(hwnd):
    text = ctypes.create_unicode_buffer(512)
    u.GetWindowTextW(hwnd, text, len(text))
    return text.value


def click(hwnd, identifier):
    child = u.GetDlgItem(hwnd, identifier)
    assert child and u.IsWindowVisible(child), f'Missing visible native button {identifier}'
    assert u.PostMessageW(child, 0x00F5, 0, 0)


def settings(expected_language):
    hwnd = wait(lambda: u.FindWindowW('CopilotReel.Osd', None), 'OSD window')
    wait(lambda: u.IsWindowVisible(hwnd) and u.GetDlgItem(hwnd, 104), 'OSD home')
    home_title = 'Quick menu' if expected_language == 0 else '\u5feb\u6377\u83dc\u5355'
    assert home_title in title(hwnd), title(hwnd)
    assert not u.GetDlgItem(hwnd, 119), 'Language must be inside second-level Settings.'
    click(hwnd, 104)
    child = wait(lambda: u.GetDlgItem(hwnd, 119), 'native language control')
    assert u.SendMessageW(child, 0x0147, 0, 0) == expected_language
    return hwnd


def switch(hwnd, index):
    child = u.GetDlgItem(hwnd, 119)
    assert child
    u.SendMessageW(child, 0x014E, index, 0)
    assert u.PostMessageW(hwnd, 0x0111, 119 | (1 << 16), child)
    expected_title = 'Settings' if index == 0 else '\u8bbe\u7f6e'
    wait(lambda: expected_title in title(hwnd), 'localized Settings title')
    content = preferences.read_bytes()
    stored = content.decode('utf-16' if content.startswith(b'\xff\xfe') else 'utf-8-sig')
    assert ('en-US' if index == 0 else 'zh-CN') in stored


def stop(process, hwnd):
    click(hwnd, 101)
    wait(lambda: not u.IsWindowVisible(hwnd), 'closed OSD')
    subprocess.run([str(exe), '--quit'], env=env, creationflags=flags, timeout=8, check=True)
    assert process.wait(timeout=5) == 0


p = None
try:
    p = subprocess.Popen([str(exe), '--settings'], env=env, creationflags=flags)
    h = settings(0)
    switch(h, 1)
    stop(p, h)
    p = None
    p = subprocess.Popen([str(exe), '--settings'], env=env, creationflags=flags)
    h = settings(1)
    switch(h, 0)
    stop(p, h)
    p = None
    result = {'passed': ['English on first launch', 'language in second-level Settings',
                         'native switch to Simplified Chinese', 'preference persistence after restart',
                         'native switch back to English'], 'isolated': True}
    (data / 'results.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result))
finally:
    if p is not None and p.poll() is None:
        subprocess.run([str(exe), '--quit'], env=env, creationflags=flags, timeout=8, check=True)
        p.wait(timeout=5)
