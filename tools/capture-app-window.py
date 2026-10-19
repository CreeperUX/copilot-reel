"""Capture one visible Copilot Reel window for local visual QA.

This helper reads only the selected app window rectangle, not a full desktop.
Requires Pillow. It does not open windows, move the pointer, or send input.
"""
import argparse
import ctypes
from ctypes import wintypes as w
from pathlib import Path
from PIL import Image

parser=argparse.ArgumentParser()
parser.add_argument('--class-name',default='CopilotReel.Osd')
parser.add_argument('--title')
parser.add_argument('--output',required=True)
parser.add_argument('--screen',action='store_true',help='Capture visible screen pixels only when the app is known to be unobscured.')
args=parser.parse_args()
if args.class_name not in ['CopilotReel.Osd','CopilotReel.Controller']:
    raise SystemExit('Only Copilot Reel window classes are admitted.')
u=ctypes.windll.user32;g=ctypes.windll.gdi32
u.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
u.FindWindowW.argtypes=[w.LPCWSTR,w.LPCWSTR];u.FindWindowW.restype=w.HWND
u.GetWindowRect.argtypes=[w.HWND,ctypes.POINTER(w.RECT)]
u.IsWindowVisible.argtypes=[w.HWND]
u.GetDC.argtypes=[w.HWND];u.GetDC.restype=w.HDC
u.ReleaseDC.argtypes=[w.HWND,w.HDC]
u.PrintWindow.argtypes=[w.HWND,w.HDC,w.UINT];u.PrintWindow.restype=w.BOOL
g.CreateCompatibleDC.argtypes=[w.HDC];g.CreateCompatibleDC.restype=w.HDC
g.CreateCompatibleBitmap.argtypes=[w.HDC,ctypes.c_int,ctypes.c_int];g.CreateCompatibleBitmap.restype=w.HBITMAP
g.SelectObject.argtypes=[w.HDC,w.HANDLE];g.SelectObject.restype=w.HANDLE
g.BitBlt.argtypes=[w.HDC,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_int,w.HDC,ctypes.c_int,ctypes.c_int,w.DWORD]
g.DeleteObject.argtypes=[w.HANDLE];g.DeleteDC.argtypes=[w.HDC]
g.GetDIBits.argtypes=[w.HDC,w.HBITMAP,w.UINT,w.UINT,w.LPVOID,w.LPVOID,w.UINT]
hwnd=u.FindWindowW(args.class_name,args.title)
if not hwnd or not u.IsWindowVisible(hwnd):raise SystemExit('The requested app window is not visible.')
rect=w.RECT();u.GetWindowRect(hwnd,ctypes.byref(rect))
width,height=rect.right-rect.left,rect.bottom-rect.top
if not 0<width<=8192 or not 0<height<=8192:raise SystemExit('Invalid capture bounds.')
class BitmapHeader(ctypes.Structure):
    _fields_=[('size',w.DWORD),('width',w.LONG),('height',w.LONG),('planes',w.WORD),('bits',w.WORD),('compression',w.DWORD),('imageSize',w.DWORD),('x',w.LONG),('y',w.LONG),('used',w.DWORD),('important',w.DWORD)]
screen=u.GetDC(None);dc=g.CreateCompatibleDC(screen);bitmap=g.CreateCompatibleBitmap(screen,width,height);old=g.SelectObject(dc,bitmap)
try:
    captured=(g.BitBlt(dc,0,0,width,height,screen,rect.left,rect.top,0x00CC0020) if args.screen else u.PrintWindow(hwnd,dc,2))
    if not captured:raise RuntimeError('Window capture failed.')
    header=BitmapHeader(40,width,-height,1,32,0,0,0,0,0,0);pixels=ctypes.create_string_buffer(width*height*4)
    if g.GetDIBits(dc,bitmap,0,height,pixels,ctypes.byref(header),0)!=height:raise RuntimeError('Pixel readback failed.')
    output=Path(args.output);output.parent.mkdir(parents=True,exist_ok=True)
    Image.frombuffer('RGB',(width,height),pixels.raw,'raw','BGRX',0,1).save(output)
    print(str(output))
finally:
    g.SelectObject(dc,old);g.DeleteObject(bitmap);g.DeleteDC(dc);u.ReleaseDC(None,screen)
