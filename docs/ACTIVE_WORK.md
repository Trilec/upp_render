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
- Stage-5 vector/gradient/AA/SVG implementation: `0d37b2472c4d49e6908f6acbf5f85cc523193006`; **IMPLEMENTATION COMPLETE — final consolidated Windows/Vulkan acceptance remains** and is folded into the final productization matrix.
- Stage-6 shared presenter/root boundary: **PASS / accepted** (`a4979f17...`, acceptance `cb01a20...`).
- Stage-6 embedded neutral frame source: **PASS / accepted** (`3ac69f1...`; focused package `947a060...`; acceptance `e3ad497...`).
- Stage-6 CtrlCore semantic recording bridge: **PASS / accepted** (`c4210d8...`; acceptance `d386ba1...`).
- Stage-6 root compositor wiring: **PASS / accepted** (`21ca529...`; acceptance `cb01a20...`).
- Renderer Showcase automated renderer coverage + corrected GPU orientation: **PASS**. Short human GUI button/property interaction smoke remains desirable and is included in the final productization validation.

## Active Objective — Final Productization Acceptance

Productization architecture, H1 hygiene and the final source/test closure pass are now **IMPLEMENTATION COMPLETE — PLATFORM VALIDATION PENDING**.

The product surface is:

1. `GpuCtrl` — embedded GPU rectangle inside a normal U++ application.
2. `GpuWindow` — whole custom client area painted by the application through `GpuPainter`.
3. `GpuTopWindow` — U++/upp_Ui control tree recorded and GPU-composited through one root surface.
4. `GpuContext` — shared compatible application GPU ownership so multiple surfaces reuse expensive backend/device state.
5. `GpuRender` — the single ordinary developer package/header; lower `Render*` packages are renderer/backend implementation layers.
6. Backend-neutral provider registration below the façade so Vulkan, Metal and WebGPU are implementation choices rather than application-API assumptions.
7. Compatible Vulkan surfaces share one runtime/instance/logical-device domain and one device-level pipeline cache while keeping surface/swapchain/frame and renderer-owned mutable state independent.

Do not start another architecture pass unless consolidated Windows/Vulkan validation reveals a real defect.

## Published Productization Checkpoints

### P1 — public painter façade

- `f526a3d1208bbab1a47ac698757950cc6811075c` — introduced `GpuPainter`, `GpuWindow`, `GpuCtrl::SetGpuPaint()` and the `GpuRender` façade package.
- `3ecccc677aaff1c61a37932407e6e0ac3e534161` — completed the size-aware painter surface, subclass/callback path, advanced `WhenBuildFrame` precedence and empty default frame behavior.

### P2 — shared application context foundation

- `f6af476448f8853490f72c36ca5ebceb039f6d59` — added backend-neutral `GpuContext` and routed ordinary presenters through `GpuContext::Default()`.

### P3 — physical U++ integration/package consolidation

- `df03851aad2ba0a94d27b2e0d9c3e28ba75da252` — consolidated the former `GpuCtrl`, `GpuTopWindow`, `RenderPresentation`, and `RenderCtrlBridge` packages under `render/GpuRender` and migrated in-repo consumers.
- The old top-level integration directories are removed.
- `render/` now contains only the public façade plus deliberate engine/backend layers: `GpuRender`, `RenderCanvas`, `RenderCore`, `RenderGpu2D`, `RenderNull`, `RenderPlatformWin32`, `RenderRhi`, `RenderSoftware`, `RenderVector`, `RenderVulkan`.

### P4 — canonical examples and public surface

- `5438c32cbd52322e78b8b34300dc73d30e81c45f` — canonical `GpuRenderEmbedded`, `GpuRenderWindow`, `GpuRenderUiWindow`; lifecycle/bring-up examples moved under diagnostics.
- `bbf0e8baab086e995f4d9f752628b15819e84109` — public docs/backend-roadmap checkpoint.
- `b7bd3f35190e57dead2edd4b687e6574d32f6a91` — Windows mechanical compatibility fixes.

`GPU-PRODUCTIZATION-W1` Windows/Vulkan result through `b7bd3f3...`: façade/header, canonical examples, embedded/root/bridge regressions, Vulkan graphics/image/text/vector, GPU2D and Renderer Showcase requested builds/runs **PASS**; no Vulkan validation errors/crashes/assertions/blank surfaces.

### P5 — backend registration / decoupling

- `0101df4bb8d11457c616af38a69f00c7e3556bf4` — initial backend-neutral provider registry / Vulkan provider seam.
- `GpuBackendRegistryTest` is the focused registration authority.

### P6 — shared Vulkan logical-device/resource domain

#### P6a — shared logical device + queues

- `319c8b4880520a546bd365316fd42b4e2fd35bf0` — compatible `VulkanSurfaceSession`s share one ref-counted logical-device entry.
- Each session retains its own surface, swapchain, frame state and selected queue views.
- Shared-device creation requests one queue from every usable queue family so the first surface does not specialize the device to only its own present requirements.
- Non-final close releases only that session's lease; final close destroys the shared device.

#### P6b — shared safe device resources

- `37042388f1c580c9efbd43c38a76c5a8e0f1bd25` — one shared device-owned `VkPipelineCache` for compatible sessions.
- Both ordinary and sampled `vkCreateGraphicsPipelines` paths use that cache.
- Grouped acceptance requires both sessions to expose the same non-null cache and the survivor to retain it after the other surface closes.
- Image/glyph RHI handles and `UiRenderer2D` caches remain presenter-owned; they are not globally shared without an explicit identity/lifetime model.
- P6 completion checkpoint: `40703a4c6c87f2aafe1f03ff76679004b9693f3c`.

Expected compatible two-surface ownership:

```text
runtime create/live while active  = 1 / 1
instance create/live while active = 1 / 1
logical device create/live        = 1 / 1
shared pipeline cache             = one identical non-null handle
surfaces live                      = 2
swapchains                         = 2 independent
non-final close                    = device/cache survive
final close                        = all live counts return to zero
```

## P7 — GPU-PRODUCTIZATION-H1 Architecture Hygiene / Legacy Removal

### H1a — provider/dependency boundary cleanup

- `d231cf88b325412a6fbea9626fc7f76e68236fba` — **IMPLEMENTED / PUBLISHED**.
- recovery checkpoint `e96bd6f244f81573d0fe2bf8f762642a64726a4a`.
- Removed migration-only `GpuRender/RenderPresentationBackend.h` and façade-owned Vulkan provider implementation.
- Neutral provider context/session/registry now live under `RenderRhi`.
- Vulkan provider implementation/registration lives in `RenderVulkan/RenderVulkanPresentation.cpp`.
- `GpuDisplayPresenter` owns backend-neutral `UiRenderer2D`, logical surface/swapchain, resize and present orchestration once.
- Vulkan provider owns only `VulkanSurfaceSession` + `VulkanGpuDevice` and exposes the latter through the neutral contract.
- Added neutral `GpuDevice::GetLastError()` and `GpuResult::OutOfDate`; generic presentation no longer inspects Vulkan reports.
- `GpuRender.upp` intentionally still pulls `RenderVulkan` so one public package links the current Windows/Vulkan provider. Generic/public source contains no Vulkan implementation dependency.

### H1b — synchronization, stale acceptance and maintenance residue

- `9da8f8c416edff3a23f2c1992e27c421617a100e` — **IMPLEMENTED / PUBLISHED — PLATFORM VALIDATION PENDING**.
- Per-surface frame/swapchain teardown now uses `vkQueueWaitIdle` on that session's graphics/present queues instead of `vkDeviceWaitIdle`.
- The final shared-device destruction deliberately retains `vkDeviceWaitIdle` before destroying device-owned resources.
- A newly created swapchain that fails before any submit/present is destroyed directly; no idle boundary is needed for that rollback.
- Swapchain cleanup reporting is no longer incorrectly poisoned by an injected final-device cleanup failure.
- `vkQueueWaitIdle` is resolved/validated as a required device procedure and has focused missing-procedure coverage.
- `RenderVulkanTest` stale two-device grouped-surface assertion was corrected to one shared logical device.
- Retained `RenderGpu2DBase.inc` and `RenderVulkanRhiBase.inc` intentionally: wrapper/base implementation shares private state in one translation unit; splitting now would introduce another private API without simplifying ownership.
- Diagnostic examples remain under `examples/diagnostics` because they provide lifecycle, auto-close, validation and zero-resource accounting not duplicated by canonical user examples.
- Public architecture/usage/backend/demo/project docs were updated to the final shared-device/provider model.

### Final source/test closure

- `778f95d03c06ff95b86d8fbec5bee6353face382` — H1 productization-hygiene checkpoint.
- `2186c391cbe404ad044ba270643d6a828887745a` — corrected the remaining product-level `GpuCtrlPresentationTest` pre-P6 two-device assumption.
- `GpuCtrlPresentationTest` now requires two independent surfaces/swapchains with one shared logical device and verifies that idle processing, explicit refresh, resize and hide/show do not recreate that device.
- The lower-level grouped Vulkan authority already requires one shared logical device, one identical non-null pipeline cache, independent swapchains, survivor retention and final zero ownership.
- `GpuCtrlMultiViewDemo` has no stale isolated-device assumption; its auto-close path remains a lifecycle/zero-resource diagnostic.
- No open pull requests remain. The current `main` tree contains no temporary workflow directory or merged review-workflow residue.

### H1 whole-repository static audit

**PASS** on the reviewed source closure tree before platform validation:

- published diffs reviewed for the final acceptance correction;
- package membership for `GpuCtrlPresentationTest` remains `Core`, `CtrlLib`, `GpuRender`, `RenderVulkan`;
- no stale pre-consolidation includes or `.upp` dependencies for `GpuCtrl`, `GpuTopWindow`, `RenderPresentation`, `RenderCtrlBridge` packages;
- old top-level integration directories absent;
- no direct `RenderVulkan` include from generic `render/GpuRender` source;
- no reverse `RenderRhi` / `RenderVulkan` source include of `GpuRender`;
- grouped Vulkan and product-level multi-control acceptance now both require one shared logical device;
- retained base `.inc` files are intentional implementation-sharing boundaries rather than migration residue;
- full productization series reviewed from pre-P1 parent `9bd73497a7a4aef779a031b09cffbb65711c5d45` through the final source/test closure.

Temporary execution/review workflows and PRs were not merged into `main`.

H1/source result: **IMPLEMENTATION COMPLETE — PLATFORM VALIDATION PENDING**.

## Backend Direction

### Vulkan

Current production/validation backend. After the final productization acceptance, follow-up should be hardening/output parity/device-loss work rather than another ownership redesign unless validation demonstrates a root architectural defect.

### Metal

First-class next backend target. Public `GpuPainter`, `GpuCtrl`, `GpuWindow`, `GpuTopWindow`, `GpuContext`, display-list and RHI contracts contain no Objective-C/Metal types. macOS is the primary bring-up target while retaining iOS/iPadOS viability.

### WebGPU

First-class next backend target. Browser/WebAssembly hosting remains a longer-term goal and needs a U++ browser platform host in addition to a WebGPU renderer provider.

Do not create placeholder Metal/WebGPU implementations before real bring-up slices exist.

## Remaining Acceptance / Debt

- Source implementation, backend decoupling, shared Vulkan device/cache ownership and H1 cleanup are **complete and published**.
- The only active completion gate is the consolidated Windows/Vulkan validation of P5/P6/H1 plus the folded Stage-5 vector gate and Renderer Showcase interaction smoke.
- No additional architecture/source cleanup is scheduled unless that validation exposes a substantive defect.

## Recovery Log

BASE: `2186c391cbe404ad044ba270643d6a828887745a` / `main`
TASK: consolidated Windows/Vulkan productization acceptance, including P5/P6/H1 and Stage-5 final vector gate
TOUCHED/INSPECTED: complete productization delta from `9bd73497...`; `render/GpuRender/*`, `render/RenderRhi/*`, `render/RenderVulkan/*`, `render/RenderGpu2D/*`, package files, focused tests, canonical/diagnostic examples, public architecture/usage/roadmap docs; final `GpuCtrlPresentationTest` product acceptance correction
STATUS: P1-P6 published; H1a/H1b published; final source/test closure published; whole-repository static audit PASS; **IMPLEMENTATION COMPLETE — PLATFORM VALIDATION PENDING**
PUBLISHED: `f526a3d...`, `f6af476...`, `df03851...`, `3ecccc6...`, `5438c32...`, `bbf0e8b...`, `b7bd3f3...`, `0101df4...`, `319c8b4...`, `3704238...`, `40703a4...`, `d231cf8...`, `e96bd6f...`, `9da8f8c...`, `778f95d...`, `2186c39...`
VALIDATION: Windows/Vulkan W1 PASS through `b7bd3f3...`; H1/source static audit PASS through `2186c39...`; final P5/P6/H1 + Stage-5 consolidated Windows/Vulkan validation pending
NEXT: refresh current `main`; run one consolidated Windows/Vulkan acceptance matrix on the published source-closure checkpoint; if clean, record final productization acceptance and move to the next hardening/backend stage.
