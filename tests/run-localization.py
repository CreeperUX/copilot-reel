"""Verify native language switching and persistence in an isolated data directory."""
import ctypes
from ctypes import wintypes as w
import json,os,subprocess,time
from pathlib import Path

root=Path(__file__).resolve().parents[1]
exe=root/'build/Release/CopilotReel.exe'
data=root/'artifacts/qa/localization'
data.mkdir(parents=True,exist_ok=True)
preferences=data/'preferences.ini'
if preferences.exists():preferences.unlink()
env=os.environ.copy();env['COPILOT_REEL_DATA_DIR']=str(data)
u=ctypes.windll.user32
u.FindWindowW.argtypes=[w.LPCWSTR,w.LPCWSTR];u.FindWindowW.restype=w.HWND
u.GetDlgItem.argtypes=[w.HWND,ctypes.c_int];u.GetDlgItem.restype=w.HWND
u.GetWindowTextW.argtypes=[w.HWND,w.LPWSTR,ctypes.c_int]
u.SendMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM];u.SendMessageW.restype=w.LPARAM
u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
assert not u.FindWindowW('CopilotReel.Controller','Copilot Reel'),'Exit the resident instance first.'

def wait_settings():
    for _ in range(150):
        hwnd=u.FindWindowW('CopilotReel.Settings',None)
        if hwnd and u.GetDlgItem(hwnd,122) and u.IsWindowVisible(hwnd):return hwnd
        time.sleep(.03)
    raise AssertionError('Settings window did not appear')

def title(hwnd):
    text=ctypes.create_unicode_buffer(512);u.GetWindowTextW(hwnd,text,512);return text.value

def switch(hwnd,index):
    control=u.GetDlgItem(hwnd,122);assert control
    u.SendMessageW(control,0x014e,index,0)
    u.SendMessageW(hwnd,0x0111,122|(1<<16),control)

def stop(p,hwnd):
    u.PostMessageW(hwnd,0x0010,0,0)
    time.sleep(.1)
    r=subprocess.run([str(exe),'--quit'],env=env,creationflags=0x08000000,timeout=8)
    assert r.returncode==0 and p.wait(timeout=5)==0

p=subprocess.Popen([str(exe),'--settings'],env=env,creationflags=0x08000000)
h=wait_settings()
assert 'Application settings' in title(h),title(h)
switch(h,1)
assert '\u5e94\u7528\u8bbe\u7f6e' in title(h),title(h)
assert 'zh-CN' in preferences.read_text(encoding='utf-8-sig')
stop(p,h)
p=subprocess.Popen([str(exe),'--settings'],env=env,creationflags=0x08000000)
h=wait_settings()
assert '\u5e94\u7528\u8bbe\u7f6e' in title(h),title(h)
switch(h,0)
assert 'Application settings' in title(h),title(h)
assert 'en-US' in preferences.read_text(encoding='utf-8-sig')
stop(p,h)
result={'passed':['English on first launch','native switch to Simplified Chinese','preference persistence after restart','native switch back to English'],'isolated':True}
(data/'results.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result))
