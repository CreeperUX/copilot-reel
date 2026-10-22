"""Capture the reel's still states through the interaction test seam and compare them with a reference set.

The application runs as an isolated, unpackaged test instance (tests/reel_seam.py): it never moves the
pointer, sends input, takes the foreground or touches the installed instance. Each variant reproduces
the still set of tools/capture-v034.py (Main, a selected vendor tile, the hub, two hub selections and an
edge-gated hold) and, from 0.3.5, the actions page of an app with its reorder chevrons: in the middle of
the reel, and at the first and the last position, where one chevron is blocked. The reorder happens in
the variant's own isolated data directory and is undone before the gated capture. Every image that also
exists in the reference folder is compared pixel by pixel.

The run fails (exit status 1, "passed": false in results.json) when a compared image differs or has
another size, when a state that existed in 0.3.4 has no reference image, or when a variant cannot be
captured. New 0.3.5 states without a reference are recorded as new.

    uv run --python 3.14 --with pillow python tools/capture-seam.py [--reference output/runtime/v0.3.4]
        [--output output/runtime/v0.3.5] [--variants dark-solid,light-solid,...]
"""
import argparse
import json
import math
import subprocess
import sys
import time
import uuid
from pathlib import Path

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
import reel_seam as seam  # noqa: E402

EXE = ROOT / 'build/Release/CopilotReel.exe'
PROBE = ROOT / 'build/Release/launch_probe.exe'
CAPTURE = ROOT / 'build/Release/capture-window.exe'
# The same profiles as tools/capture-v034.py, so Main is comparable with the 0.3.4 captures.
PROFILES = [('ChatGPT', 'https://chatgpt.com', ''), ('Claude', 'https://claude.ai', ''),
            ('Gemini', 'https://gemini.google.com', ''), ('Perplexity', 'https://www.perplexity.ai', ''),
            ('Notepad', 'notepad.exe', ''), ('Windows Settings', 'ms-settings:', ''),
            ('Probe', str(PROBE), '--radial-probe=capture')]
VARIANTS = [('dark-solid', 'theme=dark;material=solid'), ('light-solid', 'theme=light;material=solid'),
            ('dark-frosted', 'theme=dark;material=frosted'), ('light-frosted', 'theme=light;material=frosted'),
            ('contrast', 'contrast=on'), ('dark-100', 'theme=dark;material=solid;dpi=96'),
            ('dark-150', 'theme=dark;material=solid;dpi=144')]
# States captured by 0.3.4: each must have a reference image.
REFERENCE_STATES = ('main', 'main-claude', 'hub', 'hub-appearance', 'hub-language', 'gated')


def slot(reel, index, count, radius=seam.SECTOR_RADIUS_DIP):
    bounds = seam.w.RECT()
    seam.user32.GetWindowRect(reel.hwnd, seam.ctypes.byref(bounds))
    left, top, right, bottom = bounds.left, bounds.top, bounds.right, bounds.bottom
    scale = (right - left) / seam.CANVAS_DIP
    x, y = (left + right) / 2, (top + bottom) / 2
    angle = index * 2 * math.pi / count
    return round(x + math.sin(angle) * radius * scale), round(y - math.cos(angle) * radius * scale)


def write_profiles(directory):
    content = f'[CopilotReel]\r\nVersion=1\r\nCount={len(PROFILES)}\r\n'
    for index, (title, target, arguments) in enumerate(PROFILES):
        content += (f'[App{index}]\r\nName={title}\r\nKind=shell\r\nTarget={seam.encode_ini(target)}\r\n'
                    f'Arguments={arguments}\r\nWorkingDirectory=\r\n')
    (directory / 'apps.ini').write_text(content, encoding='utf-16', newline='')
    (directory / 'preferences.ini').write_text('[General]\r\nLanguage=en-US\r\n', encoding='utf-16', newline='')


def still(reel, output, variant, name, label):
    reel.sync()
    time.sleep(.35)
    values = reel.diagnostics()
    assert values.get('centerLabel') == label, (variant, name, label, values.get('centerLabel'))
    path = output / f'{variant}-{name}.png'
    subprocess.run([str(CAPTURE), str(reel.hwnd), str(path)], creationflags=seam.CREATE_NO_WINDOW, check=True, timeout=10)
    return name, path


def wait_visible(reel, visible):
    seam.wait(lambda: bool(seam.user32.IsWindowVisible(reel.hwnd)) == visible, 'visible' if visible else 'hidden')


def move_until(reel, vk, adjust):
    """Press A or D on the actions page until diagnostics report this adjust state."""
    for _ in range(len(PROFILES)):
        if reel.diagnostics().get('adjust') == adjust:
            return
        reel.key(vk)
    assert reel.diagnostics().get('adjust') == adjust, (adjust, reel.diagnostics().get('adjust'))


def capture_variant(variant, override, output, data):
    directory = data / variant
    directory.mkdir(parents=True)
    write_profiles(directory)
    work = seam.primary_work_area()
    anchor = ((work[0] + work[2]) // 2, (work[1] + work[3]) // 2)
    reel = seam.Reel(EXE, directory, run_id='cap-' + uuid.uuid4().hex[:12], pointer=anchor, visual=override)
    shots = []
    try:
        reel.start()
        reel.send_command(seam.PREVIEW, anchor)
        wait_visible(reel, True)
        shots.append(still(reel, output, variant, 'main', ''))
        reel.pointer(slot(reel, 1, 8))
        shots.append(still(reel, output, variant, 'main-claude', 'Claude'))
        reel.send_command(seam.CANCEL)
        wait_visible(reel, False)
        reel.pointer(anchor)
        reel.send_command(seam.SETTINGS, anchor)
        wait_visible(reel, True)
        shots.append(still(reel, output, variant, 'hub', ''))
        reel.pointer(slot(reel, 1, 5))
        shots.append(still(reel, output, variant, 'hub-appearance', 'Appearance'))
        reel.pointer(slot(reel, 0, 5))
        shots.append(still(reel, output, variant, 'hub-language', 'Language'))
        # 0.3.5: an app's actions page, the app in slot 0 with both reorder chevrons.
        reel.pointer(slot(reel, 3, 5))
        reel.click(slot(reel, 3, 5))
        names = reel.diagnostics().names
        reel.pointer(slot(reel, names.index('Claude'), len(names)))
        reel.click(slot(reel, names.index('Claude'), len(names)))
        reel.pointer(anchor)
        values = reel.diagnostics()
        assert values.get('adjust') == 'both', values.get('adjust')
        shots.append(still(reel, output, variant, 'entry', ''))
        reel.pointer(slot(reel, 0, len(values.names)))
        shots.append(still(reel, output, variant, 'entry-app', 'Claude'))
        # The first and the last position: one chevron is blocked and dimmed. A/D select slot 0,
        # so the pointer leaves it again for the idle frames.
        order = seam.read_app_names(directory / 'apps.ini')
        move_until(reel, seam.VK_A, 'later')
        assert seam.read_app_names(directory / 'apps.ini')[0] == 'Claude'
        reel.pointer(anchor)
        shots.append(still(reel, output, variant, 'entry-first', ''))
        reel.pointer(slot(reel, 0, len(values.names)))
        shots.append(still(reel, output, variant, 'entry-first-app', 'Claude'))
        move_until(reel, seam.VK_D, 'earlier')
        assert seam.read_app_names(directory / 'apps.ini')[-1] == 'Claude'
        reel.pointer(anchor)
        shots.append(still(reel, output, variant, 'entry-last', ''))
        # Restore the original order so the gated Main matches its reference.
        for _ in range(len(PROFILES)):
            if seam.read_app_names(directory / 'apps.ini') == order:
                break
            reel.key(seam.VK_A)
        assert seam.read_app_names(directory / 'apps.ini') == order, seam.read_app_names(directory / 'apps.ini')
        reel.send_command(seam.CANCEL)
        wait_visible(reel, False)
        corner = (work[0] + 24, work[1] + 24)
        reel.pointer(corner)
        reel.send_command(seam.HOLD_START, corner)
        wait_visible(reel, True)
        shots.append(still(reel, output, variant, 'gated', ''))
        reel.send_command(seam.CANCEL)
        wait_visible(reel, False)
        reel.stop()
    finally:
        reel.shutdown()
    return shots


def compare(path, reference):
    if not reference.exists():
        return None
    a, b = Image.open(path).convert('RGBA'), Image.open(reference).convert('RGBA')
    if a.size != b.size:
        return {'reference': str(reference.relative_to(ROOT)), 'sizeMismatch': [a.size, b.size]}
    box = ImageChops.difference(a, b).getbbox()
    differing = 0
    if box:
        differing = sum(1 for p in ImageChops.difference(a, b).getdata() if p != (0, 0, 0, 0))
    return {'reference': str(reference.relative_to(ROOT)), 'differingPixels': differing, 'box': box}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--reference', default='output/runtime/v0.3.4')
    parser.add_argument('--output', default='output/runtime/v0.3.5')
    parser.add_argument('--variants', default=','.join(v for v, _ in VARIANTS))
    args = parser.parse_args()
    for path in (EXE, PROBE, CAPTURE):
        assert path.exists(), f'{path} is missing; run tools/build.ps1 and tools/build-capture-tool.ps1.'
    seam.assert_no_input_injection(Path(__file__), ROOT / 'tests/reel_seam.py')
    seam.assert_seam_binary(EXE)
    output = (ROOT / args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    reference = (ROOT / args.reference).resolve()
    stamp = time.strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex[:6]
    data = ROOT / 'artifacts/qa' / f'capture-seam-{stamp}'
    data.mkdir(parents=True)
    wanted = [name for name in args.variants.split(',') if name]
    unknown = sorted(set(wanted) - {v for v, _ in VARIANTS})
    if unknown:
        raise SystemExit('Unknown variants: ' + ', '.join(unknown))
    evidence = {'captureMethod': 'Windows.Graphics.Capture.CreateForWindow of a seam test instance',
                'inputInjection': 'none', 'binarySha256': seam.sha256(EXE), 'reference': str(reference),
                'referenceStates': list(REFERENCE_STATES), 'variants': []}
    failures = []
    for variant, override in VARIANTS:
        if variant not in wanted:
            continue
        entry = {'variant': variant, 'override': override, 'shots': []}
        evidence['variants'].append(entry)
        try:
            shots = capture_variant(variant, override, output, data)
        except Exception as error:
            entry['error'] = f'{type(error).__name__}: {error}'
            failures.append(f'{variant}: capture failed: {entry["error"]}')
            print(variant, 'capture failed:', entry['error'])
            continue
        for state, path in shots:
            comparison = compare(path, reference / path.name)
            entry['shots'].append({'state': state, 'file': str(path.relative_to(ROOT)), 'comparison': comparison})
            if comparison is None:
                if state in REFERENCE_STATES:
                    failures.append(f'{path.name}: no reference image for a 0.3.4 state')
            elif 'sizeMismatch' in comparison:
                failures.append(f'{path.name}: size {comparison["sizeMismatch"][0]} != reference {comparison["sizeMismatch"][1]}')
            elif comparison['differingPixels']:
                failures.append(f'{path.name}: {comparison["differingPixels"]} differing pixels in {comparison["box"]}')
        print(variant, ', '.join(f"{s['state']}={'new' if s['comparison'] is None else s['comparison'].get('differingPixels', 'size')}"
                                 for s in entry['shots']))
    evidence['passed'] = not failures
    evidence['failures'] = failures
    (data / 'results.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')
    for failure in failures:
        print('FAIL', failure, file=sys.stderr)
    print(json.dumps({'passed': evidence['passed'], 'failures': len(failures), 'evidence': str(data / 'results.json')}))
    return 0 if evidence['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
