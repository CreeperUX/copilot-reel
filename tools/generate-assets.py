"""Rasterize the Soft Orbit brand geometry for Windows-required package/ICO assets.
The actual wheel UI is rendered from native vector geometry.
Requires Pillow; outputs are build-time assets, not an animation frame sequence.
"""
from pathlib import Path
from PIL import Image, ImageDraw

root=Path(__file__).resolve().parents[1]
out=root/'packaging/assets'
out.mkdir(parents=True,exist_ok=True)
scale=8
im=Image.new('RGBA',(256*scale,256*scale),(0,0,0,0))
d=ImageDraw.Draw(im)
def box(x,y,w,h): return tuple(int(v*scale) for v in (x,y,x+w,y+h))
d.rounded_rectangle(box(0,0,256,256),radius=56*scale,fill='#005FB8')
d.arc(box(36,36,184,184),start=-30,end=280,fill='white',width=12*scale)
d.arc(box(72,72,112,112),start=150,end=440,fill='white',width=12*scale)
import math
x=128+92*math.cos(math.radians(-50));y=128+92*math.sin(math.radians(-50))
d.ellipse(box(x-16,y-16,32,32),fill='white')
resample=Image.Resampling.LANCZOS
for name,size in [('Square44x44Logo',44),('Square150x150Logo',150),('StoreLogo',50)]:
    im.resize((size,size),resample).save(out/f'{name}.png')
im.resize((256,256),resample).save(out/'CopilotReel.ico',sizes=[(s,s) for s in [16,24,32,48,64,128,256]])
print('Generated 3 package PNGs and Soft Orbit ICO.')
