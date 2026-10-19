# VI source package v0.6

Soft Orbit is the selected scheme. This revision replaces the rectangular primary OSD with nested radial menus, defines in-place language arrows and A / D, distinguishes short-tap preferred-agent launch from hold-to-show, and adds pointer-led reveal/reverse motion. Official vendor artwork replaces invented brand approximations inside consistent tiles.

- `vectors/`: active radial main, Settings, language, preferred-agent, manage, and preset masters in both themes; material and high-contrast variants; historical visual-direction comparisons.
- `tokens.json`: English semantic design tokens and input/material/artwork policies.
- `stylesheet.typ`: editable English style table; not the PDF generator.
- `source/scene.py`: vector and official-artwork drawing primitives.
- `source/radial_pages.py`: 12 English specification pages.
- `source/build_vi.py`: PDF, SVG, and token generator.
- [Review PDF](../../output/pdf/copilot-reel-vi-v0.6.pdf).

## Rebuild

```powershell
python design/vi/source/build_vi.py
```

Requires Python, reportlab, and Windows Segoe UI / Microsoft YaHei fonts. Render PDF review snapshots with Poppler `pdftoppm`. All geometry and text remain vector in the PDF. Official PNG source marks may be embedded; SVG scene exports use the corresponding source SVG when present. Neither the PDF nor PNG review pages become application UI frames.

The generator retires `osd-*.svg` rectangular primary-panel masters from the active vector directory. Older PDF versions are historical reviews, not the current design contract.

The runtime uses native geometry, DirectWrite, official-artwork caches, and Windows Composition animation properties. Asset ownership and source details live in `assets/brands/provenance.json`. Backdrop behavior, physical input timing, and high-refresh performance remain separate runtime validation items in `docs/validation.md`.
