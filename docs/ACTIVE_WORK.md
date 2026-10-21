# Active Work

## Restart handoff — 2026-10-21

Read [WINDOWS_VULKAN_RESTART.md](WINDOWS_VULKAN_RESTART.md) before continuing.
The latest finishing changes are saved but uncommitted. Native lifecycle test
passed; final regressions, rebuilt captures, whole-UI qualification and current
memory/load/soak measurements remain. The older percentage below is historical.

## Current priority — Windows/Vulkan whole UI

The user's confirmed order (2026-10-13) is:
1. Finish UI drawing through the existing direct Vulkan renderer, including the remaining CPU/GDI drawing seams.
2. Measure and improve memory use, frame scheduling and responsiveness under load.
3. Qualify a convincing antialiased whole-UI application with working controls.

Keep C++/U++, Ui controls, themes, Designer and application models. Work on main.
Core and Animation are read-only by owner instruction. Their write access is not
a blocker: isolate scheduling and rendering within this repository first.
WebGPU/browser and Metal follow this Windows milestone. wgpu-native is an optional
experiment; its successful native proof is not authorization to replace the
current renderer or postpone Vulkan UI completion.

## Current checkpoint —2026-10-22

**7/10 completion gates passed (70%)**:1,2,3,4,6,8,9. The same ten gates below remain in force; this counts acceptance coverage, not engineering effort.

Semantic SVG/Painter, shared Ui helpers, shadows, clips and layers now preserve GPU drawing intent. All56 drawable catalogue entries pass224 inventory cases; eight extra shadow cases and populated main/compute/browser/colour-picker/micro-picker qualification pass. Ordinary non-GPU UiLabelDemo Release BLITZ also builds.

Current Gallery9a4d8f6f... measures120distinct updates/s. The300second heavy soak passes at119.87presentedFPS, CPUmedian/p99=6.15/14.27ms and private early/late decrease747,520B. No CPU vector/glyph raster, shared helper raster misses, software layers, fallback or GDI font requests; final native ownership ZERO.
Exact source hashes/jobs/reports: VULKAN_UI_FINISH_QUALIFICATION.json and VULKAN_UI_FINISHING.md.

The user explicitly authorized full complex-script shaping/editing now, after discussing font loading versus text layout. Implement a shared layout authority for glyph runs, measurement, hit-testing, caret/selection and cluster-safe editing; reuse DirectWrite and existing GPU glyph coverage. Keep Core/Animation read-only. Full text integration and final physical input/AA/DPI acceptance remain open. Read WINDOWS_VULKAN_RESTART.md for detailed recovery context.

Ui drawing baseline committed locally as228a1b968e0b13ef21b4cbb4baf1115c3c7a8686. Preserve concurrent unrelated Ui changes. No push/publish.

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
