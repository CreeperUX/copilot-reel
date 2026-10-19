# VI source package v0.5

The selected scheme is `orbit` (Soft Orbit). Application v0.2.0 implements a primary quick menu, secondary Settings, focused app editing, a separate Advanced sheet, and shared monochrome vector symbols. Edge placement preserves the full circle, applies minimal translation, and waits for center entry when necessary. Small work areas use a scrollable OSD fallback.

- `vectors/`: 26 SVGs; 12 comparison/brand variants, 4 Soft Orbit material variants, and 10 OSD screen/theme variants.
- `tokens.json`: English semantic design tokens and explicit implementation status.
- `stylesheet.typ`: English style specification table; supplied as editable source, not used to generate the PDF.
- `source/scene.py`: shared vector drawing primitives.
- `source/osd_pages.py`: English OSD and edge-layout specification pages.
- `source/build_vi.py`: PDF, SVG, and token generator.
- [Review PDF](../../output/pdf/copilot-reel-vi-v0.5.pdf): 12 pages.

## Rebuild

From the repository root on Windows:

```powershell
python design/vi/source/build_vi.py
```

The generator requires Python, reportlab, and the Windows Segoe UI / Microsoft YaHei fonts. Use Poppler `pdftoppm` to generate PNG review snapshots.

App symbols use the shared native visual language: a 40 DIP wheel tile, 28 DIP mark, and 1.75 DIP stroke. Known identities receive the corresponding symbol; unknown apps use a neutral application-window mark. Selection uses the Windows accent or high-contrast highlighted text. No shell bitmap or bare-initial fallback is used. Runtime UI uses Direct2D geometry and DirectWrite text; the PDF or PNG is never the UI substrate. The OSD scenes illustrate hierarchy and layout; native caption, standard controls, and system typography can vary. Actual backdrop effects are an independent compositor concern with an opaque fallback. Validation is tracked in `docs/validation.md`.
