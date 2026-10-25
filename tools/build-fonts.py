"""Build Reel Sans JP, the Japanese face the reel bundles, from Noto Sans JP.

Noto Sans JP (Source Han Sans, SIL Open Font License 1.1) is the open face closest to Apple's
Hiragino Sans: no uroko at stroke shoulders, wide counters, an even stroke rhythm and slightly
enlarged kana (docs/visual-identity.md, Typography). The reel needs two weights of it, so this
script makes two static instances, 400 and 600, and subsets each to:

- every character of Windows-31J (JIS X 0208 levels 1 and 2 with the NEC and IBM extensions),
- the kana blocks, CJK symbols and punctuation, and the half- and fullwidth forms,
- printable ASCII, and every character of the Japanese interface strings in src/.

A subset is a Modified Version under the OFL, so it is renamed (the license reserves only
"Source"; the new name also keeps it apart from a system Noto Sans JP) and keeps the copyright
and license records. Its line metrics are set to Segoe UI's, so a Japanese label's line is as
tall as an English one. assets/fonts/OFL.txt travels with it.

The source is the variable Noto Sans JP 2.004 that Windows 11 installs (KB5053656) or the same
file from https://github.com/google/fonts/tree/main/ofl/notosansjp; its SHA-256 is pinned.

    uv run --python 3.14 --with fonttools python tools/build-fonts.py [--source NotoSansJP-VF.ttf]
"""
import argparse
import hashlib
import re
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'assets/fonts'
SOURCE_SHA256 = '5113756F8A3B5D01B2211025E267C50121E3B36F465B7BBAF3CDAF4C3430BFD0'
FAMILY, POSTSCRIPT = 'Reel Sans JP', 'ReelSansJP'
WEIGHTS = {400: 'Regular', 600: 'SemiBold'}
# Segoe UI's Windows metrics (2210 and 514 of 2048 units) in Noto's 1000-unit em.
ASCENT, DESCENT = 1079, 251
BLOCKS = [(0x3000, 0x303F), (0x3040, 0x309F), (0x30A0, 0x30FF), (0x31F0, 0x31FF), (0xFF00, 0xFFEF)]


def windows_31j():
    characters = set()
    for lead in list(range(0x81, 0xA0)) + list(range(0xE0, 0xFD)):
        for trail in list(range(0x40, 0x7F)) + list(range(0x80, 0xFD)):
            try:
                characters.update(bytes([lead, trail]).decode('cp932'))
            except UnicodeDecodeError:
                pass
    characters.update(bytes([b]).decode('cp932') for b in range(0xA1, 0xE0))
    return characters


def interface_strings():
    """The Japanese (third) argument of every Tr call in src/."""
    literal = r'L"((?:[^"\\]|\\.)*)"'
    call = re.compile(r'\bTr\(\s*' + literal + r'\s*,\s*' + literal + r'\s*,\s*' + literal + r'\s*\)')
    found = set()
    for path in sorted((ROOT / 'src').glob('*.*')):
        for match in call.finditer(path.read_text(encoding='utf-8')):
            found.update(match.group(3))
    return found


def rename(font, weight, style):
    full = FAMILY if style == 'Regular' else f'{FAMILY} {style}'
    postscript = f'{POSTSCRIPT}-{style}'
    names = font['name']
    version = names.getDebugName(5) or ''
    kept = {0, 7, 13, 14}
    names.names = [record for record in names.names if record.nameID in kept]
    records = {1: full, 2: 'Regular', 3: f'2.004;REEL;{postscript}', 4: full, 5: version.split(';')[0] + '; Reel subset',
               6: postscript, 10: 'A subset of Noto Sans JP, renamed for Copilot Reel under the SIL Open Font License 1.1.'}
    if style != 'Regular':
        records.update({16: FAMILY, 17: style})
    for name_id, text in records.items():
        names.setName(text, name_id, 3, 1, 0x409)
    os2 = font['OS/2']
    os2.usWeightClass = weight
    os2.fsSelection = (os2.fsSelection & ~0b1100001) | (0b1000000 if style == 'Regular' else 0)
    font['head'].macStyle = 0
    os2.usWinAscent, os2.usWinDescent = ASCENT, DESCENT
    font['hhea'].ascent, font['hhea'].descent, font['hhea'].lineGap = ASCENT, -DESCENT, 0
    for table in ('STAT', 'meta', 'DSIG'):
        if table in font:
            del font[table]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--source', type=Path, default=Path(r'C:\Windows\Fonts\NotoSansJP-VF.ttf'))
    args = parser.parse_args()
    digest = hashlib.sha256(args.source.read_bytes()).hexdigest().upper()
    if digest != SOURCE_SHA256:
        raise SystemExit(f'{args.source} is not Noto Sans JP 2.004 (SHA-256 {digest}).')
    wanted = windows_31j() | set(map(chr, range(0x20, 0x7F))) | interface_strings()
    for first, last in BLOCKS:
        wanted.update(map(chr, range(first, last + 1)))
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for weight, style in WEIGHTS.items():
        font = instancer.instantiateVariableFont(TTFont(args.source), {'wght': weight})
        cmap = font.getBestCmap()
        missing = sorted(c for c in interface_strings() if ord(c) not in cmap and not c.isspace())
        if missing:
            raise SystemExit(f'Noto Sans JP lacks interface characters: {"".join(missing)}')
        options = subset.Options()
        options.name_IDs = ['*']
        options.name_languages = ['*']
        options.notdef_outline = True
        options.glyph_names = False
        options.hinting = False
        # Horizontal interface text: fontTools' default features without the vertical forms,
        # which would keep every alternate glyph the subset drops anyway.
        options.layout_features = [f for f in options.layout_features if f not in ('vert', 'vrt2', 'vkrn', 'vpal', 'valt', 'vhal')]
        subsetter = subset.Subsetter(options)
        subsetter.populate(unicodes=[ord(c) for c in wanted if ord(c) in cmap])
        subsetter.subset(font)
        rename(font, weight, style)
        path = OUTPUT / f'{POSTSCRIPT}-{style}.ttf'
        font.save(path)
        print(f'{path.relative_to(ROOT)}: {len(font.getBestCmap())} characters, {path.stat().st_size / 1e6:.2f} MB')


if __name__ == '__main__':
    main()
