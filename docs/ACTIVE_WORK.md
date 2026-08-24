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
- Renderer Showcase automated renderer coverage + corrected GPU orientation: **PASS**. Short human GUI button/property interaction smoke remains desirable but is not blocking architecture work.

## Active Objective — Productization / Multi-surface Architecture

The capability stack is no longer the main problem. The active objective is to finish the product architecture and then remove migration residue before wider developer presentation:

1. `GpuCtrl` — embedded GPU rectangle inside a normal U++ application.
2. `GpuWindow` — whole custom client area painted by the application through a GPU painter.
3. `GpuTopWindow` — U++/upp_Ui control tree recorded and GPU-composited through one root surface.
4. `GpuContext` — shared compatible application GPU ownership so multiple surfaces do not behave like independent GPU applications.
5. `GpuRender` — the single ordinary developer package/header; lower `Render*` packages remain renderer/backend implementation layers.
6. Backend-neutral provider registration so Vulkan, Metal and WebGPU are implementation choices rather than public-facade assumptions.
7. Vulkan shared logical-device/resource ownership: compatible surfaces share one expensive device domain while retaining independent surface/swapchain/frame lifecycle.
8. `GPU-PRODUCTIZATION-H1` — post-architecture hygiene/legacy-removal audit before final Windows acceptance.

The publication rhythm is deliberate because sessions can time out: implement one coherent ownership/architecture slice, review, publish, immediately update this file, then continue from fresh remote `main`.

## Published Productization Checkpoints

### P1 — public painter façade

- `f526a3d1208bbab1a47ac698757950cc6811075c` — introduced `GpuPainter`, `GpuWindow`, `GpuCtrl::SetGpuPaint()` and the `GpuRender` façade package.
- `3ecccc677aaff1c61a37932407e6e0ac3e534161` — finished the simple drawing surface:
  - `GpuPainter` receives live surface size via `GetSize()`;
  - `GpuCtrl` now has subclassable `GpuPaint(GpuPainter&)`, `WhenGpuPaint`, and `SetGpuPaint()`;
  - advanced `WhenBuildFrame` remains as an explicit neutral-display-list seam and takes precedence;
  - normal unconfigured `GpuCtrl` now presents an empty GPU frame rather than the internal reference/test scene;
  - `GpuWindow` uses the same size-aware `GpuPainter` model.

### P2 — shared application context foundation

- `f6af476448f8853490f72c36ca5ebceb039f6d59` — added backend-neutral `GpuContext` and routed ordinary `GpuDisplayPresenter` instances through `GpuContext::Default()`.
- Compatible presenters share context-owned backend state while retaining independent presentation targets.

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

### P4 — canonical examples and current public surface

- `5438c32cbd52322e78b8b34300dc73d30e81c45f` — added/organized the three canonical entry examples (`GpuRenderEmbedded`, `GpuRenderWindow`, `GpuRenderUiWindow`) and moved bring-up/lifecycle examples under diagnostics.
- `bbf0e8baab086e995f4d9f752628b15819e84109` — public documentation/backend-roadmap checkpoint.
- `b7bd3f35190e57dead2edd4b687e6574d32f6a91` — published the three Windows mechanical compatibility fixes found during `GPU-PRODUCTIZATION-W1` (`RoundedRect` qualification and explicit `String(...)` text literals).

`GPU-PRODUCTIZATION-W1` Windows/Vulkan result at `bbf0e8b...`: façade/header, all three canonical examples, embedded/root/bridge regressions, Vulkan graphics/image/text/vector, GPU2D, Renderer Showcase and requested Release builds **PASS**; no Vulkan validation errors/crashes/assertions/blank surfaces.

### P5 — backend registration / decoupling

- `0101df4bb8d11457c616af38a69f00c7e3556bf4` — **IMPLEMENTED / PUBLISHED**.
- Generic `GpuRender` presentation no longer directly constructs Vulkan implementation types.
- Backend context/session creation is behind a neutral provider registry; Vulkan is the first registered provider.
- This is the required seam for future Metal and WebGPU backends without changing the ordinary `GpuRender` public API.
- `GpuBackendRegistryTest` was added and requires the final Windows productization validation block.

### P6 — shared Vulkan logical-device/resource domain

#### P6a — shared logical device + queues

- `319c8b4880520a546bd365316fd42b4e2fd35bf0` — **IMPLEMENTED / PUBLISHED — PLATFORM VALIDATION PENDING**.
- `VulkanSurfaceSessionGroup` now owns a ref-counted shared-device registry beside the shared-instance registry.
- Compatible surface sessions on one instance/physical device acquire one `VkDevice`; each session retains its own surface, swapchain, frame state and selected graphics/present queue views.
- The shared device requests one queue from every usable queue family so the first surface does not specialize the device to only its own present-family requirements.
- Closing one session releases only its device lease; the final compatible lease destroys the shared device.
- Existing grouped-session acceptance authority now requires two compatible surfaces to report one runtime, one instance, one logical device, two surfaces and independent swapchains; non-final/final close and incompatible-validation contexts are also covered.
- `AfterDeviceCreation` validation injection now fires only when a lease actually created the shared device, not on device reuse.
- P6a was generated/reviewed on an isolated temporary branch and recreated as a single clean `main` commit; temporary workflow/PR history was not merged.

#### P6b — shared safe device resources

- `37042388f1c580c9efbd43c38a76c5a8e0f1bd25` — **IMPLEMENTED / PUBLISHED — PLATFORM VALIDATION PENDING**.
- The shared `VulkanDeviceContext` now owns one `VkPipelineCache` for the lifetime of the shared logical-device entry and destroys it before the final `VkDevice` release.
- Surface `FrameInterop` borrows that cache alongside the shared device/queue handles; no per-surface cache ownership is introduced.
- Both ordinary and sampled `vkCreateGraphicsPipelines` paths now use the same shared pipeline cache instead of `VK_NULL_HANDLE`.
- Grouped-surface acceptance now requires the two compatible sessions to expose the same non-null pipeline-cache handle and verifies that the surviving session retains that same cache after the other surface closes.
- Mutable surface/swapchain/frame state remains independent. Glyph/image RHI handles have not been falsely promoted to cross-renderer sharing without an explicit lifetime/identity model.
- P6b was reviewed as a four-file production diff and recreated as a clean `main` commit; temporary execution workflow/PR history was not merged.

P6 code architecture is now complete. Expected multi-surface ownership after platform validation:

```text
2 compatible GPU surfaces
runtime live         = 1
instance live        = 1
logical device live  = 1
shared pipeline cache = 1
surfaces             = 2
swapchains           = 2
```

Known post-P6 hygiene item: swapchain destroy/resize still uses device-wide `vkDeviceWaitIdle`; safe but overbroad once one device is shared. H1 must determine whether accepted per-surface/frame synchronization allows narrowing that stall without weakening lifecycle safety.

## P7 — GPU-PRODUCTIZATION-H1 Architecture Hygiene / Legacy-Removal Audit

**ACTIVE ENGINEERING STAGE.** Re-fetch `main` after the P6 checkpoint and treat repository evidence as authoritative. This is a separate engineering stage, not a cosmetic pass.

Audit and simplify:

- old device-per-surface ownership paths superseded by P6;
- duplicate context/session/device state introduced by intermediate productization stages;
- direct Vulkan construction/selection paths that bypass the P5 backend registry;
- obsolete factories, compatibility wrappers, stale callbacks and dead members;
- stale old package names/includes and unnecessary `.upp` dependencies after `GpuRender` consolidation;
- tests that still encode retired ownership assumptions (especially two logical devices for two compatible surfaces);
- redundant resource/cache ownership and avoidable synchronization/`WaitIdle` calls;
- old examples/diagnostics duplicated by canonical examples;
- staged `.inc` preservation in `RenderGpu2D` / `RenderVulkan`: normalize where it now obstructs maintainability, retain only when it still has a real acceptance/architecture purpose;
- comments, README/architecture/usage/project-plan material describing architecture that no longer exists;
- full dependency direction (`GpuRender` -> neutral renderer/RHI contracts -> registered backend) with no reverse leakage.

H1 review question: **if this repository had been designed around the final architecture from day one, what migration-only code would not exist? Remove or justify it.**

Publish H1 corrections as a coherent checkpoint, then re-fetch and review the complete productization diff before Windows acceptance.

## Backend Direction

### Vulkan

Current production/validation backend. P6 completes shared compatible logical-device/resource ownership while preserving independent surfaces/swapchains.

### Metal

First-class next backend target. Public `GpuPainter`, `GpuCtrl`, `GpuWindow`, `GpuTopWindow`, `GpuContext`, display-list and RHI contracts must contain no Objective-C/Metal types. macOS is the primary bring-up target; keep the design viable for iOS/iPadOS rather than baking desktop-window assumptions into the neutral layer.

### WebGPU

First-class next backend target. Long-term goal includes browser/WebAssembly hosting so U++ rendering/control intent can replay through WebGPU. Public APIs therefore must not require HWND/Vulkan concepts. Browser CtrlCore/event-loop/input/text/clipboard/canvas hosting is a separate platform layer above/beside the WebGPU renderer backend.

Do not create placeholder Metal/WebGPU implementations that imply platform support before their bring-up slices exist.

## Remaining Acceptance / Debt

- P7 H1 architecture hygiene / legacy removal: **active**.
- P5 backend registry and P6 ownership changes require consolidated Windows/Vulkan validation after H1.
- Stage-5 final consolidated `TASK-010-W1` remains and should be folded into that final productization Windows matrix rather than run as a disconnected duplicate cycle.
- Short human Renderer Showcase property/button interaction remains desirable but is not blocking the architecture/hygiene work.

## Recovery Log

BASE: `37042388f1c580c9efbd43c38a76c5a8e0f1bd25` / `main`
TASK: P7 / `GPU-PRODUCTIZATION-H1` architecture hygiene and legacy-removal audit, then consolidated Windows/Vulkan acceptance
TOUCHED/INSPECTED: `render/GpuRender/*`, `render/RenderVulkan/RenderVulkan.cpp`, `RenderVulkanSurfaceSession.h`, `RenderVulkanRhi.cpp`, `RenderVulkanRhiBase.inc`, `RenderVulkanTestHooks.h`, multi-surface/presentation tests, package/docs dependency slice
STATUS: P1-P5 published; W1 façade regression PASS; P6a shared logical device + P6b shared pipeline-cache domain published/static-reviewed; P6 platform validation pending; H1 active before final Windows acceptance
PUBLISHED: `f526a3d...`, `f6af476...`, `df03851...`, `3ecccc6...`, `5438c32...`, `bbf0e8b...`, `b7bd3f3...`, `0101df4...`, `319c8b4...`, `3704238...`
VALIDATION: Windows/Vulkan W1 PASS through `b7bd3f3...`; P5/P6/H1 final Windows validation pending
NEXT: re-fetch current `main`; perform H1 evidence-driven cleanup and publish it; then one consolidated Gary Windows/Vulkan acceptance matrix.
