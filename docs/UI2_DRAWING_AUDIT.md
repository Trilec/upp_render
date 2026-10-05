# Portable Ui drawing integration — UI2

Started 2026-10-05 after Windows/Vulkan RC1 qualification.
Application code owns Ui controls once; backend selection occurs below resolved
drawing. Windows/Linux target Vulkan, Apple targets Metal, browser targets WebGPU.
No OpenGL implementation is planned for this milestone.

## Current boundary audit

| Seam | Current implementation | Required work |
| --- | --- | --- |
| Control painting | Win32 SystemDraw adapter into UiCanvas/display list | A portable recording seam; keep one Ui layout/input/theme authority |
| Flat primitives/images | Shared RenderGpu2D replay and bounded resource caches | Preserve semantics and batching across providers |
| Image source rectangle/tint | Neutral source rectangle, tint/opacity and alpha-mask intent; original-image GPU upload | Software reference and GPU geometry/ownership verified; offscreen GPU pixel-readback parity remains |
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
and offscreen GPU pixel-readback parity are not claimed.

Required Gallery, validation requested, 4 s warmup plus 20 s measured:

| Scene | UI delay p99 / max | Process private peak | Presented frames | Root fallback |
| --- | --- | --- | --- | --- |
| 96 particles | 24.56 / 36.43 ms | 474,370,048 bytes | 982 | 0 |
| 512 particles | 18.07 / 29.10 ms | 535,658,496 bytes | 186 | 0 |

Both responsiveness gates PASS, no GPU error, final ownership ZERO. Heavy replay
CPU p99 was 175.39 ms; this remains a slow-rendering stress scene with responsive
UI. These runs do not establish a process-memory improvement or new plateau.

## Next bounded acceptance gates

1. Close pixel-readback parity for the implemented source-rectangle/mask/opacity path;
   compare transparent and crop-edge pixels across UNORM/sRGB GPU targets.
2. Choose and test the portable text/host boundary, then exercise a small WebGPU
   browser application using the same control and drawing code.
3. Extend full control coverage, Linux Vulkan and Metal using the verified contract.
   No duplicate painters, theme systems, one-surface-per-control or unbounded queues.

RC1 artifacts and tag remain the previous qualified snapshot. UI2 builds are
development evidence and require their own dependency identities.
