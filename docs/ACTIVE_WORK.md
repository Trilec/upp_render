# Active Work Status

Remote `main` is authoritative. This file is the recovery checkpoint for active `upp_render` work.

## Accepted Baseline

- Stage 1 backend-neutral display-list/software foundation: **PASS / accepted**.
- Stage 2 RHI + Null validation backend: **PASS / accepted**.
- Stage 3 Vulkan bootstrap/resources/presentation: **PASS / accepted**. Windows acceptance HEAD `6ab33a42a3421643359cabfdae7afed7628ad349`.
- Stage 4 GPU 2D renderer: **PASS / accepted**. Windows acceptance HEAD `f8e7b24d510b4b5889370823dc1c0a5ef43a7f54`.
- Vulkan framebuffer Y-orientation correction: **PASS / accepted**. Implementation `c13783aaad1ce10d4ade5ac8f020c56e876ae5f8`; revalidation `42d74c7bf44bac5f9ce8c92a3e553946943b8738`.
- Stage-5 image path: **PASS / accepted**. Implementation `a11862d138e6b2f06d92067b4b804d8418b69d32`; Windows acceptance `f2cd2bdf2ff7c05f7b883ef32405653ab198a98b`.
- Stage-5 text/glyph atlas: **PASS / accepted**. Implementation `f98cce413b1992cfaef55669d4672824fe703b5f`; Windows acceptance `91f1fe3cad91b5afe00de4afd6398b773e8f4715`.
- Stage-5 vector/gradient/AA/SVG implementation: `0d37b2472c4d49e6908f6acbf5f85cc523193006`; **IMPLEMENTATION COMPLETE — final consolidated Windows/Vulkan acceptance `TASK-010-W1` remains**.
- Stage-6 shared presenter/root boundary: **PASS / accepted** (`a4979f17...`, acceptance `cb01a20...`).
- Stage-6 embedded neutral frame source: **PASS / accepted** (`3ac69f1...`; focused package `947a060...`; acceptance `e3ad497...`).
- Stage-6 CtrlCore semantic recording bridge: **PASS / accepted** (`c4210d8...`; acceptance `d386ba1...`).
- Stage-6 root compositor wiring: **PASS / accepted** (`21ca529...`; acceptance `cb01a20...`).
- Renderer Showcase automated renderer coverage + GPU orientation visual boundary: **PASS**. Short human GUI button/property interaction smoke remains desirable but is not blocking architecture work.

## Active Objective — Productization / Multi-surface Architecture

The capability stack is no longer the main problem. The active objective is to turn it into a coherent U++ product:

1. `GpuCtrl` — embedded GPU rectangle inside a normal U++ application.
2. `GpuWindow` — whole custom client area painted by the application through a GPU painter.
3. `GpuTopWindow` — U++/upp_Ui control tree recorded and GPU-composited through one root surface.
4. `GpuContext` — shared compatible application GPU ownership so multiple surfaces do not behave like independent GPU applications.
5. `GpuRender` — the single ordinary developer package/header; lower `Render*` packages are renderer/backend implementation layers.
6. Preserve a backend-neutral public API suitable for Vulkan now, Metal on macOS/iOS-class platforms, and WebGPU/browser/WASM later.

## Published Productization Checkpoints

### P1 — public painter façade

- `f526a3d1208bbab1a47ac698757950cc6811075c` — introduced `GpuPainter`, `GpuWindow`, `GpuCtrl::SetGpuPaint()` and the `GpuRender` façade package.
- `3ecccc677aaff1c61a37932407e6e0ac3e534161` — finished the simple drawing surface:
  - `GpuPainter` receives live surface size via `GetSize()`;
  - `GpuCtrl` now has subclassable `GpuPaint(GpuPainter&)`, `WhenGpuPaint`, and `SetGpuPaint()`;
  - advanced `WhenBuildFrame` remains as an explicit neutral-display-list seam and takes precedence;
  - normal unconfigured `GpuCtrl` now presents an empty GPU frame rather than the internal reference/test scene;
  - `GpuWindow` uses the same size-aware `GpuPainter` model.

Status: **IMPLEMENTED — Windows compile/runtime validation pending after structural pass**.

### P2 — shared application context foundation

- `f6af476448f8853490f72c36ca5ebceb039f6d59` — added backend-neutral `GpuContext` and routed ordinary `GpuDisplayPresenter` instances through `GpuContext::Default()`.
- Current Vulkan implementation uses the context-owned `VulkanSurfaceSessionGroup`, so compatible presenters now share the accepted runtime/instance registry while retaining independent surfaces/swapchains.

Important boundary: logical `VkDevice`, renderer caches and resource pools are **not yet shared**; current grouped Vulkan sessions still create a device per surface. Extending sharing to a compatibility-keyed device/resource pool is the next heavy ownership block.

### P3 — physical U++ integration/package consolidation

- `df03851aad2ba0a94d27b2e0d9c3e28ba75da252` — consolidated the former `GpuCtrl`, `GpuTopWindow`, `RenderPresentation`, and `RenderCtrlBridge` packages under `render/GpuRender` and migrated in-repo consumers.
- `render/` now exposes one obvious U++ integration package plus genuine engine/backend layers:
  - `GpuRender` — ordinary application façade/integration;
  - `RenderCanvas` — neutral recording + `GpuPainter`;
  - `RenderCore` — neutral value types;
  - `RenderGpu2D` — GPU 2D replay;
  - `RenderNull` — headless validation backend;
  - `RenderPlatformWin32` — current native-window adapter;
  - `RenderRhi` — backend contract;
  - `RenderSoftware` — correctness/reference replay;
  - `RenderVector` — vector/Painter semantic authority;
  - `RenderVulkan` — Vulkan backend.

The old top-level integration directories are removed. Tests were retained as acceptance authorities while package/include names were migrated.

## Backend Direction

### Vulkan

Current production/validation backend. Next: move from shared runtime/instance to a compatibility-keyed application device/resource pool while preserving one surface/swapchain per native presentation target.

### Metal

First-class next backend target. Public `GpuPainter`, `GpuCtrl`, `GpuWindow`, `GpuTopWindow`, `GpuContext`, display-list and RHI contracts must contain no Objective-C/Metal types. macOS is the primary bring-up target; keep the design viable for iOS/iPadOS rather than baking desktop-window assumptions into the neutral layer.

### WebGPU

First-class next backend target. The long-term goal includes browser/WebAssembly hosting so U++ rendering/control intent can potentially replay through WebGPU. Public APIs therefore must not require HWND/Vulkan concepts. Native-window/surface adaptation and event-loop/browser integration stay behind backend/platform seams.

Do not create placeholder Metal/WebGPU implementations that imply support before their platform bring-up slices exist.

## Known Productization Debt

- README, ARCHITECTURE, GPU usage and project-plan documents are stale and must be rewritten from current repository truth.
- Add three canonical examples: embedded `GpuCtrl`, custom `GpuWindow`, whole-UI `GpuTopWindow`; move old milestone demos under an explicit diagnostics grouping or retire duplicates.
- Public `GpuRender` currently has a direct Vulkan implementation dependency through presentation. Before Metal/WebGPU bring-up, backend creation must be separated/registered so the façade is not structurally Vulkan-only.
- Shared `GpuContext` currently shares Vulkan runtime/instance but not logical device/pipelines/glyph/image caches.
- Staged `.inc` preservation in `RenderGpu2D` / `RenderVulkan` was useful during acceptance but should be normalized in a later hardening/hygiene pass once current architecture is validated.
- `TASK-010-W1` Stage-5 final consolidated Windows/Vulkan acceptance remains to close.

## Recovery Log

BASE: `3ecccc677aaff1c61a37932407e6e0ac3e534161` / `main`
TASK: Productization P4/P5 — canonical examples/current docs, backend decoupling, then shared logical-device/resource ownership
TOUCHED: `render/RenderCanvas/GpuPainter.h`, `render/GpuRender/*`, migrated examples/tests, `docs/ACTIVE_WORK.md`
STATUS: public façade + package consolidation + shared-context foundation published; Windows compile validation intentionally deferred until the next coherent structural checkpoint; device/resource sharing, examples/docs and backend decoupling remain active
PUBLISHED: `f526a3d...` public façade; `f6af476...` shared context foundation; `df03851...` integration folder consolidation; `3ecccc6...` simple size-aware GpuPainter control surface
VALIDATION: static source/dependency review only for the new productization blocks; previous accepted renderer/Vulkan evidence remains unchanged

## Next Action

1. Add/organize canonical examples and rewrite public documentation around `GpuRender` current truth.
2. Separate backend creation from `GpuRender` so Vulkan is one registered/default backend rather than a compile-time public-facade assumption; define clean seams for Metal/WebGPU.
3. Implement a compatibility-keyed shared Vulkan logical-device/resource context for multiple surfaces and update multi-control acceptance to prove shared expensive ownership with independent swapchains.
4. Run one consolidated Windows validation block covering public façade examples/header, `GpuCtrl`/`GpuTopWindow`/bridge regressions, multi-surface ownership, Renderer Showcase, and final Stage-5 `TASK-010-W1` matrix.