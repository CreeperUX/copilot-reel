"""Vector scene recorder with PDF (reportlab) and SVG back ends.

Coordinates are y-down. A Scene records drawing operations once and replays them into
either back end, so the PDF pages and the SVG masters come from one description.
Colors are '#RRGGBB' strings or (r, g, b[, a]) float tuples; `alpha` multiplies.
"""
from __future__ import annotations

import base64
import io
import math
from contextlib import contextmanager
from xml.sax.saxutils import escape

from reportlab.lib.colors import Color
from reportlab.lib.utils import ImageReader

import fonts


def rgba(value, alpha=1.0):
    if value is None:
        return None
    if isinstance(value, str):
        v = value.lstrip('#')
        return (int(v[0:2], 16) / 255, int(v[2:4], 16) / 255, int(v[4:6], 16) / 255, alpha)
    if len(value) == 3:
        return (*value, alpha)
    return (value[0], value[1], value[2], value[3] * alpha)


def hexrgb(c):
    return '#' + ''.join(f'{round(max(0, min(1, v)) * 255):02X}' for v in c[:3])


def num(v):
    if isinstance(v, float):
        s = f'{v:.3f}'.rstrip('0').rstrip('.')
        return '0' if s in ('-0', '') else s
    return str(v)


# Path helpers -------------------------------------------------------------------------

def arc(cx, cy, r, a0, a1, move=True):
    """Circular arc from a0 to a1 degrees (y down, clockwise positive) as cubic segments."""
    out = []
    p0 = (cx + r * math.cos(math.radians(a0)), cy + r * math.sin(math.radians(a0)))
    if move:
        out.append(('M', *p0))
    count = max(1, math.ceil(abs(a1 - a0) / 45))
    for i in range(count):
        t0 = math.radians(a0 + (a1 - a0) * i / count)
        t1 = math.radians(a0 + (a1 - a0) * (i + 1) / count)
        k = 4 / 3 * math.tan((t1 - t0) / 4)
        x0, y0 = cx + r * math.cos(t0), cy + r * math.sin(t0)
        x1, y1 = cx + r * math.cos(t1), cy + r * math.sin(t1)
        out.append(('C', x0 - k * r * math.sin(t0), y0 + k * r * math.cos(t0),
                    x1 + k * r * math.sin(t1), y1 - k * r * math.cos(t1), x1, y1))
    return out


def round_rect_path(x, y, w, h, r):
    r = max(0.0, min(r, w / 2, h / 2))
    if r == 0:
        return [('M', x, y), ('L', x + w, y), ('L', x + w, y + h), ('L', x, y + h), ('Z',)]
    out = [('M', x + r, y), ('L', x + w - r, y)]
    out += arc(x + w - r, y + r, r, -90, 0, False)
    out += [('L', x + w, y + h - r)]
    out += arc(x + w - r, y + h - r, r, 0, 90, False)
    out += [('L', x + r, y + h)]
    out += arc(x + r, y + h - r, r, 90, 180, False)
    out += [('L', x, y + r)]
    out += arc(x + r, y + r, r, 180, 270, False)
    out.append(('Z',))
    return out


def circle_path(cx, cy, r):
    return arc(cx, cy, r, 0, 360) + [('Z',)]


def transform_path(cmds, dx=0, dy=0, s=1):
    out = []
    for cmd, *v in cmds:
        out.append((cmd, *[(v[i] * s + (dx if i % 2 == 0 else dy)) for i in range(len(v))]))
    return out


class Scene:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.ops = []
        self._alpha = [1.0]

    # Recording -------------------------------------------------------------------------
    def _a(self, alpha):
        return alpha * self._alpha[-1]

    def path(self, cmds, fill=None, stroke=None, sw=1.0, alpha=1.0, dash=None, cap='round', join='round'):
        a = self._a(alpha)
        if a <= 0 or (fill is None and stroke is None):
            return
        self.ops.append(('path', dict(cmds=cmds, fill=rgba(fill, a) if fill is not None else None,
                                      stroke=rgba(stroke, a) if stroke is not None else None,
                                      sw=sw, dash=dash, cap=cap, join=join)))

    def rect(self, x, y, w, h, fill=None, stroke=None, sw=1.0, r=0.0, alpha=1.0, dash=None, inset=False):
        """`inset` keeps the stroke fully inside the box (Direct2D-style inset by sw / 2)."""
        if stroke is not None and inset:
            if fill is not None:
                self.path(round_rect_path(x, y, w, h, r), fill=fill, alpha=alpha)
            d = sw / 2
            self.path(round_rect_path(x + d, y + d, w - sw, h - sw, max(0, r - d)), stroke=stroke, sw=sw,
                      alpha=alpha, dash=dash, join='miter' if r == 0 else 'round')
            return
        self.path(round_rect_path(x, y, w, h, r), fill, stroke, sw, alpha, dash, join='miter' if r == 0 else 'round')

    def circle(self, cx, cy, r, fill=None, stroke=None, sw=1.0, alpha=1.0, dash=None):
        self.path(circle_path(cx, cy, r), fill, stroke, sw, alpha, dash, cap='butt')

    def line(self, x1, y1, x2, y2, color, sw=1.0, alpha=1.0, dash=None, cap='butt'):
        self.path([('M', x1, y1), ('L', x2, y2)], stroke=color, sw=sw, alpha=alpha, dash=dash, cap=cap)

    def text(self, x, y, value, size=14.0, color='#1B1B1B', face='text', cjk='cjk', align='left',
             tracking=0.0, alpha=1.0, width=None):
        """Baseline text. `face`/`cjk` are fonts.FACES names; CJK ranges use the CJK face."""
        value = str(value)
        a = self._a(alpha)
        if not value or a <= 0:
            return 0.0
        latin, cjkface = fonts.key(face), fonts.key(cjk)
        w = fonts.width(value, latin, cjkface, size, tracking)
        if align == 'center':
            x -= w / 2
        elif align == 'right':
            x -= w
        self.ops.append(('text', dict(x=x, y=y, runs=fonts.runs(value, latin, cjkface), size=size,
                                      color=rgba(color, a), tracking=tracking)))
        return w

    def image(self, img, x, y, w, h, alpha=1.0, jpeg=None):
        """`jpeg`: embed as JPEG; by default opaque RGB rasters (photographic backdrops, captures) are."""
        a = self._a(alpha)
        if a <= 0:
            return
        self.ops.append(('image', dict(img=img, x=x, y=y, w=w, h=h, alpha=a, jpeg=img.mode == 'RGB' if jpeg is None else jpeg)))

    @contextmanager
    def group(self, x=0.0, y=0.0, scale=1.0, alpha=1.0):
        self.ops.append(('push', dict(x=x, y=y, s=scale, alpha=alpha)))
        self._alpha.append(self._alpha[-1] * alpha)
        try:
            yield
        finally:
            self._alpha.pop()
            self.ops.append(('pop', {}))

    @contextmanager
    def clip(self, cmds):
        self.ops.append(('clip', dict(cmds=cmds)))
        try:
            yield
        finally:
            self.ops.append(('unclip', {}))

    def clip_rect(self, x, y, w, h, r=0.0):
        return self.clip(round_rect_path(x, y, w, h, r))

    def clip_circle(self, cx, cy, r):
        return self.clip(circle_path(cx, cy, r))

    # PDF -------------------------------------------------------------------------------
    def draw_pdf(self, c, scale=1.0):
        readers = {}
        c.saveState()
        c.scale(scale, scale)
        c.translate(0, self.h)
        c.scale(1, -1)
        for kind, p in self.ops:
            if kind == 'push':
                c.saveState()
                c.translate(p['x'], p['y'])
                c.scale(p['s'], p['s'])
            elif kind == 'pop' or kind == 'unclip':
                c.restoreState()
            elif kind == 'clip':
                c.saveState()
                c.clipPath(self._pdf_path(c, p['cmds']), stroke=0, fill=0, fillMode=1)
            elif kind == 'path':
                self._pdf_shape(c, p)
            elif kind == 'text':
                self._pdf_text(c, p)
            elif kind == 'image':
                key = id(p['img'])
                if key not in readers:
                    readers[key] = ImageReader(_encoded(p['img'], 'JPEG') if p['jpeg'] else p['img'])
                c.saveState()
                c.setFillAlpha(p['alpha'])
                c.translate(p['x'], p['y'] + p['h'])
                c.scale(1, -1)
                c.drawImage(readers[key], 0, 0, p['w'], p['h'], mask='auto')
                c.restoreState()
        c.restoreState()

    @staticmethod
    def _pdf_path(c, cmds):
        path = c.beginPath()
        for cmd, *v in cmds:
            if cmd == 'M':
                path.moveTo(*v)
            elif cmd == 'L':
                path.lineTo(*v)
            elif cmd == 'C':
                path.curveTo(*v)
            elif cmd == 'Z':
                path.close()
        return path

    def _pdf_shape(self, c, p):
        c.saveState()
        fill, stroke = p['fill'], p['stroke']
        if fill:
            c.setFillColor(Color(*fill[:3]))
            c.setFillAlpha(fill[3])
        if stroke:
            c.setStrokeColor(Color(*stroke[:3]))
            c.setStrokeAlpha(stroke[3])
            c.setLineWidth(p['sw'])
            c.setLineCap({'butt': 0, 'round': 1, 'square': 2}[p['cap']])
            c.setLineJoin({'miter': 0, 'round': 1, 'bevel': 2}[p['join']])
            if p['dash']:
                c.setDash(p['dash'], 0)
        c.drawPath(self._pdf_path(c, p['cmds']), fill=1 if fill else 0, stroke=1 if stroke else 0, fillMode=1)
        c.restoreState()

    @staticmethod
    def _pdf_text(c, p):
        c.saveState()
        col = p['color']
        c.setFillColor(Color(*col[:3]))
        c.setFillAlpha(col[3])
        c.translate(p['x'], p['y'])
        c.scale(1, -1)
        t = c.beginText(0, 0)
        if p['tracking']:
            t.setCharSpace(p['tracking'])
        for face, chunk in p['runs']:
            t.setFont(face, p['size'])
            t.textOut(chunk)
        c.drawText(t)
        c.restoreState()

    # SVG -------------------------------------------------------------------------------
    def svg(self, title, desc=''):
        out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{num(self.w)}" height="{num(self.h)}" '
               f'viewBox="0 0 {num(self.w)} {num(self.h)}" role="img">',
               f'<title>{escape(title)}</title>']
        if desc:
            out.append(f'<desc>{escape(desc)}</desc>')
        clip_id = 0
        images = {}
        for kind, p in self.ops:
            if kind == 'push':
                attrs = []
                if p['x'] or p['y'] or p['s'] != 1:
                    attrs.append(f'transform="translate({num(p["x"])} {num(p["y"])}) scale({num(p["s"])})"')
                out.append('<g ' + ' '.join(attrs) + '>' if attrs else '<g>')
            elif kind in ('pop', 'unclip'):
                out.append('</g>')
            elif kind == 'clip':
                clip_id += 1
                out.append(f'<clipPath id="clip{clip_id}"><path d="{self._svg_d(p["cmds"])}"/></clipPath>'
                           f'<g clip-path="url(#clip{clip_id})">')
            elif kind == 'path':
                out.append(self._svg_shape(p))
            elif kind == 'text':
                out.append(self._svg_text(p))
            elif kind == 'image':
                key = id(p['img'])
                fmt = 'JPEG' if p['jpeg'] else 'PNG'
                if key not in images:
                    images[key] = base64.b64encode(_encoded(p['img'], fmt).getvalue()).decode('ascii')
                op = f' opacity="{num(p["alpha"])}"' if p['alpha'] < 1 else ''
                out.append(f'<image x="{num(p["x"])}" y="{num(p["y"])}" width="{num(p["w"])}" height="{num(p["h"])}"'
                           f' preserveAspectRatio="none"{op} href="data:image/{fmt.lower()};base64,{images[key]}"/>')
        out.append('</svg>')
        return '\n'.join(out) + '\n'

    @staticmethod
    def _svg_d(cmds):
        return ' '.join(cmd + ' '.join(num(float(v)) for v in vals) for cmd, *vals in cmds)

    def _svg_shape(self, p):
        attrs = [f'd="{self._svg_d(p["cmds"])}"']
        fill, stroke = p['fill'], p['stroke']
        attrs.append(f'fill="{hexrgb(fill)}"' if fill else 'fill="none"')
        if fill and fill[3] < 1:
            attrs.append(f'fill-opacity="{num(fill[3])}"')
        if stroke:
            attrs.append(f'stroke="{hexrgb(stroke)}" stroke-width="{num(float(p["sw"]))}"')
            if stroke[3] < 1:
                attrs.append(f'stroke-opacity="{num(stroke[3])}"')
            if p['cap'] != 'butt':
                attrs.append(f'stroke-linecap="{p["cap"]}"')
            if p['join'] != 'miter':
                attrs.append(f'stroke-linejoin="{p["join"]}"')
            if p['dash']:
                attrs.append(f'stroke-dasharray="{" ".join(num(float(d)) for d in p["dash"])}"')
        return '<path ' + ' '.join(attrs) + '/>'

    @staticmethod
    def _svg_text(p):
        col = p['color']
        op = f' fill-opacity="{num(col[3])}"' if col[3] < 1 else ''
        ls = f' letter-spacing="{num(float(p["tracking"]))}"' if p['tracking'] else ''
        spans = []
        for face, chunk in p['runs']:
            family, weight = fonts.css(face)
            spans.append(f'<tspan font-family="{family}" font-weight="{weight}">{escape(chunk)}</tspan>')
        return (f'<text x="{num(float(p["x"]))}" y="{num(float(p["y"]))}" font-size="{num(float(p["size"]))}" '
                f'fill="{hexrgb(col)}"{op}{ls} xml:space="preserve">' + ''.join(spans) + '</text>')


def _encoded(img, fmt):
    buf = io.BytesIO()
    if fmt == 'JPEG':
        img.convert('RGB').save(buf, 'JPEG', quality=90, subsampling=0, optimize=True)
    else:
        img.save(buf, 'PNG', optimize=True)
    buf.seek(0)
    return buf
