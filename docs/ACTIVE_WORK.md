# Active Work

## Current priority — Windows/Vulkan whole UI

The user's confirmed order (2026-10-11) is:
1. Finish UI drawing through the existing direct Vulkan renderer, including the remaining CPU/GDI drawing seams.
2. Measure and improve memory use, frame scheduling and responsiveness under load.
3. Qualify a convincing antialiased whole-UI application with working controls.

Keep C++/U++, Ui controls, themes, Designer and application models. Work on main.
WebGPU/browser and Metal follow this Windows milestone. wgpu-native is an optional
experiment; its successful native proof is not authorization to replace the
current renderer or postpone Vulkan UI completion.

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

## Current measured checkpoint — 2026-10-11

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
the coarse Core/Animation clock and manual current-app/DPI review remain open.
See VULKAN_UI_FINISHING.md and WINDOWS_FONT_PROVIDER.md for exact scope,
artifact identity, measurements and remaining work.

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
