"""Build the Copilot Reel VI study as vector PDF and SVG assets.

Requires Python + reportlab and the Windows Segoe UI / Microsoft YaHei fonts.
All artwork is drawn from the same vector scene instructions; PNG previews are
rendered separately from the resulting PDF with Poppler.
"""
from __future__ import annotations

import json
import math
from contextlib import contextmanager
from pathlib import Path
from xml.sax.saxutils import escape

from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.colors import HexColor

ROOT = Path(__file__).resolve().parents[3]
VI = ROOT / 'design' / 'vi'
VECTORS = VI / 'vectors'
PDF_DIR = ROOT / 'output' / 'pdf'
PAGE_W, PAGE_H = 1440, 900
PDF_SCALE = 2 / 3
INK, MUTED, LINE = '#20252B', '#626A75', '#DDE2E8'
BLUE = '#005FB8'

for name, filename in [('Segoe', 'segoeui.ttf'), ('SegoeBold', 'segoeuib.ttf'),
                       ('YaHei', 'msyh.ttc'), ('YaHeiBold', 'msyhbd.ttc')]:
    pdfmetrics.registerFont(TTFont(name, str(Path('C:/Windows/Fonts') / filename), subfontIndex=0))


def font_for(text, bold=False):
    return ('YaHei' if any(ord(c) > 255 for c in text) else 'Segoe') + ('Bold' if bold else '')


class Scene:
    def __init__(self, w=PAGE_W, h=PAGE_H):
        self.w, self.h, self.ops = w, h, []

    def rect(self, x, y, w, h, fill=None, stroke=None, sw=1, r=0, alpha=1):
        self.ops.append(('rect', (x, y, w, h, fill, stroke, sw, r, alpha)))

    def circle(self, x, y, r, fill=None, stroke=None, sw=1, alpha=1):
        self.ops.append(('circle', (x, y, r, fill, stroke, sw, alpha)))

    def path(self, points, fill=None, stroke=None, sw=1, alpha=1):
        self.ops.append(('path', (points, fill, stroke, sw, alpha)))

    def line(self, x1, y1, x2, y2, color=LINE, sw=1, alpha=1):
        self.path([('M', x1, y1), ('L', x2, y2)], stroke=color, sw=sw, alpha=alpha)

    def text(self, x, y, value, size=18, color=INK, bold=False, align='left'):
        self.ops.append(('text', (x, y, str(value), size, color, bold, align)))

    def wrap(self, x, y, text, width, size=18, leading=30, color=INK, bold=False):
        lines = []
        font = font_for(text, bold)
        for paragraph in text.split('\n'):
            line = ''
            for word in paragraph.split(' '):
                candidate = (line+' '+word).strip()
                if line and pdfmetrics.stringWidth(candidate,font,size)>width:
                    lines.append(line);line=word
                else: line=candidate
            if line: lines.append(line)
        for i, value in enumerate(lines):
            self.text(x, y + i * leading, value, size, color, bold)
        return y + len(lines) * leading

    @contextmanager
    def group(self, x=0, y=0, scale=1):
        self.ops.append(('push', (x, y, scale)))
        yield
        self.ops.append(('pop', ()))

    def draw_pdf(self, c):
        c.saveState()
        c.scale(PDF_SCALE, PDF_SCALE)
        c.translate(0, self.h)
        c.scale(1, -1)
        for kind, p in self.ops:
            if kind == 'push':
                c.saveState()
                c.translate(p[0], p[1])
                c.scale(p[2], p[2])
                continue
            if kind == 'pop':
                c.restoreState()
                continue
            c.saveState()
            if kind == 'text':
                x, y, value, size, color, bold, align = p
                c.setFillColor(HexColor(color))
                c.setFont(font_for(value, bold), size)
                c.translate(x, y)
                c.scale(1, -1)
                draw = {'left': c.drawString, 'center': c.drawCentredString, 'right': c.drawRightString}[align]
                draw(0, -size * .84, value)
            else:
                if kind == 'rect':
                    x, y, w, h, fill, stroke, sw, radius, alpha = p
                elif kind == 'circle':
                    x, y, radius, fill, stroke, sw, alpha = p
                else:
                    points, fill, stroke, sw, alpha = p
                if fill:
                    c.setFillColor(HexColor(fill))
                if stroke:
                    c.setStrokeColor(HexColor(stroke))
                # ReportLab's color setter also sets opacity; apply it afterwards.
                c.setFillAlpha(alpha)
                c.setStrokeAlpha(alpha)
                c.setLineWidth(sw)
                c.setLineJoin(1)
                c.setLineCap(1)
                if kind == 'rect':
                    c.roundRect(x, y, w, h, radius, fill=bool(fill), stroke=bool(stroke))
                elif kind == 'circle':
                    c.circle(x, y, radius, fill=bool(fill), stroke=bool(stroke))
                else:
                    path = c.beginPath()
                    for item in points:
                        cmd, *v = item
                        if cmd == 'M': path.moveTo(*v)
                        elif cmd == 'L': path.lineTo(*v)
                        elif cmd == 'C': path.curveTo(*v)
                        elif cmd == 'Z': path.close()
                    c.drawPath(path, fill=bool(fill), stroke=bool(stroke))
            c.restoreState()
        c.restoreState()

    def svg(self, title):
        def n(v): return f'{v:.3f}'.rstrip('0').rstrip('.') if isinstance(v, (float, int)) else str(v)
        out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}" viewBox="0 0 {self.w} {self.h}" role="img">',
               f'<title>{escape(title)}</title>',
               '<desc>Copilot Reel concept vector master. Application symbols are placeholders. No raster images are embedded.</desc>']
        for kind, p in self.ops:
            if kind == 'push':
                out.append(f'<g transform="translate({n(p[0])} {n(p[1])}) scale({n(p[2])})">')
                continue
            if kind == 'pop':
                out.append('</g>')
                continue
            if kind == 'text':
                x, y, value, size, color, bold, align = p
                anchor = {'left': 'start', 'center': 'middle', 'right': 'end'}[align]
                out.append(f'<text x="{n(x)}" y="{n(y+size*.84)}" fill="{color}" font-family="Segoe UI Variable,Segoe UI,Microsoft YaHei,sans-serif" font-size="{n(size)}" font-weight="{600 if bold else 400}" text-anchor="{anchor}">{escape(value)}</text>')
                continue
            if kind == 'rect':
                x, y, w, h, fill, stroke, sw, radius, alpha = p
                start = f'<rect x="{n(x)}" y="{n(y)}" width="{n(w)}" height="{n(h)}" rx="{n(radius)}"'
            elif kind == 'circle':
                x, y, radius, fill, stroke, sw, alpha = p
                start = f'<circle cx="{n(x)}" cy="{n(y)}" r="{n(radius)}"'
            else:
                points, fill, stroke, sw, alpha = p
                d = ' '.join(a[0] + ' '.join(n(v) for v in a[1:]) for a in points)
                start = f'<path d="{d}"'
            out.append(start + f' fill="{fill or "none"}" stroke="{stroke or "none"}" stroke-width="{n(sw)}" stroke-linejoin="round" stroke-linecap="round" opacity="{n(alpha)}"/>')
        out.append('</svg>')
        return '\n'.join(out) + '\n'


def point(r, deg):
    a = math.radians(deg)
    return r * math.cos(a), r * math.sin(a)


def arc(r, a, b, move=True):
    out = [('M' if move else 'L', *point(r, a))]
    count = max(1, math.ceil(abs(b-a) / 60))
    for i in range(count):
        a0 = math.radians(a + (b-a)*i/count)
        a1 = math.radians(a + (b-a)*(i+1)/count)
        k = 4/3*math.tan((a1-a0)/4)
        x0, y0, x1, y1 = r*math.cos(a0), r*math.sin(a0), r*math.cos(a1), r*math.sin(a1)
        out.append(('C', x0-k*y0, y0+k*x0, x1+k*y1, y1-k*x1, x1, y1))
    return out


def wedge(ro, ri, a, b):
    return arc(ro, a, b) + arc(ri, b, a, False) + [('Z',)]


PALETTES = {
    'light': {'surface':'#F4F6F8', 'inner':'#FFFFFF', 'tile':'#FCFCFD', 'text':'#20252B', 'muted':'#626B76', 'line':'#CCD3DC', 'accent':'#005FB8', 'active':'#DEECFA', 'glyph':'#4A515D', 'icon':'#E5E9EF'},
    'dark': {'surface':'#292D33', 'inner':'#22252A', 'tile':'#343940', 'text':'#F3F5F8', 'muted':'#BEC5CE', 'line':'#545C67', 'accent':'#60CDFF', 'active':'#294555', 'glyph':'#E5EAF0', 'icon':'#444C57'},
    'contrast': {'surface':'#000000', 'inner':'#000000', 'tile':'#000000', 'text':'#FFFFFF', 'muted':'#FFFFFF', 'line':'#FFFFFF', 'accent':'#FFFF00', 'active':'#222200', 'glyph':'#FFFFFF', 'icon':'#000000'},
}
APPS = ['ChatGPT', 'Claude', 'Gemini', 'Codex', 'Perplexity', 'Custom']
SCHEMES = [('halo', 'Fluent Halo', 'FLUENT HALO'), ('petals', 'Command Petals', 'COMMAND PETALS'), ('orbit', 'Soft Orbit', 'SOFT ORBIT')]


def shadow_circle(s, x, y, r, alpha=.10):
    for extra, opacity in [(12,.10), (8,.16), (5,.22), (2,.28)]:
        s.circle(x, y+5+extra*.15, r+extra, '#000000', alpha=alpha*opacity)


def shadow_rect(s, x, y, w, h, r=8, alpha=.08):
    for extra, opacity in [(8,.12), (5,.20), (2,.30)]:
        s.rect(x-extra, y-extra+5, w+extra*2, h+extra*2, '#000000', r=r+extra, alpha=alpha*opacity)


def glyph(s, index, x, y, size, color):
    with s.group(x, y, size/24):
        if index == 0:
            s.circle(-4, 0, 6, stroke=color, sw=1.8)
            s.circle(4, 0, 6, stroke=color, sw=1.8)
        elif index == 1:
            for a in range(0, 360, 45):
                x1,y1 = point(4,a); x2,y2 = point(10,a)
                s.line(x1,y1,x2,y2,color,1.8)
        elif index == 2:
            s.path([('M',0,-10),('C',2,-3,3,-2,10,0),('C',3,2,2,3,0,10),('C',-2,3,-3,2,-10,0),('C',-3,-2,-2,-3,0,-10),('Z',)], fill=color)
        elif index == 3:
            s.path([('M',-4,-7),('L',-10,0),('L',-4,7)], stroke=color, sw=1.9)
            s.path([('M',4,-7),('L',10,0),('L',4,7)], stroke=color, sw=1.9)
            s.line(2,-9,-2,9,color,1.5)
        elif index == 4:
            s.rect(-8,-8,16,16,stroke=color,sw=1.6,r=3)
            s.line(0,-10,0,10,color,1.4)
            s.line(-10,0,10,0,color,1.4)
        else:
            s.line(-7,0,7,0,color,1.8)
            s.line(0,-7,0,7,color,1.8)


def pointer(s, x, y, dark=False, scale=1):
    with s.group(x,y,scale):
        s.path([('M',0,0),('L',2,21),('L',7,15),('L',12,24),('L',16,22),('L',11,13),('L',19,12),('Z',)], '#FFFFFF' if dark else '#111820', '#111820' if dark else '#FFFFFF', 1.1)


def wheel(s, scheme, theme='light', selected=1, x=0, y=0, scale=1, show_pointer=False, material='solid'):
    p = PALETTES[theme]
    hc = theme == 'contrast'
    with s.group(x,y,scale):
        if scheme == 'halo':
            if not hc: shadow_circle(s,0,0,160)
            s.circle(0,0,160,p['surface'],p['line'],2 if hc else 1)
            if selected is not None:
                a = -90+selected*60
                s.path(wedge(157,47,a-29,a+29),p['active'])
                s.path(arc(157,a-27,a+27),stroke=p['accent'],sw=3 if hc else 2.5)
            for i in range(6):
                a = -120+i*60
                x1,y1=point(49,a); x2,y2=point(149,a)
                s.line(x1,y1,x2,y2,p['line'],.7)
                ix,iy=point(109,-90+i*60)
                s.rect(ix-18,iy-23,36,36,p['icon'],r=8)
                glyph(s,i,ix,iy-5,28,p['glyph'])
                s.text(ix,iy+19,APPS[i],14,p['text'],align='center')
        elif scheme == 'petals':
            for i in range(6):
                ix,iy=point(132,-90+i*60)
                active = i == selected
                if not hc: shadow_rect(s,ix-52,iy-34,104,68,8)
                s.rect(ix-52,iy-34,104,68,p['active'] if active else p['tile'],p['accent'] if active else p['line'],2 if active or hc else 1,8)
                if active:
                    s.rect(ix-28,iy+29,56,3,p['accent'],r=1.5)
                glyph(s,i,ix,iy-10,25,p['glyph'])
                s.text(ix,iy+9,APPS[i],14,p['text'],align='center')
        else:
            if not hc: shadow_circle(s,0,0,168,.12)
            frosted = material == 'frosted' and not hc
            s.circle(0,0,168,p['surface'],p['line'],2 if hc else 1,alpha=.88 if frosted else 1)
            if frosted:
                # Vector-only tonal illustration, not a captured or blurred desktop.
                s.circle(0,0,166,p['surface'],alpha=.25)
                s.circle(0,-1,161,stroke=p['inner'],sw=1,alpha=.7)
            for i in range(6):
                ix,iy=point(112,-90+i*60)
                active = i == selected
                if active:
                    s.rect(ix-30,iy-30,60,64,p['active'],p['accent'],2 if hc else 1.5,14)
                    s.circle(ix,iy+25,2,p['accent'])
                s.rect(ix-20,iy-20,40,40,p['icon'],r=10)
                glyph(s,i,ix,iy,29,p['glyph'])
        s.circle(0,0,44,p['inner'],p['line'],2 if hc else 1)
        if selected is None:
            s.line(-4,-12,4,-4,p['muted'],1.4)
            s.line(4,-12,-4,-4,p['muted'],1.4)
            s.text(0,9,'Cancel',12,p['muted'],align='center')
        else:
            s.text(0,-16,APPS[selected],14,p['text'],True,'center')
            s.text(0,7,'Release',12,p['muted'],align='center')
        if show_pointer:
            px,py=point(70,-12) if selected is not None else (4,-4)
            pointer(s,px,py,theme=='dark',.8)


def logo(s, scheme, x, y, size=64, tile=True, color=BLUE, foreground='#FFFFFF'):
    with s.group(x,y,size/64):
        if tile:
            s.rect(0,0,64,64,color,r=14)
        mark=foreground if tile else color
        with s.group(32,32):
            if scheme == 'halo':
                for a in [-90,30,150]: s.path(wedge(22,14,a,a+105),mark)
                s.circle(0,0,4,mark)
            elif scheme == 'petals':
                for a in range(-90,270,60):
                    xx,yy=point(19,a)
                    s.rect(xx-5,yy-5,10,10,mark,r=2.5)
                s.circle(0,0,4,mark)
            else:
                s.path(arc(23,-30,280),stroke=mark,sw=3)
                s.path(arc(14,150,440),stroke=mark,sw=3)
                xx,yy=point(23,-50)
                s.circle(xx,yy,4,mark)


def desktop(s,x,y,w,h,dark=False):
    bg='#141D2A' if dark else '#E8EFF6'
    panel='#202B39' if dark else '#F8FAFC'
    edge='#314050' if dark else '#D5DFEB'
    s.rect(x,y,w,h,bg,r=16)
    s.path([('M',x,y+h*.72),('C',x+w*.25,y+h*.28,x+w*.65,y+h*.94,x+w,y+h*.25),('L',x+w,y+h),('L',x,y+h),('Z',)], '#243C58' if dark else '#D5E4F3',alpha=.9)
    px,py,pw,ph=x+30,y+26,w-60,h-56
    s.rect(px,py,pw,ph,panel,edge,1,10)
    s.line(px,py+34,px+pw,py+34,edge)
    for i in range(3): s.circle(px+14+i*12,py+17,2.5,edge)
    s.text(px+54,py+10,'Workspace',12,'#AAB8CA' if dark else '#7B8A9D')
    for i in range(6):
        s.rect(px+20,py+61+i*27,(pw-44)*[.7,.92,.86,.54,.8,.63][i],5,edge,r=2.5,alpha=.55)


def footer(s,page,total=10):
    s.line(64,835,1376,835,LINE)
    s.text(64,852,'COPILOT REEL  /  SOFT ORBIT + OSD CONTROLS  /  v0.3',12,MUTED)
    s.text(1376,852,f'{page:02d} / {total:02d}',12,MUTED,align='right')


def base(page,section,title,subtitle=''):
    s=Scene()
    s.rect(0,0,PAGE_W,PAGE_H,'#FFFFFF')
    s.text(64,42,section,13,BLUE,True)
    s.text(64,82,title,40,INK,True)
    if subtitle: s.text(64,142,subtitle,18,MUTED)
    footer(s,page)
    return s


def note(s,x,y,kicker,title,body,width=380):
    s.text(x,y,kicker,12,BLUE,True)
    s.text(x,y+28,title,23,INK,True)
    return s.wrap(x,y+67,body,width,17,28,MUTED)


def swatches(s,x,y,values,width=500):
    cell=width/len(values)
    for i,(name,color) in enumerate(values):
        xx=x+i*cell
        s.rect(xx,y,cell-14,48,color,LINE,.5,8)
        s.text(xx,y+61,name,13,MUTED)
        s.text(xx,y+83,color.upper(),15,INK)
