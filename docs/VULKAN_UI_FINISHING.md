# Windows/Vulkan whole-UI finishing

Updated 2026-10-11. This is implementation and qualification evidence for the
current direct Vulkan renderer. It is not a claim that all CPU font/vector work
has been removed. WebGPU and Metal follow this Windows work.

## Drawing implemented

- Solid convex fills, closed miter strokes and straight butt/square strokes use device-space coverage
  triangles. Curves flatten adaptively within a 0.35 device-pixel control-hull
  bound. Topology, finite coordinates, point count and stroke transforms are
  checked before accepting the fast path.
- Exact integer translations may reuse a GPU-rendered RGBA8 coverage atlas.
  Fractional translations use direct coverage triangles, avoiding a second AA
  filter. Geometry/coverage keys exclude colour and translation; tint and
  painter order remain draw state.
- GPUUI lets Ui's common rounded faces/borders retain semantic geometry instead
  of creating cached CPU images. Unsupported helper geometry uses the existing
  reference path. The optional interface is header-only; ordinary Ui assemblies need neither the
  render nest nor a renderer link dependency. Native UiLabelDemo builds without
  GPUUI or the render nest.
- Whole-window hosts hold a nested GUI-thread paint guard. It performs one cold
  ImageDraw probe, rather than a probe/backbuffer per recorded frame, and the
  last host restores the inherited U++ GlobalBackBuffer setting. Applications
  must not change that process-wide setting while guards own it.
- The CFONTS configuration uses RenderFontWin32/DirectWrite for font inventory,
  metrics, data and outlines. Gallery, populated Ui and text pixel qualification
  require zero legacy GDI font requests. Face retention is bounded at 64 entries.
  Software and Vulkan text now share the explicit U++ integer-advance contract.
  Glyph cache misses still prepare CPU coverage; see WINDOWS_FONT_PROVIDER.md.
- The shared Ui table now preserves editing when focus moves into its own
  inline editor. Its editor blur handler still commits when focus leaves.

## Memory and scheduling contract

Default retained payload budgets per renderer:

| Resource | Default |
| --- | ---: |
| Image pixels | 64 MiB / 4096 entries |
| CPU vector reference pixels | 32 MiB / 4096 entries |
| Glyph atlas | 16 MiB / 8192 entries |
| Path geometry | 8 MiB / 256 entries |
| GPU path coverage | 1 MiB |
| Combined active vertex payload | 32 MiB |

These are payload limits, not a process-memory or physical-VRAM reservation.
Driver alignment, CPU container capacity, swapchains, pipeline resources and
separate solid/textured buffer capacities are additional. Atlas overflow falls
back to direct GPU geometry. Oversized active vertex frames fail explicitly.
Resource retirement currently relies on the backend's completion waits; a future
asynchronous submit path must retain resources through completion.

The root worker retains at most one replaceable pending frame. The application
uses the actual Animation scheduler, exposes its target and separately reports
presented FPS, dropped/pending frames, CPU timing, cache misses and CPU raster
counts. Precise host wakes do not remove the coarse U++ timer clock. A 120 FPS
target is not a guarantee of 120 presented frames.

## Measured comparisons

Same-machine 512 particles with grid, 4-second warmup and 20-second measurement:

| Build / target | Presented FPS | Median acquire/replay/present CPU | Private peak |
| --- | ---: | ---: | ---: |
| Fresh starting implementation / 60 | 40.15 | 9.20 ms | 166,969,344 B |
| GPU coverage implementation / 60 | 40.00 | 4.98 ms | 158,859,264 B |
| Prior instrumented convex implementation / 120 | 64.00 | 4.44 ms | 156,340,224 B |
| Current DirectWrite + straight-stroke implementation / 120 | 64.00 | 4.75 ms | 156,794,880 B |

CPU elapsed time includes acquisition/presentation waits; these are not GPU
timestamps. Target 60 comparisons isolate drawing changes; target 120 additionally
changes scheduling. These are observed runs, not hardware-independent guarantees.

The current artifact's complete 300-second heavy run with validation requested
passed: 64.00 FPS, timer-delay p99 17.91 ms / max 22.79 ms, private peak
180,408,320 B. Early/late private means were 179,344,384 / 180,187,136 B
(842,752 B growth; unchanged allowance 33,554,432 B).
Median/p99 acquire/replay/present CPU time was 7.12 / 15.71 ms, maximum 23.58 ms.
Warm last-frame GPU paths were 1078, of which 1050 reused GPU coverage; direct
geometry had 1008 vertices. CPU vector rasters and glyph misses were zero.
No GPU failure or software fallback occurred and final native ownership was ZERO.
Validation-requested process memory includes extra validation overhead; do not
compare that directly with the non-validation peaks above.

Cold preparation is measured separately: normal/heavy first successful frames
took 122.55 / 118.83 ms CPU time, including 153 glyph misses, one CPU vector
raster and 31 GPU coverage renders each. Warm last-frame misses/rasters were zero.
This proves warm reuse, not removal of cold CPU glyph/complex-vector preparation.
Normal presented 64.00 FPS with median/p99 CPU time 3.08 / 5.89 ms; heavy was
4.75 / 10.28 ms. Both required DirectWrite with zero legacy GDI font requests.
Device: NVIDIA GeForce RTX 4070 Ti. Compared with the fresh starting heavy run,
median CPU elapsed time is about 48% lower and private peak about 6% lower.
The previous convex-only heavy median was 4.44 ms; the current provider/line
change does not improve every individual metric.


## Qualification application and checks

GpuUiGallery has a live scene, menu, dropdown, slider, PropertyEditor, pause/resume,
animation target and real FPS. Its second Vulkan window contains a 2000-row table,
expanded checkable tree, single-line and multiline text editors. It shares the
application GPU domain while owning its own surface, swapchain and renderer.

Commands (run the current built artifact, not an older release ZIP):

- `GpuUiGalleryVulkanDirectWrite.exe --require-gpu --particles=512 --animation-fps=120`
- `GpuUiGalleryVulkanDirectWrite.exe --qualify-ui --validation`
- `GpuUiGalleryVulkanDirectWrite.exe --benchmark --require-gpu --animation-fps=120`
- `GpuUiGalleryVulkanDirectWrite.exe --benchmark-load --require-gpu --animation-fps=120`
- `GpuUiGalleryVulkanDirectWrite.exe --benchmark-soak --benchmark-seconds=300 --require-gpu --validation --animation-fps=120`

The automated application qualification passes pause/resume, slider/mode and
inspector callbacks, native focus/text keys, tree/table keyboard navigation,
table inline editor commit/cancel across presented frames, resize, light/dark and
modal state preservation. It checks GPU-required mode, zero software fallback
and final native ownership. Three complete workspace open/edit/close cycles
observed one device, two surfaces and two swapchains each. It does not replace visual review, physical input,
IME/accessibility, or a 100%/150%/200% monitor-DPI matrix.

UiGpuDrawingTest covers 54 control drawing entries at two sizes, enabled and
disabled (216 cases), with populated table/tree/text and additional styled-face
pixel and edit-state checks. RenderVulkanPathTest checks 96 AA cases against an
independent analytic reference across RGBA/BGRA UNORM/sRGB, plus clipping,
painter order, cache bounds and active-geometry rejection.

Current whole-application manual visual/input acceptance remains pending.
Source, artifact hashes, commands and job IDs are retained in
`build/vulkan-ui-qualification-2026-10-11.json`. The refreshed drawing, vector,
text, image, bridge, presentation and populated-Ui regressions pass, including
Release/Debug path tests and implicit renderer-destruction cleanup. The current
Gallery SHA-256 is
`46939af650e99b8b388a839af886bae48f3d528ebb0ee64bfc4c180f9df16136`.

The matching Ui dependency is main commit `b09b06f99d2fee57f87d8b869c6eff0e10a1aa73`.
Current Gallery build job: `exec-268FA9EAFEBAE527514A386A78F5CB0E`; automated UI run:
`exec-3D54418774B196312D7FBA5AB04F7DE2`; completed heavy soak:
`exec-A03A35B5C771ECCF8978378C3515FCF2`. These identify the tested DirectWrite artifact above.

The ten-gate checklist currently passes gates 1, 2, 3, 8 and 9: **5/10 (50%)**.
This is an equal-count acceptance checklist, not an estimate of development effort.
The remaining gates require the full semantics stated in ACTIVE_WORK.md.

## Still required for the full non-GDI milestone

1. General compound/concave paths, gradients, dashes/open strokes and SVG still
   use the bounded Painter/image reference path. GPU composition is not proof
   that their coverage was generated on the GPU.
2. DirectWrite removes the current assembly's GDI metrics/outline dependency.
   Glyph cache misses still use ImagePainter; full shaped runs/bidi, colour emoji
   and IME are not qualified. Warm reuse does not remove that cold CPU work.
3. Shared Ui shadows, general shape/focus helpers and software layers still need
   semantic GPU paths. The common rounded-face fast path covers only its stated
   subset.
4. Remove the coarse timer-clock/rearm limitation at the owning U++/Animation
   layer, with pause/resume/re-entrant lifecycle tests and another throughput run.
5. Complete current manual controls/AA/DPI acceptance. Only then count those
   gates passed.

See ACTIVE_WORK.md for the ten explicit completion gates. Historical RC1 and
optional wgpu proofs do not complete these remaining requirements.
