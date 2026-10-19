"""Exercise the actual Win32 controller, process handoff, Raw Input and launcher.
Uses an isolated configuration and a harmless launch probe. Restores the cursor.
Do not run while another Copilot Reel instance is resident.
"""
import ctypes
from ctypes import wintypes as w
import json, math, os, subprocess, time
from pathlib import Path

root=Path(__file__).resolve().parents[1]
exe=root/'build/Release/CopilotReel.exe'
probe=root/'build/Release/launch_probe.exe'
data=root/'artifacts/qa/integration'
data.mkdir(parents=True,exist_ok=True)
marker=data/'launch-probe.txt'
if marker.exists(): marker.unlink()
log=data/'run.log'
if log.exists(): log.unlink()
target=str(probe).replace('\\','\\\\')
config='; isolated integration configuration\r\n[CopilotReel]\r\nVersion=1\r\nCount=6\r\n'
for i in range(6):
    config+=f'[App{i}]\r\nName=Probe {i}\r\nKind=shell\r\nTarget={target}\r\nArguments=\r\nWorkingDirectory=\r\n'
(data/'apps.ini').write_text(config,encoding='utf-16',newline='')
env=os.environ.copy();env['COPILOT_REEL_DATA_DIR']=str(data)
u=ctypes.windll.user32
u.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
u.FindWindowW.argtypes=[w.LPCWSTR,w.LPCWSTR];u.FindWindowW.restype=w.HWND
u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
u.IsWindowVisible.argtypes=[w.HWND]
u.GetDpiForWindow.argtypes=[w.HWND];u.GetDpiForWindow.restype=w.UINT
assert not u.FindWindowW('CopilotReel.Controller','Copilot Reel'),'Another instance is resident; stop it before this isolated test.'
start=w.POINT();u.GetCursorPos(ctypes.byref(start))
flags=0x08000000
p=subprocess.Popen([str(exe),'--background'],env=env,creationflags=flags)
results=[]

def wait(predicate, timeout=5):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        if predicate(): return
        time.sleep(.03)
    raise AssertionError('Timed out waiting for expected UI state')

def command(arg):
    result=subprocess.run([str(exe),arg],env=env,creationflags=flags,timeout=8)
    assert result.returncode==0,(arg,result.returncode)

try:
    wait(lambda:u.FindWindowW('CopilotReel.Controller','Copilot Reel'))
    hwnd=u.FindWindowW('CopilotReel.Controller','Copilot Reel')
    wait(lambda:(data/'run.log').exists() and 'Ready;' in (data/'run.log').read_text(encoding='utf-8'))
    assert 'Configuration validation failed' not in (data/'run.log').read_text(encoding='utf-8')
    # Put the pointer in the primary display's safe center, then preserve this anchor.
    anchor=(u.GetSystemMetrics(0)//2,u.GetSystemMetrics(1)//2)
    u.SetCursorPos(*anchor)
    command('--hold-start');wait(lambda:u.IsWindowVisible(hwnd))
    time.sleep(.15)
    dpi=u.GetDpiForWindow(hwnd)
    radius=112*dpi/96
    u.SetCursorPos(round(anchor[0]+radius*math.cos(-math.pi/6)),round(anchor[1]+radius*math.sin(-math.pi/6)))
    time.sleep(.12)
    command('--hold-stop')
    wait(lambda:marker.exists())
    wait(lambda:not u.IsWindowVisible(hwnd))
    assert marker.read_text(encoding='utf-8').count('launched')==1
    command('--hold-stop');time.sleep(.15)
    assert marker.read_text(encoding='utf-8').count('launched')==1
    results.append('warm process handoff + Raw Input selection + release launches exactly once')
    u.SetCursorPos(*anchor)
    command('--hold-start');wait(lambda:u.IsWindowVisible(hwnd))
    command('--hold-stop');wait(lambda:not u.IsWindowVisible(hwnd))
    assert marker.read_text(encoding='utf-8').count('launched')==1
    results.append('center release cancels')
    u.SetCursorPos(*anchor)
    # Same message and wParam contract declared in AppxManifest.
    u.PostMessageW(hwnd,0x8001,1,0);wait(lambda:u.IsWindowVisible(hwnd))
    command('--cancel');wait(lambda:not u.IsWindowVisible(hwnd))
    u.PostMessageW(hwnd,0x8001,2,0);time.sleep(.12)
    assert marker.read_text(encoding='utf-8').count('launched')==1
    results.append('fast-path message contract and canceled release')
    command('--diagnostics');time.sleep(.1)
    command('--quit');assert p.wait(timeout=5)==0
    results.append('clean process shutdown')
finally:
    u.SetCursorPos(start.x,start.y)
    if p.poll() is None:
        command('--quit');p.wait(timeout=5)
(data/'results.json').write_text(json.dumps({'passed':results,'note':'Hardware Copilot and Settings selection are separate manual/system integration checks.'},indent=2),encoding='utf-8')
print(json.dumps({'passed':results},ensure_ascii=False))
