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
It does not claim that two distinct application IDs exist. Claude Science uses
the official Claude family mark until a separate verified Science mark is
available. It is not given a fabricated science symbol.

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

Windows Settings and Notepad use theme-aware native vector command glyphs: a
gear and a bound notepad. `IdentifySystemIcon` recognizes actual `ms-settings:`
URIs, executable basenames, and known packaged identifiers. It does not infer
system identity from localized display names or arbitrary substrings. The
renderer passes the real target to this system-icon path before falling back to
a neutral application window. Chinese or user-renamed entries therefore retain
their system glyph without introducing guessed vendor marks.

`DrawAppSymbol` now draws only those system symbols or the neutral fallback.
The earlier approximate ChatGPT, Claude, Gemini, Codex, and Perplexity shapes are
removed; verified vendor artwork is the sole branded rendering source.

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

`PreloadOfficialIconAssets()` runs once on the COM-initialized UI thread before
the first interaction. It reads all artwork and decodes PNGs with Windows
Imaging Component. Oversized PNGs are downsampled to a maximum 256-pixel edge;
the application never decodes images or reads artwork files during a draw.

`DrawOfficialIcon()` uses `ID2D1DeviceContext5::DrawSvgDocument` for SVG features
supported by Direct2D. Some official assets, including VS Code and Antigravity,
contain SVG masks or filters that Direct2D does not implement. Those assets use
their official PNG, rather than silently rendering a damaged or incomplete SVG.
Before drawing a supported SVG, its cached in-memory root width and height are
normalized to numeric viewBox dimensions. This prevents physical units such as
Perplexity's `270pt` root from expanding to 360 pixels and overflowing the shared
optical box. The source SVG stays byte-for-byte unchanged. Native WARP/SVG tests
verify that Perplexity and the Gemini PNG stay inside a 28-DIP rectangle and
remain centered.

Legacy DC render targets also use the PNG fallback. The wheel geometry, text,
selection rings, system glyphs, and animation remain vector-based.

GPU images are cached by the canonical Direct2D device identity, not by the
transient device-context pointer returned by a composition surface's
`BeginDraw`. Legacy targets have a cache keyed by their canonical COM identity.
Each cache retains its owner so a recycled pointer cannot accidentally match a
stale resource. `ReleaseOfficialIcons(nullptr)` clears all device resources on
device loss or shutdown; passing a target clears only that resource domain.

Missing or unverified artwork returns `false` and uses a neutral application
glyph. It must never be replaced by an invented branded logo.

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
