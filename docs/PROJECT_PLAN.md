# upp_render project plan

## Product goal

Provide U++ developers with a simple backend-neutral accelerated rendering system:

- `GpuCtrl` for an embedded GPU surface inside an ordinary U++ application;
- `GpuWindow` for a custom full GPU client area;
- `GpuTopWindow` for a GPU-composited U++/upp_Ui interface;
- `GpuPainter` as the ordinary application drawing API;
- `GpuContext` to share compatible expensive backend state across many presentation surfaces.

Vulkan is the accepted first backend. WebGPU and Metal are first-class follow-on backends.

## Accepted renderer stages

### Stage 1 — backend-neutral recording/software reference

**PASS / accepted.** Value types, immutable display lists, deterministic inspection and software replay.

### Stage 2 — RHI and Null backend

**PASS / accepted.** Neutral GPU resource/command/surface contracts and headless lifecycle/state validation.

### Stage 3 — Vulkan backend/bootstrap

**PASS / accepted.** Vulkan 1.3 runtime/instance/device/queues, surfaces, swapchains, frame lifecycle, resources/uploads and cleanup.

### Stage 4 — GPU 2D renderer

**PASS / accepted.** Geometry, strokes, rounded rectangles, clipping, affine transforms, opacity/source-over and batching. Vulkan framebuffer orientation correction is accepted.

### Stage 5 — image/text/vector

**PASS / accepted.** Images, text/glyph atlases, vector paths, gradients, AA and SVG passed the consolidated Windows/Vulkan gate.

### Stage 6 — U++ integration foundation

**PASS / accepted for the current representative slice.** Shared presenter, embedded frame source, CtrlCore recording bridge and root compositor are working and validated.

### Productization / H1

**PASS / accepted.** The code now has:

- one ordinary `GpuRender` package/header and `GpuPainter` drawing surface;
- canonical embedded/custom-window/root-compositor examples;
- backend-neutral provider registration in `RenderRhi`;
- Vulkan provider implementation/registration in `RenderVulkan`;
- generic presentation orchestration through neutral `GpuDevice` contracts;
- compatibility-keyed Vulkan runtime/instance/logical-device sharing;
- one shared device-level `VkPipelineCache` for compatible surfaces;
- independent surface/swapchain/frame and presenter renderer state;
- queue-scoped per-surface teardown and final device-wide idle only when the last shared device lease is destroyed.

Final productization acceptance is recorded in `docs/ACTIVE_WORK.md`.

## UI1 — complete Vulkan U++ UI experience

**ACTIVE / highest priority.** Do this before starting another backend.

The goal is not merely that `GpuTopWindow` can show a Button. The goal is a convincing full U++ application surface rendered through Vulkan.

Required completion:

1. representative standard U++ controls record and render through one `GpuTopWindow` without unsupported common Draw semantics;
2. images, text, vectors, scrollable/data controls and custom Draw controls work together in a realistic root control tree;
3. a modal `GpuTopWindow` dialog can coexist with a GPU parent and reuse the compatible application GPU domain;
4. multiple GPU top-level windows preserve independent surface/swapchain state while sharing compatible device resources;
5. popup/dropdown/menu/tool-tip behavior is audited explicitly — separately-created native popup windows must not be described as GPU-composited until they really are;
6. ordinary `TopWindow` + embedded `GpuCtrl` remains fully supported for bounded accelerated content surrounded by normal U++/GDI controls;
7. recorder gaps are closed based on real control usage first, then broadly useful neutralizable Draw operations;
8. remaining native/GDI/invert/XOR/pattern boundaries stay explicit rather than being silently discarded.

Acceptance/demo targets:

- `GpuUiGallery` — full root-composited U++ UI with a lightweight animated custom `Ctrl`;
- `GpuSurfaceDemo` — ordinary Ui shell with one/two adjustable embedded `GpuCtrl` surfaces;
- `examples/diagnostics/GpuEmbeddedMotion` — retained stock-U++/GDI compatibility probe;
- `GpuUiCoverageTest` — representative standard-control tree must record without unsupported common Draw operations and replay through the software reference.

The animated demo is deliberately cheap: a small deterministic calm-particle field at a modest refresh rate. It is intended to look pleasant while making redraw, resize and presentation stalls obvious on low-end hardware.

### UI1-A / UI1-B / UI1-R1 / UI1-R2

Accepted. See `docs/ACTIVE_WORK.md` for the compact recovery checkpoint.

### UI1-C — transient and multi-window completion

**PASS / accepted.** Real product-control menus, dropdowns, tooltip and consolidated root-smoke acceptance are recorded in `docs/ACTIVE_WORK.md`.

Do not broaden this milestone into a renderer redesign. Architecture discoveries that do not block transient correctness belong in UI1-D/UI2 planning.

## UI1-D — shared immutable resource evolution

Windows/Vulkan v1 implements the immutable-image subset: serial/size/format identity, shared native allocations with independent handles and reference lifetimes, bounded image/vector/glyph retention, and close-first survivor tests. General glyph/vector metadata sharing and the broader requirements below remain future convergence work. See [current measured implementation](WINDOWS_VULKAN_V1.md).

The recent `upp_Ui` / UiNodeGraph software benchmark work strengthens this milestone: repeated immutable presentation work should not be regenerated per item/per frame merely because the GPU can execute it quickly.

Candidate resource classes:

- immutable image/upload identity;
- glyph/font atlas/page identity;
- reusable vector/path identity;
- SVG/vector-source identity;
- immutable geometry;
- reusable composed surfaces only where semantics and lifetime justify them;
- pipeline/material state already compatible with the context/device domain.

Required design properties:

- stable identity/keying rather than accidental pointer identity;
- explicit ownership/reference lifetime;
- context/device compatibility;
- invalidation/versioning;
- synchronization/readiness;
- memory budget and eviction policy;
- presenter-survivor behavior;
- device/context loss/recovery behavior.

Do **not** simply globalize presenter-owned mutable caches. Two windows must be able to share an immutable resource without coupling surface/frame lifetime or allowing stale handles after one presenter closes.

UI1-D should preserve the ability to support future normal `upp_Ui` workloads efficiently, including semantic image/text/path reuse, batching/instancing and local damage. It should not force source-rect images, tint, 9-slice or non-rect clipping into awkward backend-specific workarounds.

## UI2 — focused upp_Ui / upp_render convergence

After UI1-C and the UI1-D resource model are stable, define a bounded integration milestone for normal `upp_Ui` presentation rather than gradually accumulating ad-hoc dependencies.

The target dependency direction is:

```text
upp_Ui software-only operation
  -> U++ Draw / UiDraw

opt-in GPU composition
  -> resolved U++ / upp_Ui presentation + damage
  -> renderer-side integration / RenderCtrlBridge
  -> UiCanvas / immutable UiDisplayList
  -> RenderGpu2D / RenderRhi
  -> backend
```

`upp_Ui` must remain independently usable and must not acquire a hard dependency on `upp_render` or `RenderVulkan`.

UI2 should explicitly cover:

- renderer-side adapter/integration ownership;
- dirty-region propagation and partial-damage policy;
- source-rect image drawing/crop;
- image opacity/tint/modulation;
- neutral 9-slice helper strategy;
- rounded/path clipping or equivalent non-rect clip semantics;
- DPI/logical-coordinate contract;
- repeated styled-surface/image/text/path resource reuse;
- LOD/prepared-presentation boundaries for heavy controls;
- software-reference parity;
- one root-composited scene rather than one native GPU surface per ordinary control.

Do not freeze the exact integration mechanism prematurely. `UiCanvas` / `UiDisplayList` remain the renderer-owned neutral vocabulary and should stay semantic enough to preserve batching, instancing and resource-reuse opportunities.

See `docs/UPP_UI_RENDER_CONVERGENCE.md` for the architectural constraints behind UI1-D/UI2.

## Stage 7 — effects/compute/specialized rendering

Not started as a product stage. Likely scope:

- offscreen/layer helpers;
- effects/compositing;
- compute/storage resources;
- specialized high-volume views.

Do not let Stage 7 distract from completing UI1/UI1-D/UI2.

## Stage 8 — hardening/backends/platform expansion

### WebGPU

Preferred next backend experiment after the complete Vulkan UI/resource/integration foundation if it gives faster executable evidence on the current Windows development machine.

Two distinct problems must remain separated:

1. `RenderWebGPU` provider behind the existing backend registry/RHI contract;
2. true browser U++ hosting, which additionally needs a WebAssembly/browser CtrlCore platform layer for event loop, input, focus, timers, text/IME, clipboard, cursor and canvas/window hosting.

A native Windows WebGPU implementation via an appropriate WebGPU implementation may be useful for early RHI conformance, but it must not be confused with browser hosting.

### Metal

First-class Apple backend after or in parallel with WebGPU when a real macOS validation machine is available. Bring up macOS first while keeping platform/presentation choices viable for iOS/iPadOS.

### Cross-backend hardening

Once a second backend is executable:

- run the same backend-neutral conformance scenes/tests on both;
- add output/readback parity where practical;
- stress resize, multi-surface and resource lifetime;
- add deliberate device-loss/recovery policy rather than backend-specific ad hoc recovery.

## Architecture rules

- no backend-native types in application painter/control APIs;
- no hard `upp_Ui -> upp_render` or `upp_Ui -> backend` dependency for normal software use;
- no second U++ theme/layout/input/control-state/model system;
- one root surface for a GPU-composited top-level UI;
- embedded native surfaces only for explicitly accelerated `GpuCtrl` content inside ordinary non-root-composited U++ windows;
- transient native popup/menu/tooltip windows may use additional presentation surfaces while sharing compatible context/device state;
- compatible presenters share expensive backend/device state and immutable Image allocations through explicit identity/reference lifetimes; mutable glyph atlases and vector metadata stay local;
- preserve UI invalidation/damage information as an integration concept;
- keep `UiDisplayList` semantic enough for backend-neutral optimization;
- software replay remains semantic reference/fallback;
- popup/native-child boundaries must be explicit;
- diagnose root causes; do not weaken tests to accommodate architecture changes;
- publish coherent recoverable checkpoints and keep `docs/ACTIVE_WORK.md` compact/current.

## Embedded-surface product demonstration

`GpuSurfaceDemo` follows the UiLabel / UiProgressRing shell and puts one or two bounded Vulkan `GpuCtrl` instances inside an ordinary Ui application. Its inspector changes width/height, shape count/type, speed, grid/text/background and per-surface animation. Randomize and second-surface destruction/recreation make drawing and lifetime behavior visible. The Code page emits a complete minimal public-API application.

Surface ownership independence is implemented. GpuTopWindow now offers opt-in immutable-frame worker replay with one replaceable pending frame and synchronized public presenter transactions. This bounds root admission and protects GUI event processing without guaranteeing independent GPU queues or sibling fairness. Current `GpuCtrl` recording/presentation executes synchronously in the GUI paint handler, including Vulkan frame/fence waits. Future off-thread preparation/render scheduling needs a bounded explicit design for immutable handoff, shared queue synchronization and safe teardown. It must not move ordinary Ui control access onto arbitrary workers.

## Current sequence

1. Review the embedded `GpuSurfaceDemo` product example and maintain current cross-repository validation; UI1-C is accepted.
2. UI1-D: broaden the implemented immutable-image identity/lifetime/reuse/survivor contract to other resource types when justified.
3. UI2: focused `upp_Ui` integration/convergence milestone with damage/parity requirements.
4. Then begin WebGPU provider work on the current machine; Metal follows when executable Apple validation is available.

## Version 1 release preparation

The release candidate is Windows/Vulkan-first. Current RC scope, measured memory/responsiveness requirements, shader boundary and demo distribution are recorded in [RELEASE_CANDIDATE.md](RELEASE_CANDIDATE.md). BGFX is a design reference ([BGFX_REVIEW.md](BGFX_REVIEW.md)); no backend replacement or dependency adoption is implied.
