# Windows font provider for Vulkan UI

RenderFontWin32 implements U++'s existing CFONTS platform-font hook using
Windows DirectWrite. It replaces font enumeration, metrics, OpenType table/file
access and glyph outline extraction without modifying the U++ Draw checkout.
The renderer and Ui control tree retain their existing Font API and fallback
authority. Windows supplies dwrite.dll; application users do not install a
font SDK or a Rust toolchain.

## Assembly

Add RenderFontWin32 to an application's uses list and enable CFONTS for the
whole assembly. GpuUiGallery and UiGpuDrawingTest accept it through conditional
uses; their default configuration includes GUI GPUUI CFONTS. Explicit builds
without CFONTS retain U++'s original platform provider.

Do not link two CFONTS providers into one assembly. The package is Windows only.
CoreText and browser font hosting need their own platform implementations later.

Example current build flags: `+GUI,GPUUI,CFONTS`.
Focused console tests: RenderFontWin32Test and RenderVulkanTextDirectWriteTest,
both built with `+CONSOLE,CFONTS`.

## Contract and bounds

- U++ generic/symbol face indices remain stable. Missing family names resolve
  through DirectWrite's Arial fallback; scalar fallback/composition remains
  U++'s existing font logic.
- DirectWrite supplies grid-compatible integer metrics and unhinted outlines.
  The API name GetGdiCompatibleMetrics describes the metric convention; these
  operations do not create an HDC or call GDI text/outline functions.
- At most 64 opened font faces are retained, with least-recently-used eviction.
  The operating-system font-family inventory and DirectWrite's shared services
  are separate from that opened-face cache.
- Font table/file reads validate offsets and return at most 64 MiB per call.
  Collection font indices are preserved in CommonFontInfo.
- A bounded 64-entry native HFONT compatibility cache exists because ordinary
  CtrlCore SystemDraw still links that entry point. Every invocation increments
  legacy_gdi_font_requests. Current strict Gallery/test qualification requires
  zero such requests; CFONTS alone does not prohibit native drawing in other apps.
- Font/cache diagnostics and explicit cleanup are exposed without COM types.
  Font names resolve before taking the provider lock, preserving the U++ font
  lock order. Concurrent metric/outline/diagnostic access is tested.

## What text rendering means here

The current neutral DrawText contract preserves U++ control text: per-scalar
integer Font advances, U++ fallback and decorations. Software replay supplies
those explicit advances to Painter; otherwise Painter implicitly measures a
larger font and uses different fractional spacing.

DirectWrite supplies outlines and metrics; compute-capable Vulkan devices now
generate grayscale glyph coverage directly on the GPU. CPU work prepares bounded
edges, without ImagePainter or an intermediate pixel image. Fixed 32-sample
vertical coverage integrates horizontal spans; the rare crossing-overflow case
uses a bounded 32 x 32 grid. Glyph masks remain cached in bounded atlas pages.
Cold glyphs share storage-buffer ranges and at most 128 dispatches per batch;
warm frames perform no glyph coverage submissions. Devices without optional
compute retain an explicitly counted CPU reference.

Fallback/composed characters resolve through U++'s existing GlyphInfo/Compose
authority. Base and diacritic components composite independently and share their
ordinary glyph entries. A cold-whitespace bug was fixed: empty outlines now
return the same advance on their first and cached frames.

The existing scalar contract is deliberate; it does not provide full shaped
glyph runs, Arabic/Indic shaping, bidi or colour emoji. IME/accessibility and
monitor-DPI acceptance remain additional work. Individual Unicode tests do not
qualify those features.

## GPU glyph qualification — 2026-10-15

Release and Debug pass the Unicode readback test in all four RGBA/BGRA
UNORM/sRGB formats, with zero CPU glyph rasters, ordinary pixel uploads or GDI
font requests. The existing Unicode tolerance is unchanged: maximum 0.03,
mean 0.001. Observed maxima are 0.01569 (UNORM) / 0.01884 (sRGB).

The larger printable-character fixture generates 222 cold glyph masks in three
compute submissions. Cold/warm pixels are identical, and a failed entry-budget
frame discards queued writes and recovers on the next valid frame. Six glyph
crossing pixels differ from Painter's legacy area accumulator; independent
128 x 128 double-precision polygon sampling validates them with maximum error
0.005124. This declared legacy difference is separate from the original tight
Unicode test, whose tolerance remains unchanged.

See [GPU coverage](GPU_VECTOR_COVERAGE.md) for bounds, cache semantics and the
qualification manifest. These results remove CPU glyph coverage for the tested
Vulkan path; they do not complete the shaped-text gate.

## Historical CPU-glyph evidence — 2026-10-11

The font test covers line/glyph metrics, compound B outlines, Latin, Greek,
Cyrillic, Han and supplementary Unicode fallback, invalid scalars, OpenType
table/file bounds, decorated text coverage, cache churn/cleanup/recovery and
four-thread metric/outline/diagnostic access. It reports zero legacy GDI font
requests and a maximum of 64 cached faces.

The Vulkan text test uses actual texture readback in RGBA/BGRA UNORM/sRGB.
Aligned Unicode glyph masks match an independently painted reference exactly
in UNORM. The sRGB maximum linear coverage difference is 0.004977 (encoding
quantization), with mean 0.000055. Underline/strikeout change actual GPU pixels
while reusing the glyph atlas. Both providers run the same pixel tests.
Vulkan validation and complete native resource cleanup remain required.

Exact artifacts/jobs and whole-UI integration results are recorded in
`build/vulkan-ui-qualification-2026-10-11.json`; see VULKAN_UI_FINISHING.md for the
current application scope and remaining drawing work.

DirectWrite reference:
[GetGlyphRunOutline](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritefontface-getglyphrunoutline).
