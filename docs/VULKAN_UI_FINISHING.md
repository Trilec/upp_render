# Windows/Vulkan whole-UI finishing

Updated 2026-10-15. This is implementation and qualification evidence for the
current direct Vulkan renderer. It is not a claim that all CPU font/vector work
has been removed. WebGPU and Metal follow this Windows work.

## Current general coverage checkpoint — 2026-10-15

General paths and cold glyph masks now have GPU-generated coverage and bounded
caches. Fill/stroke curves, compound paths, gradient spread/alpha/colour-space,
clipping, integer placement reuse, bounds and recovery pass current tests.
The scalar text path preserves U++ metrics/fallback/decorations; full shaped
runs remain open. SVG and other Ui shared raster helpers remain explicit.

The ten-gate Windows checklist is **6/10 (60%)**, with gate 4 newly passed.
This does not imply 100% GPU semantics for every helper or completion of manual
AA/input/DPI review. See [GPU coverage and current evidence](GPU_VECTOR_COVERAGE.md).

## Drawing implemented (earlier fast-path checkpoint)

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

## Current application measurements — 2026-10-15

Current executable: build/GpuUiGalleryVector.exe, SHA-256
8f52c2de6ff7c737e9e40529f1f3247833a8551c5e0fee7c342c99e177b4abe7.
Build job: exec-429A5938219CA0D10AE31A0E1C31F170.
Both runs below requested validation and required GPU presentation, used a
4-second warmup and 20-second measurement, with grid enabled and Animation target 120.

| Workload | Presented FPS | First frame CPU | Median / p99 replay CPU | Private peak |
| --- | ---: | ---: | ---: | ---: |
| 96 particles | 64.01 | 87.03 ms | 6.03 / 11.52 ms | 194,785,280 B |
| 512 particles | 64.01 | 80.26 ms | 6.57 / 14.06 ms | 195,473,408 B |

Both cold frames generated 148 drawable glyph masks and one general vector mask
on the GPU, using four compute submissions including atlas clear. CPU glyph
and vector rasters were zero. Warm glyph/vector coverage submissions were zero;
retained coverage scratch was 4,247,552 B. Existing Ui helper images remain
separately reported (nine shared raster-cache misses); this is not all-helper
GPU acceptance. Both runs passed responsiveness, reported no GPU error or
presentation fallback, and ended with native ownership ZERO and zero GDI fonts.

Initial unbatched measurements were 403.11 ms (point-grid coverage) and 258.76 ms
(horizontal integration); bounded glyph batching produced the current 87.03 ms
normal first frame. These are observed process-cold timings with driver/cache
effects, not a controlled GPU microbenchmark or a claim that warm throughput
improved. The Animation callback/record/present cadence still measured 64.01 Hz.

Current automated workspace/compute-dialog qualification:
exec-8BC58631CAA064A3244C8919B53200AB. Current 216-case control drawing test:
exec-1EA9360C46772C782DD0A2BC3BF91883.
General vectors and glyph pixel tests pass Release/Debug; see GPU_VECTOR_COVERAGE.md
and the exact qualification manifest for identities, limits and legacy-AA differences.

The current 300-second measured heavy soak (plus 4-second warmup) also passed:
63.96 presented FPS, CPU median/p99 6.47/13.62 ms, timer delay p99/max
18.15/42.72 ms. Private early/late means were 198,071,296 / 198,521,856 B:
450,560 B growth against the unchanged 33,554,432 B allowance. Peak was
199,172,096 B. Warm coverage misses remained zero; two sampled descriptor pools
were created and reused 38,718 times. No GPU error/fallback occurred and final
native ownership was ZERO, with zero GDI font requests.
Soak job: exec-A14A8BFB8F450D48F8F3891DF91E3EA5.

## Historical measured comparisons — 2026-10-11

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

Commands for the current Gallery build/GpuUiGalleryVector.exe (SHA-256 below):

- `GpuUiGalleryVector.exe --require-gpu --particles=512 --animation-fps=120`
- `GpuUiGalleryVector.exe --qualify-ui --validation`
- `GpuUiGalleryVector.exe --benchmark --require-gpu --animation-fps=120`
- `GpuUiGalleryVector.exe --benchmark-load --require-gpu --animation-fps=120`
- `GpuUiGalleryVector.exe --benchmark-soak --benchmark-seconds=300 --require-gpu --validation --animation-fps=120`

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

The 2026-10-11 checkpoint passed gates 1, 2, 3, 8 and 9: **5/10 (50%)**.
The current checklist adds gate 4: **6/10 (60%)**.
This is an equal-count acceptance checklist, not an estimate of development effort.
The remaining gates require the full semantics stated in ACTIVE_WORK.md.

## Still required for the full non-GDI milestone

1. SVG retains the bounded Painter/image reference path. General FillPath/StrokePath
   coverage is now GPU-generated and independently qualified; this does not remove
   SVG or Ui helpers that create images before recording.
2. DirectWrite removes this assembly's GDI font dependency and cold glyph masks
   now generate on the GPU. Full shaped runs/bidi, colour emoji and IME still
   need explicit contracts and acceptance. Warm reuse does not remove that cold CPU work.
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

## Historical sampled-image allocation refinement — 2026-10-13

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
