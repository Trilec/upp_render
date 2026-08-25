# Active Work Status

Remote `main` is authoritative. This file is the recovery checkpoint for `upp_render`.

## Accepted Product Baseline

The Vulkan productization phase is **PASS / accepted** through source acceptance HEAD `7e9b5cdacf76acef56da4943a228f0281d1dde95`.

Accepted stages:

- Stage 1 backend-neutral display-list/software foundation: **PASS**.
- Stage 2 RHI + Null validation backend: **PASS**.
- Stage 3 Vulkan bootstrap/resources/presentation: **PASS**.
- Stage 4 GPU 2D renderer: **PASS**.
- Vulkan framebuffer Y-orientation correction: **PASS**.
- Stage 5 image path: **PASS**.
- Stage 5 text/glyph atlas: **PASS**.
- Stage 5 vector/gradient/AA/SVG: **PASS** after the final consolidated Windows/Vulkan gate.
- Stage 6 shared presenter/root boundary: **PASS**.
- Stage 6 embedded neutral frame source: **PASS**.
- Stage 6 CtrlCore semantic recording bridge: **PASS**.
- Stage 6 root compositor wiring: **PASS**.
- Renderer Showcase automated + human smoke: **PASS**.

## Current Product Surface

Application-facing API:

1. `GpuRender` — single ordinary developer package/header.
2. `GpuPainter` — backend-neutral application drawing surface.
3. `GpuCtrl` — bounded GPU surface embedded inside a normal U++ UI.
4. `GpuWindow` — whole custom client area rendered through `GpuPainter`.
5. `GpuTopWindow` — ordinary U++ control tree recorded and composited through one root GPU surface.
6. `GpuContext` — shared compatible application GPU ownership.

Architecture:

- neutral display-list / painter intent remains above all backends;
- backend selection/lifetime is routed through the neutral provider registry under `RenderRhi`;
- Vulkan provider implementation/registration lives under `RenderVulkan`;
- generic `GpuRender` presentation contains no Vulkan implementation dependency;
- compatible Vulkan surfaces share one runtime, instance, logical device, queue domain and device-level pipeline cache;
- each surface retains independent surface/swapchain/frame lifecycle;
- renderer-owned mutable image/glyph caches remain presenter-owned until an explicit cross-surface resource identity/lifetime model exists;
- final shared-device destruction uses `vkDeviceWaitIdle`; per-surface teardown uses the relevant queue-idle boundary.

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

## Productization Checkpoints

- P1 public painter façade: `f526a3d1208bbab1a47ac698757950cc6811075c`, `3ecccc677aaff1c61a37932407e6e0ac3e534161`.
- P2 shared application context: `f6af476448f8853490f72c36ca5ebceb039f6d59`.
- P3 package consolidation into `GpuRender`: `df03851aad2ba0a94d27b2e0d9c3e28ba75da252`.
- P4 canonical examples/docs: `5438c32cbd52322e78b8b34300dc73d30e81c45f`, `bbf0e8baab086e995f4d9f752628b15819e84109`, Windows compatibility `b7bd3f35190e57dead2edd4b687e6574d32f6a91`.
- P5 backend registration/decoupling: `0101df4bb8d11457c616af38a69f00c7e3556bf4`.
- P6a shared logical device/queues: `319c8b4880520a546bd365316fd42b4e2fd35bf0`.
- P6b shared pipeline cache: `37042388f1c580c9efbd43c38a76c5a8e0f1bd25`; completion checkpoint `40703a4c6c87f2aafe1f03ff76679004b9693f3c`.
- H1 provider/dependency cleanup: `d231cf88b325412a6fbea9626fc7f76e68236fba`.
- H1 synchronization/maintenance cleanup: `9da8f8c416edff3a23f2c1992e27c421617a100e`.
- H1 source/test closure: `778f95d03c06ff95b86d8fbec5bee6353face382`, `2186c391cbe404ad044ba270643d6a828887745a`.
- Windows `One<T>::Get()` mechanical provider fix: `1abfb232c772ee91f2ab252ea9a668680a6e51c0`.
- Focused post-create cleanup regression package: `6954fb86e7e35aa244178c3cfd828b0dc75c2b43`, `4128b6c36ec09e3a589bb641c998c35a3e95d01d`.
- Post-create shared-device cleanup outcome fix: `7e9b5cdacf76acef56da4943a228f0281d1dde95`.

## Final Windows/Vulkan Acceptance — GPU-PRODUCTIZATION-W2-R1

**PASS** on `7e9b5cdacf76acef56da4943a228f0281d1dde95`.

Debug PASS:

- `GpuBackendRegistryTest`
- `RenderRhiTest`
- `RenderSampledRhiTest`
- `RenderVulkanPostCreateCleanupTest`
- `RenderVulkanTest`
- `RenderVulkanResourceTest`
- `RenderVulkanGraphicsTest`
- `RenderVulkanImageTest`
- `RenderVulkanTextTest`
- `RenderVulkanVectorTest`
- `RenderGpu2DTest`
- `RenderGpuVectorTest`
- `RenderVectorTest`
- `GpuCtrlFrameSourceTest`
- `GpuCtrlPresentationTest`
- `GpuCtrlReplayTest`
- `GpuTopWindowPresentationTest`
- `RenderCtrlBridgeTest`
- `RendererShowcaseTest`

Canonical Debug examples built/opened/closed cleanly:

- `GpuRenderEmbedded`
- `GpuRenderWindow`
- `GpuRenderUiWindow`

Release PASS:

- `GpuBackendRegistryTest`
- `RenderRhiTest`
- `RenderVulkanPostCreateCleanupTest`
- `RenderVulkanTest`
- `GpuCtrlPresentationTest`
- `GpuTopWindowPresentationTest`
- `RenderVulkanGraphicsTest`
- `RenderVulkanImageTest`
- `RenderVulkanTextTest`
- `RenderVulkanVectorTest`

Canonical Release examples compile/link PASS.

Renderer Showcase Debug/Release builds and process smoke PASS. Debug remained live through GPU/software/reset interaction and large/small resize operations. No crash, assertion, blank-surface failure or Vulkan validation failure was observed. A valid screenshot was not captured because another foreground window obscured the capture surface; this is not a product blocker because the interaction/liveness gate and prior orientation acceptance both passed.

Final live Vulkan ownership returned to zero.

Accepted final labels:

- **GPU-PRODUCTIZATION-W2-R1: PASS**
- **Post-create cleanup reporting: PASS**
- **P5 backend registration: PASS**
- **P6 shared Vulkan logical device/resources: PASS**
- **H1 architecture hygiene: PASS**
- **Stage-5 final vector gate: PASS**
- **Renderer Showcase final smoke: PASS**
- **Final live Vulkan ownership: ZERO**

Existing non-blocking compiler warning remains in preserved `RenderGpu2DBase.inc` enum handling. Do not create a validation cycle solely to silence it.

## Backend Direction

### Vulkan

Current accepted production backend. Next Vulkan work should be hardening, device-loss/recovery, output parity and performance/resource-cache evolution rather than another ownership redesign unless evidence demonstrates a root architectural defect.

### Metal

First-class next backend target. Public `GpuPainter`, `GpuCtrl`, `GpuWindow`, `GpuTopWindow`, `GpuContext`, display-list and RHI contracts contain no Objective-C/Metal types. Bring up macOS first while retaining iOS/iPadOS viability in platform decisions.

### WebGPU

First-class future backend target. The renderer provider can sit behind the same registry/RHI seam. A true U++ web application additionally requires a browser/WebAssembly CtrlCore platform host for event loop, input, focus, timers, text/IME, clipboard, cursor and canvas/window hosting.

Do not add placeholder Metal/WebGPU backends. Start each as a real bring-up slice with executable evidence.

## Next Bounded Task

Begin the next backend/hardening phase from this accepted baseline. Preferred order:

1. define and implement the first real Metal provider bring-up slice without changing public drawing APIs;
2. keep Vulkan regression coverage as the compatibility authority while adding backend-neutral provider tests;
3. separately define the browser host requirements before WebGPU implementation so renderer work is not confused with the missing U++ browser platform layer;
4. continue Vulkan hardening only where it supports cross-backend correctness or exposes a real runtime defect.

## Recovery Log

BASE: `7e9b5cdacf76acef56da4943a228f0281d1dde95` / `main` source acceptance baseline
TASK: Vulkan productization phase closed; next bounded work is real Metal provider bring-up planning/implementation
TOUCHED/INSPECTED: `render/GpuRender/*`, `render/RenderRhi/*`, `render/RenderVulkan/*`, shared-device/provider ownership paths, focused/product acceptance tests, canonical examples, Renderer Showcase, public architecture/recovery docs
STATUS: **PASS / ACCEPTED — PRODUCTIZATION PHASE CLOSED**
PUBLISHED: through `7e9b5cdacf76acef56da4943a228f0281d1dde95` plus this recovery checkpoint
VALIDATION: GPU-PRODUCTIZATION-W2-R1 full Debug/Release matrix PASS; Showcase final smoke PASS; no Vulkan validation errors; final live Vulkan ownership ZERO
NEXT: refresh `main`; begin the first real Metal provider bring-up slice from the neutral registry/RHI seam; preserve Vulkan as the accepted regression baseline.
