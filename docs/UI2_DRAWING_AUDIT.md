# Portable Ui drawing integration — UI2

Started 2026-10-05 after Windows/Vulkan RC1 qualification.
Application code owns Ui controls once; backend selection occurs below resolved
drawing. Windows/Linux target Vulkan, Apple targets Metal, browser targets WebGPU.
No OpenGL implementation is planned for this milestone.

Updated evidence: [Vulkan UI finishing](VULKAN_UI_FINISHING.md) records the
2026-10-11 convex GPU/rounded-face work, populated-control qualification and
completed 300-second soak. The earlier incomplete runs below are retained as
historical evidence; they are not the current sustained result.

## Original boundary audit

| Seam | Current implementation | Required work |
| --- | --- | --- |
| Control painting | Win32 SystemDraw adapter into UiCanvas/display list | A portable recording seam; keep one Ui layout/input/theme authority |
| Flat primitives/images | Shared RenderGpu2D replay and bounded resource caches | Preserve semantics and batching across providers |
| Image source rectangle/tint | Neutral source rectangle, tint/opacity and alpha-mask intent; original-image GPU upload | Crop/mask/opacity pixels and transparent-edge interpolation verified across RGBA/BGRA UNORM/sRGB |
| Paths/SVG | Cached CPU Painter raster into sampled GPU images | Keep portable CPU raster available; optimize measured hot paths without per-control GPU APIs |
| Text | U++ font metrics and per-character glyph raster into atlases | Portable shaping/measurement/glyph contract including fallback fonts, bidi, IME, selection and DPI |
| Clip | Rectangular neutral clip; non-axis-aligned transformed clip rejected | Non-rectangular clip semantics and reference pixels |
| Group opacity/shadows | No complete neutral layer/effect contract | Implement only required control semantics with explicit memory limits |
| Damage | U++ invalidation consumed through whole-root recording | Carry damage into recording/replay; measure idle and small-update work |
| Root failure policy | Default software fallback; opt-in SetRequireGpu | Required mode reports errors and never enters ordinary root software painting |
| Transient windows | Separate synchronous GPU presenters with fallback | Owned transient presenters inherit required mode and propagate failure to the root |
| Platform host | Win32 window descriptors/session hooks only | Linux Wayland/X11, Apple host and browser/Wasm event/input/lifecycle integration |
| Backend composition | GpuRender currently includes Vulkan; root accepts Vulkan | Platform-selected provider composition; Metal and WebGPU implementations |

Native GDI/SystemDraw entry, exclusion clipping, patterned/XOR polygons and several
invert operations fail explicitly in RenderCtrlBridge. These are audit entries,
not instructions to reproduce obsolete drawing operations on every backend.
Map each actual Ui use to its intended appearance before extending the vocabulary.

## First implementation: required GPU presentation

Select SetRequireGpu() before opening a GpuTopWindow. Startup or frame failure
leaves the root on a failed GPU path, retains GetGpuError(), releases failed
presentation resources and consumes native paint without software control replay.
RetryGpuInit() is explicit. The default software fallback remains available.
GetSoftwareFallbackCount() counts root native paint dispatches to ordinary
TopWindow painting; it is not a count of CPU rasterization or all GDI calls.

GpuUiGallery --require-gpu enables this policy and exits nonzero on GPU errors,
writing GpuUiGallery-gpu-failure.txt. Benchmark reports include gpu_required and
software_fallback_count. Required-mode benchmark files use the separate
GpuUiGallery-required-normal/load/soak.txt names to preserve earlier qualification
reports. Gallery modal roots and owned popup/tooltip presenters inherit the policy. Failed transient
recording/presentation stops that presenter and reports to its root; unavailable
paint hooks report failure and hide the transient. This still uses Windows font
and hosting APIs, so it does not certify a fully GDI-free application.

## Static-library provider composition

A required-mode non-BLITZ Gallery run exposed a missing Vulkan provider:
its registration-only object was omitted by the static linker. GpuRender now
explicitly anchors Vulkan provider registration when opening a Vulkan presenter.
Application code still includes GpuRender and does not register providers itself.
The original RC1 tag and packaged binaries are preserved.

## Reproducible control inventory

Run tools/audit_ui_drawing.py --ui <upp_Ui root> --output build/ui2-drawing-audit.json.
The audit records current inventory identity, dependency revision/dirty state,
per-file SHA-256 and locations of native/raster/text/image/helper references.
The scan includes 25 inventory support headers and their matching implementation files.
It includes comments and direct source references; it is not a complete call graph.
The current development inventory contains 55 controls: direct source observations
include CPU raster references in 15, text in 26, images in 15 and shared drawing
helpers in 39. The direct native reference is UiColorPicker screen-colour sampling,
a host-service concern rather than evidence that its Paint uses GDI.
Every control starts unverified until its native state/input/drawing tests pass.
Do not remove upstream controls based only on catalogue coverage.

## Development verification — 2026-10-05

The Gallery was clean-built without BLITZ in a separate cache. Current Ui BLITZ
builds have icon implementation/header-order linkage errors; concurrent Ui work
was preserved. Dependency: Ui 5fd831cfad273ae0e907aec8a551cf83f6ae2f79,
dirty working tree, inventory SHA-256
018dd3fd3e78fbb43634a9cef6e17eced51323c25e98954999ca0d5075b54372.
The audit artifact records the actual scanned file hashes.

Required-GPU Gallery, Vulkan validation requested, 4 s warmup plus 20 s measurement:

| Scene | UI delay p99 / max | Process private peak | Presented frames | Root fallback |
| --- | --- | --- | --- | --- |
| 96 particles | 18.42 / 25.21 ms | 305,205,248 bytes | 1017 | 0 |
| 512 particles | 18.03 / 31.38 ms | 536,489,984 bytes | 206 | 0 |

Both runs passed the responsiveness gate, reported no GPU error, and released
final Vulkan ownership to ZERO. Heavy replay CPU elapsed p99 was 150.44 ms;
responsive UI does not imply fast scene rendering. The worker's single pending
frame remains bounded and replaces older frames under load. These short runs
are not a new memory-plateau/soak qualification or complete control acceptance.
Debug and non-BLITZ Release root regression PASS: startup failure, synchronous/worker post-record
failure, explicit retry and unsupported owned-popup failure. Default and required
popup lifecycle tests both PASS: shared device, independent teardown and final ZERO.
No new manual visual/input acceptance is claimed by these timed/native regressions.
Reports: build/GpuUiGallery-required-normal.txt and
build/GpuUiGallery-required-load.txt. GPU timestamp timing remains unavailable.

## Image integration — 2026-10-06

DrawImage now carries an integer source rectangle, tint/opacity and alpha-mask
intent. The U++ Draw bridge overrides scaled DrawImageOp, bypassing inherited CPU
rescaling, cropping and per-colour image creation. Vector materialization keeps
the intent. Text and image-only scenes use the same active geometry path.

GPU crop filtering clamps to source-edge texel centres using at most nine quads;
normal full images remain one quad. Masks use the original image's alpha with
a separate fragment shader and the existing vertex/descriptor layout. Adjacent
mask colours batch together. Texture identity remains original image serial,
size and target colour format. Crops retain/upload the full original image;
cache limits apply to its full allocation, not the cropped area.

Software reference pixels match independently prepared crop/mask/opacity output.
Debug/Release geometry/cache tests and real Vulkan validation tests PASS: one original
upload for multiple crops/colours, warm zero uploads, separate mask fragment with
shared vertex shader, edge-centre UVs and final ownership ZERO. Native Draw bridge
identity/scaled-destination regression PASS. New manual visual/input acceptance
are not claimed for this image implementation.

Required Gallery, validation requested, 4 s warmup plus 20 s measured:

| Scene | UI delay p99 / max | Process private peak | Presented frames | Root fallback |
| --- | --- | --- | --- | --- |
| 96 particles | 24.56 / 36.43 ms | 474,370,048 bytes | 982 | 0 |
| 512 particles | 18.07 / 29.10 ms | 535,658,496 bytes | 186 | 0 |

Both responsiveness gates PASS, no GPU error, final ownership ZERO. Heavy replay
CPU p99 was 175.39 ms; this remains a slow-rendering stress scene with responsive
UI. These runs do not establish a process-memory improvement or new plateau.

## Next bounded acceptance gates

1. Populate model-backed controls and verify drawing/input states beyond the
   completed 216 default enabled/disabled drawing cases.
2. Choose and test the portable text/host boundary, then exercise a small WebGPU
   browser application using the same control and drawing code.
3. Extend full control coverage, Linux Vulkan and Metal using the verified contract.
   No duplicate painters, theme systems, one-surface-per-control or unbounded queues.

RC1 artifacts and tag remain the previous qualified snapshot. UI2 builds are
development evidence and require their own dependency identities.

## Pixel acceptance and transparent filtering — 2026-10-06

VulkanGpuDevice.ReadTexturePixels is a synchronous diagnostic for initialized,
owned RGBA/BGRA8 TransferSrc targets, with no open command lists and exclusive
queue access. It caps the output at 64 MiB, invalidates noncoherent host memory,
restores the image layout and destroys temporary staging/command resources.
It is never called by the production UI frame path.

Readback verifies cropped translucent pixels, RGB tint, alpha masks and opacity
in all four RGBA/BGRA UNORM/sRGB formats against independent compositing values.
Aligned UNORM crop/mask/tint scenes also compare every pixel with the software
reference. Warm replay after readback verifies restored layout and zero uploads.

The tests exposed two defects: Painter's default transparent extension faded
magnified crop boundaries; straight-alpha GPU filtering darkened transitions
into transparency (red 99 rather than 159 at one tested UNORM sample).
The software image fill now pads crop edges. GPU images use premultiplied colour
filtering and explicit PremultipliedSourceOver blending. UNORM uploads reuse
original U++ pixels; sRGB uploads encode colour premultiplied in linear space.
Tint opacity multiplies both RGB and alpha. Glyphs use the alpha-mask pipeline.
This preserves one original-image allocation across mask/tint/crop variants.

## Browser fundamentals and implementation order

WebGPU remains the chosen browser backend. Windows can qualify it before Metal:
[Chrome supports WebGPU on suitable Windows hardware](https://developer.chrome.com/docs/web-platform/webgpu/overview).
Browser capability/adapter creation must still be checked on the actual machine.

A Windows executable cannot run in a canvas by switching its Vulkan backend.
The application/control code needs a WebAssembly build plus a browser host.
[Emscripten supports browser WebGPU via Emdawnwebgpu](https://emscripten.org/docs/porting/multimedia_and_graphics/WebGPU-support.html).
Its [runtime](https://emscripten.org/docs/porting/emscripten-runtime-environment.html)
requires browser-compatible event/lifecycle and filesystem handling.
Browser shaders use [WGSL](https://www.w3.org/TR/WGSL/); existing Vulkan SPIR-V
is not the browser shader input.

Project implementation order: finish Windows drawing/control conformance and
sustained responsiveness first; separate portable text and control recording
from Win32 services; then implement RenderWebGPU and a Wasm host with the same
Ui controls/display-list contract. Browser host work includes pointer/key/focus,
DPI/resize, IME/composition, clipboard, font loading and page lifecycle. Bound
caches/queues and explicit failure remain shared semantics. Metal follows with
Apple-host validation. WebGPU implementation remains unstarted; documentation
and Windows feasibility do not count as a working provider.

## Inventory drawing baseline and sustained load

UiGpuDrawingTest records and renders 54 painted inventory entries on real
Vulkan: default enabled/disabled states at 128x32 and 320x180, including the
UiTag painter fixture. All 216 cases passed with no unsupported operations,
zero validation warnings/errors and zero final ownership. UiOsFileDialog is
a host service, not a painted Ctrl. Empty models and headless layout containers
are included; this baseline does not accept populated models, input, hover,
pressed/focus states, themes, DPI, IME or accessibility.

The first updated 300-second required-GPU heavy soak passed memory plateau
(early/late mean private bytes 534,887,424 / 536,577,024) and cleanup. It failed
the existing responsiveness gate: p99 20.99 ms, maximum 107.37 ms above 100 ms.
Preserved evidence: build/ui2-pixel-soak-failure-2026-10-06.json. The threshold
is unchanged. Root recording/enqueue CPU timings now identify front-end costs.
Discarded pending frames release their payload outside the worker/statistics
mutex; at most one pending frame is still retained. The repeat also failed:
p99 23.47 ms/max 126.04 ms, with record max 19.83 ms/enqueue max 0.60 ms.
Memory plateau PASS (early/late 535,615,488 / 536,798,208 bytes), final ZERO.
Evidence: build/ui2-pixel-soak-repeat-failure-2026-10-06.json. These diagnostics
do not yet explain the timer outlier; computer-use review had already closed.

## Gallery visual/input review — 2026-10-06

The rebuilt required-GPU Gallery passed native review: dropdown mouse selection
and keyboard open/Escape dismissal, pause/resume state, slider value changes,
inspector grid toggle, readable tooltip and a shared-domain GPU modal that closes
without losing the root state. This is representative input acceptance, not all
control states. The owner also reports the visible controls look and function well.
The interactive review exited normally before the sustained benchmark began.
The whole client UI is recorded and composited through Vulkan; CPU font/path
rasterization and Windows hosting remain dependencies.

Concurrent Ui development was preserved. Benchmark evidence identifies each
executable and separately records the observed dirty dependency files.

## Particle preparation and Animation integration

Native ellipses now record canonical local curve coordinates plus translation.
Moving them no longer changes cached geometry through floating-point cancellation.
The bridge regression verifies identical fill/stroke keys and translated pixels.
The first 512-particle short retest reduced replay median from 134.59 to 52.50 ms,
image entries from 4096 to 532 and private peak from 538,951,680 to 243,462,144 bytes.
These are different-duration runs; the short result is not a plateau qualification.

Solid filled/stroked paths now cache white coverage independently of colour and
opacity, replaying through the alpha-mask shader. Gradient/SVG content retains
coloured rasters. Real Vulkan readback verifies one shared raster/upload for red
and blue translucent versions in all four target formats.

The Gallery now explicitly uses upp_animation: one owned looping Animation drives
elapsed-time phase at the shared 60 Hz scheduler rate. Pause/Resume freezes the
scheduler state; reset replays from zero. Animation::Finalize follows application
teardown. This replaces the per-scene fixed-step timer without adding a painter
or changing Ui's input/layout ownership. The owner reports faster motion, with a small slowdown when showing the grid.
The live status now reports actual presentation FPS and replay CPU milliseconds.
The inspector supports 512 particles. Optional precise Windows wakes dispatch
the existing Animation/U++ scheduler; lifecycle and coalescing tests pass.
Grid on/off production measurements and sustained validation are recorded below.

## Live FPS and grid comparison — 2026-10-06

Required-GPU Release production runs used identical GpuUiGalleryUi2Next.exe
(SHA-256 4a27b22c7126ceee60de41d730d83fa3b93d9de5baa06c1d78392bf787fb6877),
512 particles, four-second warmup and about 20 seconds of measured throughput.

| Grid | Presented FPS | UI delay p99 / max ms | Private peak bytes |
|---|---:|---:|---:|
| On | 44.00 | 18.30 / 21.34 | 165,511,168 |
| Off | 41.60 | 29.69 / 52.57 | 167,182,336 |

Both passed the unchanged short responsiveness gate, with no software fallback
and final native ownership ZERO. This single sequential pair does not establish
a grid-related slowdown; replay medians were 9.36 / 9.29 ms. FPS counts frames
actually presented after warmup, not the configured Animation rate. GPU timing
is unavailable. Evidence: build/ui2-fps-grid-2026-10-06.json.

Precise host wakes do not replace the scheduler time base: this U++ build's
msecs() still uses Windows GetTickCount. A configured 60 Hz scheduler therefore
does not guarantee 60 presented FPS. Further pacing/preparation measurements
are needed; the high-end GPU alone does not remove UI-thread recording costs.

The subsequent five-minute validation run ended at about 78 seconds before
its measurement report completed. The report retained RUNNING plus final ZERO;
the job returned exit 1. The existing GPU-failure file predates this run.
No runtime failure cause or owner action is inferred. This is an incomplete
qualification, not a sustained pass or a completed responsiveness measurement.
