"""Document chrome for the VI pages: page frame, type styles, panels, redlines, tags.

The page is 1440 x 900 layout units (PDF at 2/3: 960 x 600 pt). Product values never
appear here; this file only styles the document around the product scenes.
"""
from __future__ import annotations

import math

import fonts
from scene import Scene
from tokens import VERSION, APP_VERSION

W, H = 1440, 900
PDF_SCALE = 2 / 3
ML = 72
MR = 72
CW = W - ML - MR
TOP = 192
BOTTOM = 812

# Document palette (chrome only): neutral ink, Mica-like panels, magenta redlines.
INK, INK2, INK3 = '#1B1B1B', '#5D5D5D', '#8A8A8A'
RULE, PANEL, PANEL_DARK, PAPER = '#E5E5E5', '#F3F3F3', '#202020', '#FFFFFF'
RED = '#C30052'
DO, DONT = '#0F7B0F', '#C42B1C'
TOTAL = [0]


class Page:
    def __init__(self, number, kicker, title, lede=''):
        self.number = number
        s = self.s = Scene(W, H)
        s.rect(0, 0, W, H, fill=PAPER)
        s.text(ML, 60, f'{number:02d}', 12, INK, face='small-sb', tracking=1.2)
        s.text(ML + 30, 60, kicker.upper(), 12, INK2, face='small-sb', tracking=1.2)
        s.text(ML, 104, title, 34, INK, face='display')
        if lede:
            s.text(ML, 140, lede, 16, INK2, face='text')

    def finish(self, total):
        s = self.s
        s.line(ML, 836, W - MR, 836, RULE, 1)
        s.text(ML, 862, 'Copilot Reel  ·  Visual identity v' + VERSION, 11, INK3, face='small')
        s.text(W / 2, 862, f'Describes the native {APP_VERSION} build  ·  values from design/vi/tokens.json', 11, INK3,
               face='small', align='center')
        s.text(W - MR, 862, f'{self.number:02d} / {total:02d}', 11, INK3, face='small', align='right')
        return s


def para(s, x, y, text, width, size=14, leading=None, color=INK2, face='text'):
    """Wrapped paragraph from baseline y; returns the baseline after the last line."""
    leading = leading or round(size * 1.5)
    for i, line in enumerate(fonts.wrap(text, width, face, size)):
        s.text(x, y + i * leading, line, size, color, face=face)
    return y + len(fonts.wrap(text, width, face, size)) * leading


def heading(s, x, y, text, size=15, color=INK):
    s.text(x, y, text, size, color, face='text-sb')


def caption(s, x, y, text, color=INK2, align='left', size=12, face='small'):
    s.text(x, y, text, size, color, face=face, align=align)


def mono(s, x, y, text, color=INK2, align='left', size=12):
    s.text(x, y, text, size, color, face='mono', align=align)


def panel(s, x, y, w, h, dark=False, fill=None, r=12):
    s.rect(x, y, w, h, fill=fill or (PANEL_DARK if dark else PANEL), r=r)


def tag(s, x, y, text, kind='vector', dark=False):
    """Small provenance pill: 'native' (runtime capture), 'vector' (VI scene), 'tonal' (illustration)."""
    colors = {'native': ('#0F7B0F', '#E7F3E7'), 'vector': ('#5D5D5D', '#EDEDED'), 'tonal': ('#8A5A00', '#FFF4CE')}
    fg, bg = colors[kind]
    if dark:
        fg, bg = {'native': ('#6CCB5F', '#1F3320'), 'vector': ('#CFCFCF', '#3A3A3A'), 'tonal': ('#FCE100', '#433519')}[kind]
    w = fonts.width(text.upper(), 'small-sb', 'cjk', 9.5, 0.8) + 14
    s.rect(x, y - 12, w, 17, fill=bg, r=8.5)
    s.text(x + 7, y, text.upper(), 9.5, fg, face='small-sb', tracking=0.8)
    return w


# Redlines -----------------------------------------------------------------------------------

def _arrow(s, x, y, angle, color, size=4.5, sw=0.9):
    a = math.radians(angle)
    for d in (150, -150):
        b = a + math.radians(d)
        s.line(x, y, x + size * math.cos(b), y + size * math.sin(b), color, sw, cap='round')


def dim_h(s, x1, x2, y, text, color=RED, above=True, ext=None, size=11):
    """Horizontal dimension with end ticks and centered text."""
    s.line(x1, y, x2, y, color, 0.9)
    for x in (x1, x2):
        s.line(x, y - 4, x, y + 4, color, 0.9)
    if ext:
        for x in (x1, x2):
            s.line(x, ext, x, y, color, 0.6, dash=[2, 2])
    s.text((x1 + x2) / 2, y - 5 if above else y + 13, text, size, color, face='small-sb', align='center')


def dim_v(s, x, y1, y2, text, color=RED, left=False, size=11):
    s.line(x, y1, x, y2, color, 0.9)
    for y in (y1, y2):
        s.line(x - 4, y, x + 4, y, color, 0.9)
    s.text(x - 6 if left else x + 6, (y1 + y2) / 2 + 4, text, size, color, face='small-sb', align='right' if left else 'left')


def leader(s, x1, y1, x2, y2, text, color=RED, align=None, size=11, dot=True):
    """A callout line from (x1, y1) on the object to (x2, y2) with text after the end."""
    s.line(x1, y1, x2, y2, color, 0.8)
    if dot:
        s.circle(x1, y1, 1.8, fill=color)
    align = align or ('left' if x2 >= x1 else 'right')
    tx = x2 + 4 if align == 'left' else x2 - 4
    lines = text.split('\n')
    for i, line in enumerate(lines):
        s.text(tx, y2 + 4 + i * 14, line, size, color, face='small-sb', align=align)


def radius_line(s, cx, cy, r, angle, text, color=RED, text_at=0.55, offset=10):
    a = math.radians(angle)
    s.line(cx, cy, cx + r * math.cos(a), cy + r * math.sin(a), color, 0.9)
    s.circle(cx, cy, 1.8, fill=color)
    _arrow(s, cx + r * math.cos(a), cy + r * math.sin(a), angle, color)
    tx, ty = cx + r * text_at * math.cos(a), cy + r * text_at * math.sin(a)
    nx, ny = -math.sin(a), math.cos(a)
    s.text(tx + nx * offset, ty + ny * offset + 4, text, 11, color, face='small-sb', align='center')


def check_mark(s, x, y, ok=True, size=16):
    col = DO if ok else DONT
    s.circle(x, y, size / 2, fill=col)
    k = size / 16
    if ok:
        s.path([('M', x - 4 * k, y), ('L', x - 1.2 * k, y + 2.8 * k), ('L', x + 4.2 * k, y - 3 * k)], stroke='#FFFFFF', sw=1.8 * k)
    else:
        s.line(x - 3.2 * k, y - 3.2 * k, x + 3.2 * k, y + 3.2 * k, '#FFFFFF', 1.8 * k, cap='round')
        s.line(x + 3.2 * k, y - 3.2 * k, x - 3.2 * k, y + 3.2 * k, '#FFFFFF', 1.8 * k, cap='round')
