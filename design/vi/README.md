# VI source package v0.9

Soft Orbit for Copilot Reel 0.3.4: a Windows-neutral plate with one flyout shadow, a concentric selection seat that replaces the tile fill and travels along the orbit, official artwork in one tile system (plated marks fill the tile; open marks share one optical size; system apps use their Shell icons), Segoe Fluent Icons commands, and drill-in/out page transitions in which the tapped tile becomes the back chevron. The center stays quiet: one name on hover, never a caption or helper copy.

- `tokens.json`: the single, hand-edited source of design values (geometry, palettes and accent roles, material, motion, typography, per-brand artwork treatment). `tools/gen-vi-tokens.ps1` generates `src/vi_tokens.h` from it for the application; this package reads it directly.
- `vectors/`: active SVG masters on the 384 DIP canvas: `radial-{main,hub,language,preferred,manage,presets}-{light,dark}.svg`, `radial-{main,hub}-contrast.svg`, `radial-main-{light,dark}-frosted.svg`.
- `stylesheet.typ`: Typst token sheet that reads `tokens.json`; it repeats no values.
- `source/build_vi.py`: builds the PDF, the SVG masters, the 2x previews and the review zip.
- `source/pages.py`: the 15 specification pages; `source/reel.py`: vector mirror of `src/renderer.cpp`; `source/scene.py`, `art.py`, `fonts.py`, `tokens.py`, `layout.py`, `masters.py`: drawing, artwork, font, token, layout and master helpers.
- [Review PDF](../../output/pdf/copilot-reel-vi-v0.9.pdf) and [normative summary](../../docs/visual-identity.md).

## Rebuild

```powershell
uv run --python 3.14 --with reportlab --with pillow --with fonttools --with numpy python design/vi/source/build_vi.py
```

`--pages 2,7` rebuilds selected pages for review. The build needs the Windows fonts Segoe UI Variable, Segoe Fluent Icons, Microsoft YaHei UI and Consolas, and renders previews with Poppler `pdftoppm` or PyMuPDF. It never writes `tokens.json`.

Scenes are vector and use the token accent fallbacks; frosted scenes are labelled tonal illustrations. Official vendor PNGs are embedded unmodified, with plated marks clipped to the tile shape exactly as the application does. Pages 12 and 15 show native captures from `output/runtime/v0.3.4/`. Neither the PDF nor the PNG previews ever become application UI frames. Older PDF versions are historical reviews, not the current design contract.

Asset ownership and sources live in `assets/brands/provenance.json`. The live frosted backdrop, real High Contrast themes, physical Copilot-key timing and high-refresh performance remain runtime validation items in `docs/validation.md`.
