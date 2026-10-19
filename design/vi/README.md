# VI source package v0.3

The selected scheme is `orbit` (Soft Orbit). The OSD extension provides a primary quick menu, secondary Settings, and focused app editing. Edge half-orbit/fan layouts are specifications awaiting implementation.

- `vectors/`: 26 SVGs; 12 comparison/brand variants, 4 Soft Orbit material variants, and 10 OSD screen/theme variants.
- `tokens.json`: English semantic design tokens and explicit implementation status.
- `stylesheet.typ`: English style specification table; supplied as editable source, not used to generate the PDF.
- `source/scene.py`: shared vector drawing primitives.
- `source/osd_pages.py`: English OSD and edge-layout specification pages.
- `source/build_vi.py`: PDF, SVG, and token generator.
- [Review PDF](../../output/pdf/copilot-reel-vi-v0.3.pdf): 10 pages.

## Rebuild

From the repository root on Windows:

```powershell
python design/vi/source/build_vi.py
```

The generator requires Python, reportlab, and the Windows Segoe UI / Microsoft YaHei fonts. Use Poppler `pdftoppm` to generate PNG review snapshots.

App symbols are placeholders. Runtime UI uses native geometry, DirectWrite text, and original app icons; the PDF or PNG is never the UI substrate. Actual backdrop effects are an independent compositor concern with an opaque fallback.
