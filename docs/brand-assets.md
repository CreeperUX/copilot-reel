# Official application artwork and preset discovery

Copilot Reel uses vendor artwork to identify the applications the user can launch.
The common visual treatment belongs to the surrounding tile: size, padding,
alignment, corner radius, hover feedback, and the Windows accent-color ring.
The vendor's silhouette, internal colors, and proportions are preserved.

## First preset catalog

| Preset | Verified source | Desktop resolution |
| --- | --- | --- |
| ChatGPT / Codex | Installed OpenAI package manifest and icons | Registered `OpenAI.Codex_2p2nqsd0c76g0!App`; otherwise an installed ChatGPT shortcut |
| Claude Desktop | Installed Anthropic package manifest and icon | Registered `Claude_pzs8sxrjxfjjc!Claude`; otherwise an installed Claude shortcut |
| Claude Science | [Official product page](https://claude.com/product/claude-science) | Exact installed Claude Science or Claude for Science shortcut |
| Antigravity | [Google press assets](https://antigravity.google/press) | Installed shortcut or existing per-user Antigravity executable |
| ZCode | [Official installation guide](https://zcode.z.ai/en/docs/install), installed app artwork | Installed shortcut or existing per-user ZCode executable |
| DeepSeek Harness (DSH) | [Official desktop download](https://www.deepseek.com/en/download/), [official source repository](https://github.com/deepseek-ai/deepseek-harness) | Exact installed DeepSeek Harness or DSH shortcut |
| Visual Studio Code | [Microsoft brand assets](https://code.visualstudio.com/brand) | Installed shortcut or existing per-user VS Code executable |
| JetBrains IDE | [JetBrains product artwork](https://www.jetbrains.com/company/brand/) | Installed IntelliJ IDEA, PyCharm, WebStorm, Rider, CLion, GoLand, PhpStorm, RubyMine, RustRover, or Air shortcuts; names retain their product/version |
| Junie | [Official product site](https://junie.jetbrains.com/) and JetBrains artwork | Installed Junie or Junie Local shortcut, otherwise the user chooses the actual host |

`AgentPresets()` does not replace saved entries. A missing desktop target is an
empty `AppTarget.target`; the caller must open the editor or file picker before
saving it. It must not silently open a marketing website, invent a protocol,
execute `npx`, install an application, or claim that an IDE has Junie installed.
The returned catalog can exceed eight entries when several JetBrains IDEs are
installed. The picker must paginate it; the active wheel still has its own limit.

The OpenAI package's legacy name contains `Codex`, but its manifest names the
application ChatGPT. The combined preset uses that verified application ID.
It does not claim that two distinct application IDs exist. An installed Claude
Science shows the mark embedded in its own executable (Anthropic, PBC) as its
Shell icon; an unresolved Claude Science preset uses the Claude family mark. It
is not given a fabricated science symbol.

DSH is resolved to **DeepSeek's own DeepSeek Harness desktop**, not a similarly
named community wrapper. The official download page links to Windows and macOS
builds; the upstream repository identifies the desktop package as
`@deepseek-ai/dsh-desktop` and contains its Windows icon. No DSH installation was
found in this development machine's Start menu, Appx package list, or standard
per-user/machine uninstall records at verification time.

## Existing defaults and Windows system targets

The saved default catalog also includes Gemini, Perplexity, Windows Settings,
and Notepad. Gemini uses Google's 512-pixel sparkle artwork linked by
[gemini.google.com](https://gemini.google.com/). Perplexity uses its own
[favicon SVG](https://www.perplexity.ai/favicon.svg) and
[PNG fallback](https://www.perplexity.ai/favicon.png), both advertised by its
[official application page](https://www.perplexity.ai/). These source files are
preserved byte-for-byte and included in the same startup asset cache.

Windows Settings, Notepad and every custom application without vendor artwork
use their own Windows Shell icon (`SHCreateItemFromParsingName` +
`IShellItemImageFactory::GetImage`, 256 px, icon only): `ms-settings:` targets use
the Settings package identity, Notepad its package, and executables or shortcuts
their resolved file. Icons are extracted once per target while the configuration
loads (`PrepareArtwork`), never during a draw. `IdentifySystemIcon` still
recognizes actual system destinations for localized display names; it never
infers identity from display names or arbitrary substrings.

Vendor artwork is chosen by the launch target alone (`IdentifyBrand`): the package
family of an AUMID, the file name of an executable or shortcut together with its
install folder, or a URL host. A profile folder such as `C:\Users\<name>` never
counts, and the display name is consulted only while a preset has no target yet.
Renaming an app therefore cannot change its artwork, and a user named Claude
does not stamp the Anthropic mark on unrelated applications.

The earlier approximate ChatGPT, Claude, Gemini, Codex, and Perplexity shapes and
the hand-drawn Settings and Notepad glyphs are removed from the reel; verified
vendor artwork and Shell icons are its only application images. The focused
editor's list keeps neutral line glyphs (`DrawAppSymbol`).

## Files and provenance

[The machine-readable manifest](../assets/brands/provenance.json) records the
SHA-256 digest, exact source, owner, and processing for every artwork file.
GitHub sources are pinned to the retrieved commit. Installed application assets
retain their package version and internal resource path. No asset comes from
an unofficial logo aggregator or a hand-drawn approximation.

PNG variants for JetBrains products without an upstream PNG in the selected
resource set were rasterized from the adjacent original SVG at 256 x 256 with
`sharp`. This conversion does not recolor, crop, round, or redraw the brand mark.
The original SVG remains in the repository and package.

## Native rendering and resource lifetime

`PreloadOfficialIconAssets()` reads the PNG of every brand listed in
`design/vi/tokens.json` (`iconography.brands`) once on the UI thread. The original
SVGs stay in the repository and package for provenance; the reel renders the
official PNGs, which avoids partial rendering of SVG masks and filters that
Direct2D does not implement.

Every reel item belongs to one artwork class (`src/artwork.h`):

| Class | Assets | Treatment |
| --- | --- | --- |
| Plated | Claude, Perplexity, ZCode, DeepSeek Harness, JetBrains Air | The vendor plate replaces the neutral tile: cropped to the plate's own bounds (DeepSeek Harness drops its baked shadow margin), scaled to cover the 40 DIP tile and masked to the tile's 10 DIP corner radius, with the tile hairline on top. |
| Glyph | ChatGPT, Gemini, VS Code, Antigravity, the JetBrains IDE family, Junie, Toolbox, Shell icons | Centered on the neutral tile. The ink bounding box fits a 28 DIP keyline times a per-brand optical scale (0.86 for the dense JetBrains product squares, 1.04 for the Gemini sparkle), so marks read at equal visual weight. |
| Command | Internal reel actions | Segoe Fluent Icons glyphs at 24 DIP in the foreground color. |
| Generic | No artwork | A neutral window glyph (Segoe Fluent Icons E737). |

The class, optical scale and content rectangle of each brand are tokens, measured
from the artwork's alpha (coverage of its bounding box and match with a rounded
rectangle) and tuned on native contact sheets. Shell icons are classified by the
same alpha analysis at run time.

Masking a plated mark to the tile shape is the only geometric treatment. Marks are
never recolored, redrawn, distorted, or placed inside a second plate. Light and
dark variants are used only where the vendor ships them (ChatGPT); in high
contrast the variant follows the brightness of the system window color.

Raster artwork is pre-scaled with WIC high-quality cubic filtering to the exact
device-pixel size for the current DPI and drawn 1:1 on the pixel grid; any other
size uses Direct2D high-quality cubic interpolation. Caches are keyed by source
and pixel size, held per Direct2D device, and released on DPI change and device
loss (`ReleaseArtwork`).

Missing or unverified artwork uses the neutral glyph. It must never be replaced by
an invented branded logo.

## Vendor notices

All names and artwork identify their respective vendors and products. Copilot
Reel is an independent launcher and does not imply sponsorship or endorsement.

- OpenAI and ChatGPT marks belong to OpenAI. See the [OpenAI design guidelines](https://openai.com/brand/).
- Claude marks belong to Anthropic. The source image is the unmodified official Windows application resource.
- Antigravity artwork belongs to Google and comes from its [press asset collection](https://antigravity.google/press).
- Gemini artwork belongs to Google and comes from the official Gemini application page.
- Perplexity artwork belongs to Perplexity and comes from its official application favicon resources.
- ZCode artwork belongs to Z.ai and comes from the installed official Windows application resource.
- DeepSeek Harness is a DeepSeek trademark. Its source repository is MIT licensed; the [license](../assets/brands/deepseek-LICENSE.txt) is retained. Its [brand guidelines](https://github.com/deepseek-ai/deepseek-harness/blob/master/BRAND_GUIDELINES.md) prohibit misleading claims of affiliation.
- Visual Studio Code artwork belongs to Microsoft. Its [brand guidelines](https://code.visualstudio.com/brand) prohibit redrawing or restyling the icon, so its internal artwork remains unchanged.
- Copyright © 2026 JetBrains s.r.o. JetBrains, Junie, IntelliJ IDEA, PyCharm, WebStorm, Rider, CLion, GoLand, PhpStorm, RubyMine, RustRover, Air, Toolbox, and their logos are trademarks of JetBrains s.r.o. See [JetBrains](https://www.jetbrains.com/), its [brand guidelines](https://www.jetbrains.com/company/brand/), and the retained [asset license](../assets/brands/jetbrains-LICENSE.txt). Product icons identify products; the company icon identifies the generic JetBrains IDE picker entry.

The repository's source-code license does not replace these vendor trademark
terms or grant additional rights to the bundled artwork.
