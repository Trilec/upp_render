# Active Work

## Current Windows / Vulkan checkpoint — 2026-10-22

**7 of 10 current acceptance gates pass (70%):1,2,3,4,5,6,8.**
This counts the ten acceptance criteria below, not coding effort.

The whole-Ui root, modal workspaces, compute dialogs, file browser, full colour
picker, micro picker and text composition now pass automated Vulkan qualification.
The catalogue passes224 drawing cases at standard scale and224 at doubled
application units. Native physical monitor DPI remains a separate review.

Shared DirectWrite/system ICU layout is implemented for complex scripts, rich
text, caret/selection/hit-testing and grapheme editing. Native Windows IME
composition is routed by renderer-owned code. Release and Debug pass64 Vulkan
pixel,37 editor,36 styled,27 document,38 IME and80 font-layer checks.
Supported colour fonts:COLRv0 and embedded PNG/JPEG/TIFF/BGRA. SVG colour glyphs
and COLRv1 paint trees are not implemented.

Renderer source c1eb86f733dcf250ee14797fba0191aa732e825a and committed Ui
f88aab5150ca7f375b8c4a61a01da57832c82e03 produce the preserved final candidate.
Normal and512-particle/grid load sustain120 distinct animation updates/s.
Both full300s runs passed memory/cadence, but the current sustained responsiveness
gate is reopened: maximum timer delays361ms with validation and404.19ms without
validation exceed the unchanged100ms gate. Production private memory peaks at
179,949,568B with1,286,144B early/late growth. No software fallback, CPU glyph/vector
rasters or legacy GDI fonts; final tracked native ownership is zero.
The separate diagnostic build then passed both production and validation-enabled300s runs:120 distinct Hz,maximum timer21.23/19.30ms and under1MiB early/late growth. Neither reproduced a stall above50ms. The earlier delays remain unexplained; no scheduling fix is claimed.

Remaining:resolve sustained maximum-delay failure; physical controls/AA/monitor
DPI and real OS IME candidate interaction; current Caro runtime qualification.
Caro transfer is awaiting owner review of a changed approved Robocopy hash.
The current runtime ZIP and exact failed/passed reports are recorded in
[WINDOWS_VULKAN_FINAL_QUALIFICATION.json](WINDOWS_VULKAN_FINAL_QUALIFICATION.json).
Recover jobs and latest state from
[WINDOWS_VULKAN_RESTART.md](WINDOWS_VULKAN_RESTART.md).

Work on main; local commits only. Core and Animation remain read-only.
Unrelated Ui changes are preserved. WebGPU/browser, Linux and Metal follow this
Windows milestone; the native wgpu experiment does not replace Vulkan completion.

The material below preserves earlier milestones and the unchanged acceptance
criteria. Historical passing soaks do not override the current failed run.

## Starting checkpoint

- KlickCurt / Render: project-028DDA9C9C89, E:/apps/github/upp_render.
- Starting main: 5ca9485502f97ef0499faaca0205794a89d8aac6, clean.
- Direct Vulkan implementation and RC1 artifacts remain intact.
- RenderCtrlBridge was extracted unchanged; it still depends on Win32 SystemDraw.
- Verified historical bundle: build/render-before-wgpu-2026-10-08.bundle,
  SHA-256 97e1b69a2aec889377fcb2e96fc5686e2eb329b5ee1b1a23e4f177ba283553b0.
  Bundles preserve tracked Git history, not ignored outputs/dependencies.
- Preserve historical manifests, evidence, bundles and Patch recovery journals.

## Completion gates for this request

These ten gates define the Windows whole-UI checklist, not a time estimate.
A gate counts complete only when its complete stated criterion passes.

1. Whole-window Vulkan root presentation and resize/lifecycle regression.
2. Required-GPU failure/retry plus shared-domain popup/dialog lifecycle.
3. Rectangle/image/mask/tint/premultiplied pixel and painter-order correctness.
4. Direct GPU vector coverage, including filled/stroked curves, compound paths,
   gradients, clipping and explicit bounds; cached CPU raster is separately counted.
5. Deliberate text shaping/metrics/glyph preparation with the GDI drawing
   dependency removed and fallback/Unicode/decorations tested.
6. Ui shared styled/shape helpers preserve semantic GPU drawing intent; the
   recorder does not allocate a GDI probe/backbuffer on the frame path.
7. Populated control states/input, focus, clipping, light/dark and DPI acceptance.
8. Measured cold/warm normal/heavy throughput and responsiveness with bounded
   queues/cache/vertex payloads, real presented FPS and explicit diagnostics.
9. Completed five-minute heavy soak: existing delay thresholds, bounded memory
   plateau, no GPU error/fallback, complete report and final ownership ZERO.
10. Current whole-UI demo visual/input acceptance with AA and live diagnostics.

Historical evidence satisfies parts of these gates; none of the earlier RC1,
inventory-only or native wgpu proofs imply this whole checklist is complete.

## Preserved whole-UI checkpoint — 2026-10-11

**5/10 completion gates passed (50%)**: gates 1, 2, 3, 8 and 9.
This is an equal-count acceptance checklist, not a percentage of coding effort.
Convex fills, closed miter strokes and straight butt/square strokes use bounded
GPU coverage geometry. Optional semantic rounded Ui faces and a lifetime paint
guard are implemented. RenderFontWin32 now supplies DirectWrite metrics/outlines
through U++'s CFONTS hook; the strict Gallery reports zero legacy GDI font requests.
Populated table/tree/text, editing, focus, resize and light/dark automation pass
over three modal cycles. Normal/heavy throughput is 64.00 presented FPS at a
120 FPS Animation target; median CPU time is 3.08 / 4.75 ms.
The current artifact's full 300-second heavy validation-requested soak passes:
private-memory early/late growth 842,752 B, no GPU failure/fallback, final native
ownership ZERO. Cold preparation is 153 glyph misses and one CPU vector raster;
warm last-frame misses/rasters are zero. General vectors, other Ui raster helpers,
frame-pacing diagnosis and manual current-app/DPI review remain open. The 64 FPS
observation does not isolate a sole cause; separate Animation cadence, recording,
enqueue, GPU submit and acquire/present waits before attributing a limit.
See VULKAN_UI_FINISHING.md and WINDOWS_FONT_PROVIDER.md for exact scope,
artifact identity, measurements and remaining work.

## Additional Vulkan compute baseline — 2026-10-13

Optional storage-buffer compute, bounded descriptor reuse, device-local output,
GPU buffer-to-texture copying and GpuCtrl-owned custom content are implemented.
Numerical Release/Debug tests and three compute modal control/resize/lifecycle
cycles pass. A five-minute RTX 4070 Ti run measured 119.99 presented FPS, CPU median/
p99 2.46/3.66 ms and 5,263,360 B private growth. A later fixed 3840 x 2160 run
measured 119.93 FPS, CPU median/p99 4.89/8.50 ms and 249,856 B private growth.
Both reused one shader pipeline, image and descriptor pool; final native ownership
ZERO. The 4K callback maximum was 141.01 ms and remains an optimization target.
The instrumented 512-particle scene measured callback/progress/record/present
cadence at 64.00 Hz; heavy CPU replay p99 was 15.54 ms. These are two observed
limits, not proof of a sole cause. Core and Animation were not changed.
This supplements the ten UI gates; it does not complete general vectors, shaping,
source-image OCIO or asynchronous video. See VULKAN_COMPUTE.md and build/vulkan-compute-qualification-2026-10-13.json.

## Sampled-image allocation refinement — 2026-10-13

Completed sampled-image descriptor pools now use bounded reuse as well.
Release/Debug repeated pixel checks prove pool creation plateaus. The current
whole-UI 300-second load soak passed: 64 FPS, CPU median/p99 7.22/15.27 ms,
private early/late growth 845,824 B, no fallback/error and final native ZERO.
Only two sampled pools were created; 38,830 reuse operations were recorded.
The current 4K compute run holds 119.94 FPS with one compute and one sampled
pool created. Its callback max 156.13 ms remains an optimization target.
Current artifact: 59d1a2c8e44715c709040e5e9811b13beed1e3ab52d14924c6a6746db7829dcf.
See build/vulkan-sampling-qualification-2026-10-13.json and
WINDOWS_FRAME_PACING.md. This does not complete the open vector/text/helper/DPI
gates or the source-image OCIO/async pipeline.

## Known starting seams (historical baseline)

- Vulkan composites the entire client UI. U++ owns controls, layout and input.
- FillPath/StrokePath/SVG are CPU Painter images before sampled Vulkan replay.
- Glyph-atlas misses use ImagePainter and Windows font outline/metric services.
- Ui shared helpers can create cached AA Images before the recorder sees them.
- Control recording probes GlobalBackBuffer via ImageDraw on every frame.
- The precise Windows wake feeds a scheduler whose time base remains GetTickCount.
- Default inventory evidence covers 216 drawing cases, not populated/input/DPI.
- Latest five-minute UI2 run ended early; sustained qualification remains open.
- Native precision transfer/readback passed; efficient asynchronous video
  streaming, colour/LUTs, readback and compute are separate future imaging gates.

## Evidence and recovery

See VULKAN_UI_FINISHING.md, UI2_DRAWING_AUDIT.md, UI_GPU_RENDERING_ARCHITECTURE.md,
GPU_IMAGE_PIPELINE.md and WINDOWS_VULKAN_V1.md.
Optional experiment: WGPU_NATIVE_PROOF.md and WGPU_DEPENDENCY.md.
WGPU_ARCHITECTURE.md / WGPU_IMPLEMENTATION_PLAN.md are future design references.

Record actual source/dependency/artifact hashes, build/run jobs and test scope.
Tests, visual review and platform delivery are separate evidence.
FPS is not latency. CPU elapsed time is not GPU timestamp time.
Never count software fallback as required-GPU success or an incomplete soak as PASS.
