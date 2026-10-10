> Priority correction — 2026-10-11: this is an optional future architecture experiment. The current authorized milestone is completing the existing direct Windows/Vulkan whole-UI renderer. This document does not authorize a renderer replacement. See ACTIVE_WORK.md.

# Clean wgpu renderer architecture

Status: design baseline; implementation and platform qualification pending.
Repository baseline: 0999308ad1b7e2f60789356b7923fb93955c5480.
This document governs the new wgpu path. ARCHITECTURE.md describes the preserved
Vulkan implementation; its accepted behaviour remains useful reference evidence.

## Decision and scope

Keep C++/U++, Ui controls, themes, Designer, application models and imaging code.
Use wgpu-native's C API directly inside the new native GPU implementation.
Do not port applications to Rust merely to obtain a portable graphics backend.
Rendera remains a useful renderer name: wgpu supplies GPU machinery, while our
renderer supplies Ui composition, text/images, caches, scheduling and host integration.

Do not reproduce a second Vulkan/Metal/WebGPU resource API over wgpu.
Retain a semantic canvas/display-list boundary because controls need drawing
operations, not GPU command encoders. Retain U++ window/control facades where
their lifecycle is useful. No permanent RenderRhi/GpuDevice provider adapter in
the new path. Small private RAII owners are justified for correct wgpu lifetimes;
they must not become another public backend framework.

Native: wgpu-native selects Vulkan on Windows initially, Metal on Apple devices,
and optionally DX12 if separately qualified. Browser: a WASM/browser WebGPU
binding and browser host, not the native library deployed into a browser.
Select only needed native backends in dependency builds. Cross-platform API
coverage does not prove U++ host, font or application portability.

## Minimal structure

| Unit | Responsibility |
| --- | --- |
| RenderCtrlBridge | Existing Ctrl painting into neutral lists; Win32 SystemDraw capture, independent of GPU backend |
| Existing canvas/list semantics | Record ordered fills, clips, images, text and supported vector operations; immutable frame data |
| RenderWgpu (new package) | Direct wgpu handles, pipelines, uploads, replay, bounded caches, device/surface lifetime |
| U++ host facade | Root/embedded presentation, invalidation, events, DPI, popups and GUI-thread ownership |
| Host-specific source files | Win32 first; Apple native and browser hosts admitted only with working target builds |

Start with one new GPU package, not a package per helper class. Keep wgpu headers
private to implementation where feasible; avoid exposing wgpu handles to Ui.
A media image-pass API belongs to the renderer and describes source, colour
intent, viewport and completion. It must not recreate a general-purpose RHI.

The recorder implementation has moved unchanged out of GpuRender into
RenderCtrlBridge. Its dependencies are CtrlCore and RenderCanvas; the old include
forwards to it. The native wgpu proof is isolated in RenderWgpuNativeTest and
passes Windows C ABI/device/surface checks. It is not yet the RenderWgpu replay
implementation. Evidence and reuse boundary: WGPU_NATIVE_PROOF.md.

## Ownership, scheduling and shutdown

Ui owns hierarchy, layout, input, focus, theme, state and invalidation.
Recording runs on its GUI thread. Immutable frames pin their image/glyph/vector
data and carry root, frame serial and optional media-generation identity.
No GPU worker traverses live controls or reads mutable application models.

One device domain owns its device, serialized queue submissions and shared
immutable resources. Each window owns a surface and independently resizable
presentation state. Closing one surface must not destroy resources needed by
another. Explicitly release per-frame, surface and domain owners in that order.
Completion callbacks carry lifetime-safe state and are invalidated/drained before
host destruction. Device loss resolves outstanding work as failed and provides
a controlled rebuild path; silent software fallback cannot count as GPU success.

Begin with a synchronous host/replay path to establish correctness. Add an
optional worker only after measuring the need. Worker interactive mode allows
one replaceable pending frame. EveryFrame media mode never silently replaces a
frame: use bounded acceptance/backpressure. Submission completion and presentation
submission are distinct from physical display scanout; report only observable
events. CineView must acknowledge the renderer outcome, not just CPU selection.

Limit accepted work before allocation: in-flight frames, upload/staging slots,
texture bytes, cache entries and readback bytes. Busy/budget/unsupported are
explicit results. Current-frame resources are pinned; eviction retires unused
resources after their last referencing submission completes. No steady-state
queue/device-idle wait per image or frame. Poll native asynchronous callbacks
without blocking the GUI thread; browser scheduling remains asynchronous.

## Drawing and shaders

Preserve painter order and nested clip semantics. Batch only adjacent compatible
commands, never reorder translucent controls. Begin with rectangles, textured
quads and alpha-mask text. WGSL sources live in the repository with reproducible
build embedding, rather than opaque inherited SPIR-V arrays.

Specify premultiplied UI alpha, linear/sRGB interpretation and render-target
encoding. Retain cropping, masks, padded rows, origin checks and overflow
validation from accepted behaviour, not necessarily old implementation code.
Validate format/usage/filtering/blending limits on the actual adapter before use.
High precision sources retain original bits; colour changes must not re-upload
unchanged sources. Handle negative, non-finite and HDR values in the colour pass.

CPU Painter rasterisation is an explicit initial vector/SVG path with bounded
coverage caches. Existing Windows glyph production can establish Windows parity;
it cannot be called portable text. Plan shaping/font discovery/rasterisation and
DPI behaviour for Apple/browser separately. Complex paths, inverted/XOR drawing,
native-widget themed output and unsupported formats need explicit implementation
or a visible unsupported result; never claim generic wgpu parity automatically.

Compute belongs inside RenderWgpu using actual compute pipelines and bind groups.
Admit storage resources, LUTs and async readback when required by a real imaging
operation. OCIO integration requires pinned shader generation, WGSL compatibility
or an explicit translation step, binding layouts and CPU-reference comparisons.
Existing OpenImageIO/OpenFX/OCIO application integrations stay at their current
boundaries; wgpu does not make every C++ dependency available on iPad or browsers.

## Platform gates

| Target | Required proof beyond GPU backend |
| --- | --- |
| Windows x64 | U++/clang link, matching native ABI, Win32 surface, DPI/input/popups, driver and teardown checks |
| macOS | Apple toolchain/link, Metal surface, U++ host and portable text |
| Native iPad | ARM64 device and simulator builds, signing, UIKit/Metal layer, touch/keyboard/IME, lifecycle/rotation, dependency audit |
| Browser/iPad browser | WASM toolchain, compatible C WebGPU binding, canvas/events/text, async lifetime, browser capability checks |

Native iPad feasibility is an early gate, before a full control migration.
A Windows machine cannot certify it. A Mac Metal demo does not establish UIKit
hosting. Avoid a custom fork of U++ for every platform without first estimating
host work against available U++ platform support.

## Qualification and lean mandate

Reuse neutral semantics and behaviour fixtures only when dependencies are clean.
Do not adapt legacy Vulkan objects into wgpu or keep two implementations for
every new feature. Preserve the old path as a migration reference until the new
path meets its gates; then retire obsolete dependencies from the default build.

Measure baseline versus wgpu on the same machine/workload: clean build time,
release package size, startup, cold/warm replay CPU, upload counts, private/GPU
memory where measurable, input-delay p50/p95/p99, frame outcomes and shutdown.
Use actual GPU timings only where supported. Five-minute churn/resize/close tests
must reach a memory plateau and release owned resources. No speed or size claims
from API choice alone. Record limits that require a direct-native escape hatch
before adding one; no speculative multi-backend framework.

References:
- https://github.com/gfx-rs/wgpu
- https://github.com/gfx-rs/wgpu-native
- https://github.com/webgpu-native/webgpu-headers
- GPU_IMAGE_PIPELINE.md
- CINEVIEW_RENDERER_HANDOFF.md
- WGPU_IMPLEMENTATION_PLAN.md
- WGPU_RECOVERY.md
