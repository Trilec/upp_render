# Windows/Vulkan whole-UI finishing

Updated 2026-10-13. This is implementation and qualification evidence for the
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

Commands for the current compute-enabled Gallery (identity below; no older release ZIP):

- `GpuUiGalleryCompute.exe --require-gpu --particles=512 --animation-fps=120`
- `GpuUiGalleryCompute.exe --qualify-ui --validation`
- `GpuUiGalleryCompute.exe --benchmark --require-gpu --animation-fps=120`
- `GpuUiGalleryCompute.exe --benchmark-load --require-gpu --animation-fps=120`
- `GpuUiGalleryCompute.exe --benchmark-soak --benchmark-seconds=300 --require-gpu --validation --animation-fps=120`

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
Release/Debug path tests and implicit renderer-destruction cleanup. The preserved 2026-10-11
Gallery SHA-256 is
`46939af650e99b8b388a839af886bae48f3d528ebb0ee64bfc4c180f9df16136`.

Its matching Ui dependency was main commit `b09b06f99d2fee57f87d8b869c6eff0e10a1aa73`.
Preserved 2026-10-11 Gallery build job: `exec-268FA9EAFEBAE527514A386A78F5CB0E`; automated UI run:
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
4. Diagnose Animation callback cadence, control recording, enqueue and GPU
   acquire/submit/present waits separately. Core and Animation remain read-only.
   The embedded compute host can present at 120 FPS; that does not establish the
   sole cause of the original particle scene's 64 FPS observation.
5. Complete current manual controls/AA/DPI acceptance. Only then count those
   gates passed.

See ACTIVE_WORK.md for the ten explicit completion gates. Historical RC1 and
optional wgpu proofs do not complete these remaining requirements.

## Direct compute and embedded GPU content — 2026-10-13

The RHI now includes optional storage-buffer compute, complete push constants,
device limits, GPU buffer-to-texture copy and bounded completed-command descriptor
reuse. Device-local output cannot be mapped by callers. Quantization tests pass
in Release/Debug, including independent UINT16/dither references and GPU copy
bytes, zero validation diagnostics and final native ownership ZERO.

GpuUiGalleryCompute adds Scene > Image and compute: an ordinary Vulkan Ui dialog
with Pause, Randomize, Dither, mode and exposure controls around a GPU-generated
image. Three automated open/control/pause/resize/close cycles pass in addition
to the three populated workspace cycles. Shader/pipeline creation occurs once
per content open; image/buffer allocation changes only on resize.
The cutout recorder now handles native embedded GPU children with bounded disjoint
regions; exact offset/clip/restore pixels and excessive complexity are tested.
The software reference's untracked clip-state push was corrected at the same time.

The initial compute-enabled executable was build/GpuUiGalleryCompute.exe, SHA-256
58d8b611b780016b8368eb607c719959e16aa0042f393724ade5373bfac14716,
built by exec-056BE168766EBF65B03D4AB6AECA9F27. Its three-workspace/
three-compute-cycle qualification: exec-B78A90D2C8688A1588EFCC5F9E7A8D49.
Fixed 4K run: exec-A0499BC90A62F40F49BEF87DCB6A1210.
The five-minute compute soak used the preceding 6b69874985a6e498... artifact
and job exec-322E07D137F6505B56BB51719EA8EDD4; the newer optional 4K flag is
not retroactively included in that soak. Exact identities and reports are in
build/vulkan-compute-qualification-2026-10-13.json.
The Ui dependency had concurrent Screenplay extraction changes on top of b09b06f...;
those changes were left untouched, and observed dependency hashes are recorded.
Current visual/input and 100%/150%/200% monitor-DPI acceptance remain pending.

See VULKAN_COMPUTE.md for API limits, build-time shaders, measured workload and
remaining OCIO/source-image/async-streaming work. This baseline does not alter
the ten-gate denominator or claim the remaining drawing seams complete.

## Sampled-image allocation refinement — 2026-10-13

The current Gallery SHA-256 is
59d1a2c8e44715c709040e5e9811b13beed1e3ab52d14924c6a6746db7829dcf,
built by exec-5E239532A79DF8A8BC0602CD0DB615E6.
Current automated UI qualification: exec-A99866B2F376F481548AE5F7D47F140D.
Current fixed 4K run: exec-A921FB0035E0C80B95E4CF03F2C1DC11.
Completed current whole-UI 300-second soak: exec-A0E2E955FEE2100EA888F27E1999426B.

The sampled-image path now retains at most four reset descriptor pools after
successful synchronous completion; failed/live command descriptors remain separate.
Release and Debug image tests check repeated pixels and pool-creation plateau.
The current UI soak created two pools and reused them 38,830 times, with final
native ownership ZERO. CPU median/p99 was 7.22/15.27 ms; private growth 845,824 B.
Animation callback/progress/record/present rates remain 64 Hz. The current 4K
compute run measured 119.94 FPS but a 156.13 ms callback outlier remains.
This is a resource-allocation improvement, not evidence that latency is finished.

Exact reports/source/artifact identities are in
build/vulkan-sampling-qualification-2026-10-13.json.
See WINDOWS_FRAME_PACING.md for the source trace; Core and Animation were not modified.
