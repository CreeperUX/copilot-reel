"""Capture only the application's own HWND using Windows Graphics Capture.

Run exclusively, with no resident Copilot Reel instance. The fixture never
launches a preset and always restores the cursor and foreground window.
"""
import ctypes
from ctypes import wintypes as w
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'build/Release/CopilotReel.exe'
CAPTURE = ROOT / 'build/Release/capture-window.exe'
DATA = ROOT / 'artifacts/qa/visual-v0.3.1'
OUTPUT = ROOT / 'output/runtime/v0.3.1'
DATA.mkdir(parents=True, exist_ok=True)
OUTPUT.mkdir(parents=True, exist_ok=True)
u = ctypes.WinDLL('user32', use_last_error=True)
u.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
u.FindWindowW.argtypes = [w.LPCWSTR, w.LPCWSTR]
u.FindWindowW.restype = w.HWND
u.IsWindowVisible.argtypes = [w.HWND]
u.GetForegroundWindow.restype = w.HWND
u.SetForegroundWindow.argtypes = [w.HWND]
assert not u.FindWindowW('CopilotReel.Controller', 'Copilot Reel'), 'Close the resident application before this isolated fixture.'
assert CAPTURE.exists(), 'Run tools/build-capture-tool.ps1 first.'
original_pointer = w.POINT()
u.GetCursorPos(ctypes.byref(original_pointer))
original_foreground = u.GetForegroundWindow()
names = ['ChatGPT', 'Claude Desktop', 'DeepSeek Harness (DSH)', 'Antigravity', 'ZCode', 'Visual Studio Code']
content = '[CopilotReel]\r\nVersion=1\r\nCount=6\r\n'
for index, name in enumerate(names):
    content += f'[App{index}]\r\nName={name}\r\nKind=shell\r\nTarget=notepad.exe\r\nArguments=\r\nWorkingDirectory=\r\n'
(DATA / 'apps.ini').write_text(content, encoding='utf-16', newline='')
(DATA / 'preferences.ini').write_text('[General]\r\nLanguage=en-US\r\n', encoding='utf-16', newline='')
env = os.environ.copy()
env['COPILOT_REEL_DATA_DIR'] = str(DATA)
process = None


def command(value):
    subprocess.run([str(EXE), value], env=env, creationflags=0x08000000, check=True, timeout=8)


def wait(predicate):
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(.025)
    raise AssertionError('The native application did not reach the expected state.')


try:
    u.SetCursorPos(u.GetSystemMetrics(0) // 2, u.GetSystemMetrics(1) // 2)
    process = subprocess.Popen([str(EXE), '--background'], env=env, creationflags=0x08000000)
    wait(lambda: u.FindWindowW('CopilotReel.Controller', 'Copilot Reel'))
    hwnd = u.FindWindowW('CopilotReel.Controller', 'Copilot Reel')
    evidence = {'captureMethod': 'Windows.Graphics.Capture.CreateForWindow', 'hwnd': hwnd, 'application': str(EXE), 'fixtures': []}
    for mode, name in [('--preview', 'reel'), ('--settings', 'settings')]:
        command(mode)
        wait(lambda: u.IsWindowVisible(hwnd))
        time.sleep(.5)
        command('--diagnostics')
        diagnostics = (DATA / 'diagnostics.txt').read_text(encoding='utf-8')
        assert 'last error=' not in diagnostics, diagnostics
        subprocess.run([str(CAPTURE), str(hwnd), str(OUTPUT / f'{name}.png')],
                       creationflags=0x08000000, check=True, timeout=10)
        evidence['fixtures'].append({'name': name, 'command': mode, 'diagnostics': diagnostics})
        command('--cancel')
        wait(lambda: not u.IsWindowVisible(hwnd))
    command('--quit')
    assert process.wait(timeout=8) == 0
    (DATA / 'capture-evidence.json').write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
    print(OUTPUT)
finally:
    if process is not None and process.poll() is None:
        command('--quit')
        process.wait(timeout=8)
    u.SetCursorPos(original_pointer.x, original_pointer.y)
    if original_foreground:
        u.SetForegroundWindow(original_foreground)
