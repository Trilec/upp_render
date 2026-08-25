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
- `GpuEmbeddedMotion` — ordinary U++ window with a lightweight animated `GpuCtrl`;
- `GpuUiCoverageTest` — representative standard-control tree must record without unsupported common Draw operations and replay through the software reference.

The animated demo is deliberately cheap: a small deterministic calm-particle field at a modest refresh rate. It is intended to look pleasant while making redraw, resize and presentation stalls obvious on low-end hardware.

## UI2 — shared resource evolution

After UI1 common-control correctness is stable, define explicit resource identity/lifetime for safe cross-presenter sharing.

Candidates:

- immutable image/upload cache;
- glyph atlas/page cache;
- immutable vector/SVG raster cache;
- pipeline/material state already compatible with the context/device domain.

Do **not** simply globalize presenter-owned mutable caches. Two windows must be able to share an immutable resource without coupling surface/frame lifetime or allowing stale handles after context/device loss.

## Stage 7 — effects/compute/specialized rendering

Not started as a product stage. Likely scope:

- offscreen/layer helpers;
- effects/compositing;
- compute/storage resources;
- specialized high-volume views.

Do not let Stage 7 distract from completing UI1/UI2.

## Stage 8 — hardening/backends/platform expansion

### WebGPU

Preferred next backend experiment after the complete Vulkan UI gate if it gives faster executable evidence on the current Windows development machine.

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
- no second U++ theme/layout/input/control-state system;
- one root surface for a GPU-composited top-level UI;
- embedded native surfaces only for explicitly accelerated `GpuCtrl` content inside ordinary non-root-composited U++ windows;
- compatible presenters may share expensive backend/device state, but mutable surface/frame and renderer-owned resources require explicit identity/lifetime before sharing;
- software replay remains semantic reference/fallback;
- popup/native-child boundaries must be explicit;
- diagnose root causes; do not weaken tests to accommodate architecture changes;
- publish coherent recoverable checkpoints and keep `docs/ACTIVE_WORK.md` current.

## Current sequence

1. UI1-A: build/run the new full UI gallery, embedded-motion demo and representative control-recording test.
2. UI1-B: implement missing common Draw semantics revealed by those real controls; add broadly useful arc/rotated-text/Drawing/Painting coverage where coherent.
3. UI1-C: broaden to modal dialogs, multiple GPU top-level windows, dropdown/menu/popup/tool-tip behavior and representative `upp_Ui` control coverage.
4. UI2: add explicit shared image/glyph/resource identities and lifetime tests.
5. Then begin WebGPU provider work on the current machine; Metal follows when executable Apple validation is available.
