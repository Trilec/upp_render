# upp_render project plan

## Product goal

Provide U++ developers with a simple backend-neutral accelerated rendering system:

- `GpuCtrl` for an embedded GPU surface;
- `GpuWindow` for a custom full GPU client area;
- `GpuTopWindow` for a GPU-composited U++/upp_Ui interface;
- `GpuPainter` as the ordinary drawing API;
- `GpuContext` to share compatible expensive backend state across many presentation surfaces.

Vulkan is first. Metal and WebGPU are first-class follow-on backends.

## Stage status

### Stage 1 — backend-neutral recording/software reference

**PASS / accepted.** Value types, immutable display lists, deterministic inspection and software replay.

### Stage 2 — RHI and Null backend

**PASS / accepted.** Neutral GPU resource/command/surface contracts and headless lifecycle/state validation.

### Stage 3 — Vulkan backend/bootstrap

**PASS / accepted.** Vulkan 1.3 runtime/instance/device/queues, surfaces, swapchains, frame lifecycle, resources/uploads and cleanup.

### Stage 4 — GPU 2D renderer

**PASS / accepted.** Geometry, strokes, rounded rectangles, clipping, affine transforms, opacity/source-over and batching. Vulkan framebuffer orientation correction is accepted.

### Stage 5 — image/text/vector

Images and text are **PASS / accepted**. Vector/gradient/AA/SVG implementation is complete and has passed targeted Vulkan regressions; its final consolidated Windows/Vulkan acceptance is folded into the productization acceptance matrix tracked in `docs/ACTIVE_WORK.md`.

### Stage 6 — U++ integration

Underlying presentation, embedded frame source, CtrlCore recording bridge and root compositor are **PASS / accepted**.

### Productization / H1

**IMPLEMENTATION COMPLETE — PLATFORM VALIDATION PENDING.** The code now has:

- one ordinary `GpuRender` package/header and `GpuPainter` drawing surface;
- canonical embedded/custom-window/root-compositor examples;
- backend-neutral provider registration in `RenderRhi`;
- Vulkan provider implementation/registration in `RenderVulkan`;
- generic presentation orchestration through neutral `GpuDevice` contracts;
- compatibility-keyed Vulkan runtime/instance/logical-device sharing;
- one shared device-level `VkPipelineCache` for compatible surfaces;
- independent surface/swapchain/frame and presenter renderer state;
- queue-scoped per-surface teardown rather than a device-wide stall for ordinary swapchain destruction;
- final device-wide idle only when the last shared device lease is destroyed;
- migration-residue audit covering obsolete ownership assumptions, package/provider direction, staged `.inc` rationale, diagnostics and public docs.

The remaining productization work is the consolidated Windows/Vulkan acceptance run, not another architecture pass unless validation exposes a real defect.

### Stage 7 — effects/compute/specialized rendering

Not started as a product stage. Defer until current productization acceptance is clean. Likely scope: effects/layers, offscreen helpers, compute/storage resources, specialized views.

### Stage 8 — hardening/backends/platform expansion

Planned after current acceptance:

1. output parity/readback and longer-running multi-surface hardening where useful;
2. **Metal** — macOS bring-up first; design platform/presentation seams to remain viable for iOS/iPadOS;
3. **WebGPU** — backend bring-up plus browser/WebAssembly feasibility. Browser U++ requires a platform host/event/input/text layer in addition to RenderRhi/WebGPU;
4. future backend evaluation after the neutral contract proves itself across Vulkan plus at least one substantially different backend.

## Current acceptance sequence

1. Build and run the backend registry and Vulkan bootstrap/surface/frame/RHI regression authorities.
2. Validate shared compatible surfaces: one runtime, one instance, one logical device and one pipeline cache, with independent surfaces/swapchains and clean survivor/final-close behavior.
3. Re-run GPU2D image/text/vector coverage, including the remaining consolidated Stage-5 vector gate.
4. Re-run embedded frame-source, root compositor and CtrlCore bridge coverage.
5. Build/smoke the canonical application examples and Renderer Showcase.
6. If all results are clean, record final Windows/Vulkan productization acceptance in `docs/ACTIVE_WORK.md` and move to the next backend/hardening stage.

## Architecture rules

- no backend-native types in application painter/control APIs;
- no second U++ theme/layout/input/control-state system;
- one root surface for a GPU-composited top-level UI;
- embedded native surfaces only for explicitly accelerated `GpuCtrl` content;
- compatible presenters may share expensive backend/device state, but mutable surface/frame and renderer-owned resources require explicit identity/lifetime before sharing;
- software replay remains semantic reference/fallback;
- diagnose root causes; do not weaken tests to accommodate architecture changes;
- publish coherent recoverable checkpoints and keep `docs/ACTIVE_WORK.md` current.
