"""Capture the 0.3.4 reel natively: still states per visual variant, and motion frame strips.

Run exclusively, with no resident Copilot Reel instance and without touching the desktop:
    uv run --python 3.14 --with pillow python tools/capture-v034.py

Each variant restarts the application in a fresh isolated data directory with
COPILOT_REEL_VISUAL_OVERRIDE (theme, material, contrast, DPI). Only the harmless
launch probe is ever launched. The pointer and foreground window are restored.
Windows Graphics Capture records the application HWND only; DWM host-backdrop content
may be absent from frosted captures, which the evidence file records.
"""
import ctypes
from ctypes import wintypes as w
import csv
import json
import math
import os
from pathlib import Path
import re
import subprocess
import time
import uuid

from PIL import Image, ImageDraw, ImageFont


class MouseInput(ctypes.Structure):
    _fields_ = [("dx", w.LONG), ("dy", w.LONG), ("mouseData", w.DWORD),
                ("dwFlags", w.DWORD), ("time", w.DWORD), ("dwExtraInfo", w.WPARAM)]


class KeyboardInput(ctypes.Structure):
    _fields_ = [("wVk", w.WORD), ("wScan", w.WORD), ("dwFlags", w.DWORD),
                ("time", w.DWORD), ("dwExtraInfo", w.WPARAM)]


class InputPayload(ctypes.Union):
    _fields_ = [("mouse", MouseInput), ("keyboard", KeyboardInput)]


class Input(ctypes.Structure):
    _fields_ = [("type", w.DWORD), ("payload", InputPayload)]


class MonitorInfo(ctypes.Structure):
    _fields_ = [("cbSize", w.DWORD), ("rcMonitor", w.RECT), ("rcWork", w.RECT), ("dwFlags", w.DWORD)]


ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'build/Release/CopilotReel.exe'
PROBE = ROOT / 'build/Release/launch_probe.exe'
CAPTURE = ROOT / 'build/Release/capture-window.exe'
OUTPUT = ROOT / 'output/runtime/v0.3.4'
QA = ROOT / 'artifacts/qa' / ('visual-v0.3.4-' + time.strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex[:6])
OUTPUT.mkdir(parents=True, exist_ok=True)
QA.mkdir(parents=True)

u = ctypes.WinDLL('user32', use_last_error=True)
u.SetProcessDpiAwarenessContext.argtypes = [ctypes.c_void_p]
u.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
u.FindWindowW.argtypes = [w.LPCWSTR, w.LPCWSTR]
u.FindWindowW.restype = w.HWND
u.IsWindowVisible.argtypes = [w.HWND]
u.GetCursorPos.argtypes = [ctypes.POINTER(w.POINT)]
u.GetWindowRect.argtypes = [w.HWND, ctypes.POINTER(w.RECT)]
u.GetWindowThreadProcessId.argtypes = [w.HWND, ctypes.POINTER(w.DWORD)]
u.GetForegroundWindow.restype = w.HWND
u.SetForegroundWindow.argtypes = [w.HWND]
u.SendInput.argtypes = [w.UINT, ctypes.POINTER(Input), ctypes.c_int]
u.SendInput.restype = w.UINT
u.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
u.MonitorFromPoint.argtypes = [w.POINT, w.DWORD]
u.MonitorFromPoint.restype = w.HANDLE
u.GetMonitorInfoW.argtypes = [w.HANDLE, ctypes.POINTER(MonitorInfo)]
assert not u.FindWindowW('CopilotReel.Controller', 'Copilot Reel'), 'Close the resident application before this isolated fixture.'
assert EXE.exists() and PROBE.exists() and CAPTURE.exists(), 'Run tools/build.ps1 and tools/build-capture-tool.ps1 first.'
original_pointer = w.POINT()
u.GetCursorPos(ctypes.byref(original_pointer))
original_foreground = u.GetForegroundWindow()
monitor = MonitorInfo(cbSize=ctypes.sizeof(MonitorInfo))
assert u.GetMonitorInfoW(u.MonitorFromPoint(original_pointer, 2), ctypes.byref(monitor))
work = monitor.rcWork
anchor = ((work.left + work.right) // 2, (work.top + work.bottom) // 2)

# Main shows seven apps and the +: vendor identities by URL host (never launched),
# system apps by target, and the probe, which is the only commit target.
PROFILES = [('ChatGPT', 'https://chatgpt.com', ''), ('Claude', 'https://claude.ai', ''),
            ('Gemini', 'https://gemini.google.com', ''), ('Perplexity', 'https://www.perplexity.ai', ''),
            ('Notepad', 'notepad.exe', ''), ('Windows Settings', 'ms-settings:', ''),
            ('Probe', str(PROBE), '--radial-probe=capture')]
VARIANTS = [('dark-solid', 'theme=dark;material=solid'), ('light-solid', 'theme=light;material=solid'),
            ('dark-frosted', 'theme=dark;material=frosted'), ('light-frosted', 'theme=light;material=frosted'),
            ('contrast', 'contrast=on'), ('dark-100', 'theme=dark;material=solid;dpi=96'),
            ('dark-150', 'theme=dark;material=solid;dpi=144')]
evidence = {'captureMethod': 'Windows.Graphics.Capture.CreateForWindow', 'application': str(EXE),
            'profiles': [p[0] for p in PROFILES], 'variants': [], 'motion': [],
            'limits': ['Captures contain only the application HWND; frosted host-backdrop content and the desktop '
                       'beneath are not part of the image.',
                       'contrast=on applies the high-contrast code path with the current system colors; it does not '
                       'switch Windows to a high-contrast theme.',
                       'Frame strips are visual evidence, not frame-rate measurements.']}
process = None
hwnd = None
data = None


def encoded(value):
    return value.replace('\\', '\\\\').replace('\t', '\\t')


def prepare(name, override):
    directory = QA / name
    directory.mkdir()
    content = f'[CopilotReel]\r\nVersion=1\r\nCount={len(PROFILES)}\r\n'
    for index, (title, target, arguments) in enumerate(PROFILES):
        content += (f'[App{index}]\r\nName={title}\r\nKind=shell\r\nTarget={encoded(target)}\r\n'
                    f'Arguments={arguments}\r\nWorkingDirectory=\r\n')
    (directory / 'apps.ini').write_text(content, encoding='utf-16', newline='')
    (directory / 'preferences.ini').write_text('[General]\r\nLanguage=en-US\r\n', encoding='utf-16', newline='')
    environment = os.environ.copy()
    environment['COPILOT_REEL_DATA_DIR'] = str(directory)
    environment['COPILOT_REEL_VISUAL_OVERRIDE'] = override
    return directory, environment


def command(environment, value):
    subprocess.run([str(EXE), value], env=environment, creationflags=0x08000000, check=True, timeout=8)


def wait(predicate, description='the expected state', timeout=8):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result = predicate()
        if result:
            return result
        time.sleep(.02)
    raise AssertionError('Timed out waiting for ' + description)


def send(events):
    array = (Input * len(events))(*events)
    assert u.SendInput(len(events), array, ctypes.sizeof(Input)) == len(events), ctypes.get_last_error()


def move(x, y, settle=.08):
    assert u.SetCursorPos(round(x), round(y))
    send([Input(type=0, payload=InputPayload(mouse=MouseInput(dwFlags=0x0001)))])
    time.sleep(settle)


def click(x, y):
    move(x, y, .03)
    send([Input(type=0, payload=InputPayload(mouse=MouseInput(dwFlags=flag))) for flag in (0x0002, 0x0004)])


def key(code):
    wait(lambda: u.GetForegroundWindow() == hwnd, 'keyboard focus')
    send([Input(type=1, payload=InputPayload(keyboard=KeyboardInput(wVk=code, dwFlags=flag))) for flag in (0, 2)])


def geometry():
    bounds = w.RECT()
    assert u.GetWindowRect(hwnd, ctypes.byref(bounds))
    return (bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2, (bounds.right - bounds.left) / 384


def slot(index, count, radius=112):
    x, y, scale = geometry()
    angle = index * 2 * math.pi / count
    return x + math.sin(angle) * radius * scale, y - math.cos(angle) * radius * scale


def diagnostics(environment, directory):
    path = directory / 'diagnostics.txt'
    if path.exists():
        path.unlink()
    command(environment, '--diagnostics')
    wait(lambda: path.exists() and path.stat().st_size > 0, 'diagnostics')
    text = path.read_text(encoding='utf-8')
    assert 'last error=' not in text, text
    return text, dict(re.findall(r'^([^=\r\n]+)=([^\r\n]*)', text, re.MULTILINE))


def launch(environment):
    global process, hwnd
    process = subprocess.Popen([str(EXE), '--background'], env=environment, creationflags=0x08000000)
    hwnd = wait(lambda: u.FindWindowW('CopilotReel.Controller', 'Copilot Reel'), 'controller creation')
    owner = w.DWORD()
    u.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
    assert owner.value == process.pid, 'Only this fixture-owned application may be captured.'


def quit_app(environment):
    global process
    if process is not None and process.poll() is None:
        command(environment, '--quit')
        process.wait(timeout=8)
    process = None


def still(environment, directory, variant, name, label):
    text, values = diagnostics(environment, directory)
    assert values.get('centerLabel') == label, (variant, name, label, values.get('centerLabel'))
    path = OUTPUT / f'{variant}-{name}.png'
    subprocess.run([str(CAPTURE), str(hwnd), str(path)], creationflags=0x08000000, check=True, timeout=10)
    return {'name': name, 'file': str(path.relative_to(ROOT)), 'centerLabel': label,
            'material': re.search(r'; material=([^;\r\n]*)', text).group(1),
            'dpi': int(re.search(r'; DPI=(\d+)', text).group(1))}


def capture_variant(variant, override):
    directory, environment = prepare(variant, override)
    launch(environment)
    shots = []
    move(*anchor)
    command(environment, '--preview')
    wait(lambda: u.IsWindowVisible(hwnd), 'preview')
    time.sleep(.35)
    shots.append(still(environment, directory, variant, 'main', ''))
    move(*slot(1, 8))
    time.sleep(.35)
    shots.append(still(environment, directory, variant, 'main-claude', 'Claude'))
    command(environment, '--cancel')
    wait(lambda: not u.IsWindowVisible(hwnd), 'hide')
    move(*anchor)
    command(environment, '--settings')
    wait(lambda: u.IsWindowVisible(hwnd), 'hub')
    time.sleep(.35)
    shots.append(still(environment, directory, variant, 'hub', ''))
    move(*slot(1, 5))
    time.sleep(.35)
    shots.append(still(environment, directory, variant, 'hub-appearance', 'Appearance'))
    move(*slot(0, 5))
    time.sleep(.35)
    shots.append(still(environment, directory, variant, 'hub-language', 'Language'))
    command(environment, '--cancel')
    wait(lambda: not u.IsWindowVisible(hwnd), 'hide')
    # Edge gating: a hold near the top-left corner clamps the plate away from the pointer.
    move(work.left + 24, work.top + 24)
    command(environment, '--hold-start')
    wait(lambda: u.IsWindowVisible(hwnd), 'gated hold')
    time.sleep(.35)
    shots.append(still(environment, directory, variant, 'gated', ''))
    command(environment, '--cancel')
    wait(lambda: not u.IsWindowVisible(hwnd), 'hide')
    quit_app(environment)
    evidence['variants'].append({'variant': variant, 'override': override, 'shots': shots})
    return [OUTPUT / f'{variant}-{shot["name"]}.png' for shot in shots]


def sequence(environment, directory, name, duration, actions):
    """Records frames while `actions` (offset seconds, callable) run after capture is ready."""
    frames = directory / ('frames-' + name)
    capture = subprocess.Popen([str(CAPTURE), str(hwnd), str(frames), '--sequence', str(duration)],
                               creationflags=0x08000000)
    wait(lambda: (frames / 'ready.txt').exists(), 'capture readiness')
    began = time.perf_counter()
    events = []
    for offset, label, action in actions:
        delay = offset - (time.perf_counter() - began)
        if delay > 0:
            time.sleep(delay)
        events.append({'event': label, 'elapsedMs': round((time.perf_counter() - began) * 1000, 1)})
        action()
    assert capture.wait(timeout=duration / 1000 + 15) == 0
    with open(frames / 'frames.csv', newline='') as handle:
        rows = list(csv.DictReader(handle))
    return frames, rows, events


def sheet(name, frames, rows, events, background, columns=8, cell=240):
    images = [(Image.open(frames / f'frame-{row["frame"]}.png').convert('RGBA'), float(row['captureMilliseconds']))
              for row in rows]
    # Drop consecutive identical frames so the strip shows change, not idle time.
    kept = []
    for image, stamp in images:
        if not kept or image.tobytes() != kept[-1][0].tobytes():
            kept.append((image, stamp))
    kept = kept[:48]
    rows_count = (len(kept) + columns - 1) // columns
    canvas = Image.new('RGB', (columns * cell, rows_count * (cell + 24) + 40), background)
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 15)
    ink = (20, 20, 20) if sum(background) > 380 else (230, 230, 230)
    draw.text((8, 10), name + '  ·  ' + '  ·  '.join(f'{e["event"]} @{e["elapsedMs"]:.0f} ms' for e in events), fill=ink, font=font)
    for index, (image, stamp) in enumerate(kept):
        x, y = (index % columns) * cell, 40 + (index // columns) * (cell + 24)
        thumb = image.resize((cell - 8, cell - 8), Image.LANCZOS)
        canvas.paste(thumb, (x + 4, y), thumb)
        draw.text((x + 8, y + cell - 6), f'{stamp:.1f} ms', fill=ink, font=font)
    path = OUTPUT / f'motion-{name}.png'
    canvas.save(path)
    return path, len(kept)


def capture_motion():
    directory, environment = prepare('motion', 'theme=dark;material=solid')
    launch(environment)
    move(*anchor)
    command(environment, '--preview')
    wait(lambda: u.IsWindowVisible(hwnd), 'preview')
    time.sleep(.4)
    move(*anchor)

    def reopen():
        command(environment, '--preview')
    # Open (after a cancel), selection appear and travel along the orbit.
    frames, rows, events = sequence(environment, directory, 'open-select', 1400, [
        (.05, 'cancel', lambda: u.PostMessageW(hwnd, 0x0010, 0, 0)),
        (.30, 'open', reopen),
        (.75, 'hover Claude', lambda: move(*slot(1, 8), 0)),
        (1.05, 'hover Gemini', lambda: move(*slot(2, 8), 0))])
    evidence['motion'].append({'name': 'open-select', 'events': events, 'frames': len(rows),
                               'sheet': str(sheet('open-select', frames, rows, events, (32, 32, 32))[0].relative_to(ROOT))})
    # Drill into the hub from the +, with the handoff to the back chevron, then back out.
    frames, rows, events = sequence(environment, directory, 'drill', 1300, [
        (.05, 'hover +', lambda: move(*slot(7, 8), 0)),
        (.30, 'click + (drill in)', lambda: click(*slot(7, 8))),
        (.85, 'Esc (drill out)', lambda: key(0x1b))])
    evidence['motion'].append({'name': 'drill', 'events': events, 'frames': len(rows),
                               'sheet': str(sheet('drill', frames, rows, events, (32, 32, 32))[0].relative_to(ROOT))})
    # Commit on the probe: the chosen tile holds while the rest leaves.
    marker = directory / 'launch-probe.txt'
    frames, rows, events = sequence(environment, directory, 'commit', 700, [
        (.05, 'hover Probe', lambda: move(*slot(6, 8), 0)),
        (.30, 'click Probe (commit)', lambda: click(*slot(6, 8)))])
    evidence['motion'].append({'name': 'commit', 'events': events, 'frames': len(rows),
                               'sheet': str(sheet('commit', frames, rows, events, (32, 32, 32))[0].relative_to(ROOT))})
    wait(lambda: marker.exists() and 'launched' in marker.read_text(encoding='utf-8'), 'probe launch')
    assert marker.read_text(encoding='utf-8').count('launched') == 1, 'The commit must launch exactly once.'
    quit_app(environment)


def contact(variant, paths, background):
    images = [Image.open(path).convert('RGBA') for path in paths]
    cell = 384
    canvas = Image.new('RGB', (cell * len(images), cell + 30), background)
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 15)
    ink = (20, 20, 20) if sum(background) > 380 else (230, 230, 230)
    for index, (image, path) in enumerate(zip(images, paths)):
        thumb = image.resize((cell, cell), Image.LANCZOS)
        canvas.paste(thumb, (index * cell, 0), thumb)
        draw.text((index * cell + 8, cell + 6), path.stem, fill=ink, font=font)
    target = OUTPUT / f'sheet-{variant}.png'
    canvas.save(target)
    return target


try:
    for variant, override in VARIANTS:
        paths = capture_variant(variant, override)
        background = (243, 243, 243) if 'light' in variant else (32, 32, 32)
        evidence['variants'][-1]['sheet'] = str(contact(variant, paths, background).relative_to(ROOT))
    capture_motion()
    (QA / 'capture-evidence.json').write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
    print(QA / 'capture-evidence.json')
finally:
    if process is not None and process.poll() is None:
        process.kill()
    u.SetCursorPos(original_pointer.x, original_pointer.y)
    if original_foreground:
        u.SetForegroundWindow(original_foreground)
