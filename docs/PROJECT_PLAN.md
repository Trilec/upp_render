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

Images and text are **PASS / accepted**. Vector/gradient/AA/SVG implementation is complete and has passed targeted Vulkan regressions, but the final consolidated Windows/Vulkan acceptance `TASK-010-W1` remains tracked.

### Stage 6 — U++ integration

Underlying presentation, embedded frame source, CtrlCore recording bridge and root compositor are **PASS / accepted**. Active work is productization: one public package, simple painter API, shared context/device resources, canonical examples and current documentation.

### Stage 7 — effects/compute/specialized rendering

Not started as a product stage. Defer until the productization/shared-context boundary is stable. Likely scope: effects/layers, offscreen helpers, compute/storage resources, specialized views.

### Stage 8 — hardening/backends/platform expansion

In progress conceptually; implementation follows current productization.

Planned backend/platform tracks:

1. **Vulkan shared-device/resource hardening** — many surfaces on compatible shared device/context, retained independent swapchains, stronger output parity/readback tests.
2. **Metal** — macOS bring-up first; design platform/presentation seams to remain viable for iOS/iPadOS.
3. **WebGPU** — backend bring-up plus browser/WebAssembly feasibility. Browser U++ requires a platform host/event/input/text layer in addition to RenderRhi/WebGPU.
4. Future backend evaluation only after the neutral contract proves itself across at least Vulkan + one substantially different backend.

## Current productization sequence

1. Public `GpuRender` façade and `GpuPainter` — implemented, validation pending.
2. Consolidate U++ integration packages/folders — implemented, validation pending.
3. Canonical user examples and current docs — implemented in current productization pass.
4. Decouple backend creation/registration from the façade so `GpuRender` is not structurally Vulkan-only.
5. Implement compatibility-keyed shared Vulkan logical-device/pipeline/image/glyph resource ownership in `GpuContext`.
6. Consolidated Windows validation, including the remaining Stage-5 final gate.
7. Metal bring-up slice.
8. WebGPU/browser-host feasibility slice.

## Architecture rules

- no backend-native types in application painter/control APIs;
- no second U++ theme/layout/input/control-state system;
- one root surface for a GPU-composited top-level UI;
- embedded native surfaces only for explicitly accelerated `GpuCtrl` content;
- software replay remains semantic reference/fallback;
- diagnose root causes; do not weaken tests to accommodate architecture changes;
- publish coherent recoverable checkpoints and keep `docs/ACTIVE_WORK.md` current.
