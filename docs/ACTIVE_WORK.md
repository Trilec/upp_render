# Active Work

## Current Windows/Vulkan checkpoint — 2026-10-24

**8/10 strict acceptance gates (80%); native text7/8 (87.5%).**
Current automated functional checks pass; physical AA/input/OS IME/monitor DPI
acceptance remains distinct from generated input. Owner confirms inspector,
workspace and animation worked and closed normally.

Final Release: build/GpuUiGalleryWindowsV1Final.exe,8,911,872B,
SHA65f771f7f1d2f7a1b4b7c451cce9bebec5ce9945bc842c44d17125bc703c683e.
FullUI Release validation15.375s, Debug validation20.375s and Caro production
13.078s PASS. Actual inspector colour accept/cancel is now included, along with
three workspace/compute cycles, browser, full/micro pickers and text composition.
All current reports show GDI0/fallback0/final native ownershipZERO.

Corrected renderer-owned timer starvation and invalid demo timer IDs. Found an
additional real drawing gap: Sizeable().Zoomable() editor dialogs loseWS_POPUP
and escaped the transient GPU hook. Actual inspector activation reproduced
surface1 and242 GDI requests; corrected owned-TopWindow inheritance gives two
surfaces/one device and GDI0. Default nonmaximizable dialog probe had passed and
was not representative; that history remains explicit. Core/Animation untouched.

Current Release/Debug nested standard-dialog regressions pass. Embedded filtered
timer delivery and callback destruction pass Release/Debug. Root test frame-wait
budget now restarts after independently timed modal stages; performance limits
were not changed.

Current Caro compute20s:60.02FPS,CPU4.36/6.58ms,callbackmax21.68ms,
privatepeak161329152B,one pipeline/image build,resource/memory/finalZERO PASS.
Prior Caro five-minute attempts were minimized or user-closed; one visible
changing-workload stress failed latency/memory/GDI. Preserve those failures.
Do not infer a successful Caro visible soak. Current final Curt production300s PASS119.99distinctHz/122.84FPS,timermax22.22ms,
0events>50ms,privategrowth1661952B,one reused pool,GDI0/finalZERO.

Authoritative evidence:
[WINDOWS_VULKAN_CARO_QUALIFICATION.json](WINDOWS_VULKAN_CARO_QUALIFICATION.json).
[Current summary](VULKAN_UI_FINISHING.md).
Historical wakeup race and earlier RTX300s passes:
[WINDOWS_VULKAN_LATENCY_INVESTIGATION.json](WINDOWS_VULKAN_LATENCY_INVESTIGATION.json).

New package staging build/upp-render-ui-2026-10-24-final contains65f7...exe,
matching instructions/checksums and current reports. Older qualified8058 and
c510 intermediate packets are superseded. Final archive/runtime delivery is independently verified on Caro. Pinned Ui exportf88aab5; independent dc789c1 worktree
has a substantial active refactor, preserved. Main only, local commits, no push.

The material below preserves earlier checkpoints; later current evidence above
supersedes earlier statuses while retaining their historical results.

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

Final ZIP:build/upp-render-ui-2026-10-24-final.zip,3884676B,
SHA4b08f8942347ab6ff4a5c981331bde564d5d46638423cba6dd35edf2bb21c5b4.
Share:F:/networkshare/upp-render-ui-2026-10-24-final.zip and extracted final folder.
Caro:C:/GitHub/upp_render/build/runtime-2026-10-24-final/GpuUiGallery.exe.
Original ZIP and executable independently hash-identical on Caro; no test running.
