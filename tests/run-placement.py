"""Verify ordinary launch centers the reel on its captured pointer, without moving it."""
from pathlib import Path
from ctypes import wintypes as w
import ctypes,json,os,re,subprocess,time
root=Path(__file__).resolve().parents[1]
data=root/'artifacts/qa/placement';data.mkdir(parents=True,exist_ok=True)
log=data/'run.log'
if log.exists():log.unlink()
u=ctypes.windll.user32
u.FindWindowW.argtypes=[w.LPCWSTR,w.LPCWSTR];u.FindWindowW.restype=w.HWND
assert not u.FindWindowW('CopilotReel.Controller','Copilot Reel'),'Exit the resident instance first.'
env=os.environ.copy();env['COPILOT_REEL_DATA_DIR']=str(data)
exe=str(root/'build/Release/CopilotReel.exe')
p=subprocess.Popen([exe],env=env,creationflags=0x08000000)
try:
    found=None
    for _ in range(150):
        if log.exists():
            found=re.search(r'pointer=(-?\d+),(-?\d+); window=(-?\d+),(-?\d+),(-?\d+),(-?\d+)',log.read_text(encoding='utf-8'))
            if found:break
        time.sleep(.03)
    assert found,'Default launch did not open a pointer-centered preview.'
    x,y,left,top,right,bottom=map(int,found.groups())
    assert abs((left+right)/2-x)<=1 and abs((top+bottom)/2-y)<=1
    result={'passed':True,'pointer':[x,y],'window':[left,top,right,bottom],'cursorWarpedByTest':False,'note':'Edge clipping is a separate pending adaptive-layout concern.'}
    (data/'results.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps(result))
finally:
    subprocess.run([exe,'--quit'],env=env,creationflags=0x08000000,timeout=8);p.wait(timeout=5)
