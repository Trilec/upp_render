# Active Work Status

This file is the recovery checkpoint for active `upp_render` implementation work.
Update it whenever a coherent checkpoint is published so work can resume from repository state rather than chat history.

## Accepted Baseline

- Branch: `main`
- Stage 3 Vulkan backend/bootstrap: **PASS / 100% accepted**
- Stage-3 Windows acceptance HEAD: `6ab33a42a3421643359cabfdae7afed7628ad349`
- Stage-3 substantive convergence SHA: `ced346bae602ed6b9b34c8a468c19cc26ffc5c08`
- Stage-3 validation: `RenderVulkanGraphicsTest` Debug 4/4, Release 2/2; resource Debug/Release; RHI Debug/Release; frame and clear regressions all PASS
- Stage-3 Vulkan validation: 0 warnings / 0 errors
- Stage-3 final ownership: runtime/instance/debug messenger/surface/device/swapchain = `0/0/0/0/0/0`
- Stage 4 GPU 2D renderer: **PASS / 100% accepted**
- Stage-4 Windows acceptance HEAD: `f8e7b24d510b4b5889370823dc1c0a5ef43a7f54`
- Stage-4 final compile correction SHA: `17c46c69d9961a9b75da98dd4d3e8c2ff17f678a`
- Stage-4 validation: `RenderGpu2DTest`, `GpuCtrlReplayTest`, and `GpuCtrlPresentationTest` Debug/Release all PASS
- Stage-4 Vulkan validation: 0 warnings / 0 errors
- Stage-4 lifecycle: refresh, resize/recreation, hide/show and GPU readiness PASS; no crash/hang/device loss
- Stage-4 final ownership: runtime/instance/debug messenger/surface/device/swapchain = `0/0/0/0/0/0`

## Current Objective

Proceed directly into Stage 5 — Text, Images and Vector Rendering.

Active implementation task: `TASK-010A` — establish the neutral Stage-5 content contract and first production image/text foundation without disturbing the accepted Stage-3/Stage-4 ownership and primitive renderer.

## Stage 3 - Vulkan Backend / Bootstrap

**100% complete and platform accepted.**

Accepted implementation includes:

- runtime / loader / validation, instance/debug messenger, physical/device/queue setup and Win32 surface
- explicit grouped/default Vulkan ownership with deterministic cleanup
- session-owned swapchain creation/recreation and frame acquire/present
- neutral vertex/fragment shader lifecycle and SPIR-V payload descriptor
- `Position2Color4F` vertex layout and render-pass clear colour
- real `VkShaderModule`, dynamic-rendering graphics pipelines, command recording, vertex binding and `vkCmdDraw`
- synchronous correctness-first queue submission and deterministic resource lifetime guards
- neutral logical surface/swapchain/frame lifecycle mapped onto the existing borrowed `VulkanSurfaceSession`
- acquired swapchain images exposed only as temporary borrowed neutral render targets
- exact negotiated sRGB/UNORM swapchain identity
- transition of rendered swapchain images back to `PRESENT_SRC_KHR`
- session-authoritative presentation and resize recreation
- no second Vulkan runtime/instance/device/surface/swapchain/acquire/present authority

Key publication SHAs:

- S17C-A: `28c303d4859b7b6fdce3380e23fcab68aa84c731`
- B1 merge: `a1e0d84eed295fc4650710aa27d1f777bd149463`
- B1-R2 resource-test update merge: `a2310a8b25ef0cfc58c2f7585f7d43359d9c3e82`
- B2A session handoff merge: `5357c7938dc4b0e067470fc1c188c2b8941a6d53`
- B2-B0 explicit sRGB format merge: `869ab63649ccd6bf4ee4cca2365d58e5a4c3ce86`
- final Stage-3 substantive convergence merge: `ced346bae602ed6b9b34c8a468c19cc26ffc5c08`
- final Stage-3 acceptance HEAD: `6ab33a42a3421643359cabfdae7afed7628ad349`

`TASK-009-W1` Stage-3 evidence:

- required Stage-3 SHA ancestor check: PASS / exit 0
- `RenderVulkanGraphicsTest` Debug: 4/4 PASS
- `RenderVulkanGraphicsTest` Release: 2/2 PASS
- `RenderVulkanResourceTest` Debug/Release: PASS
- `RenderRhiTest` Debug/Release: PASS
- `RenderVulkanFrameTest` Debug: PASS
- `RenderVulkanClearFrameTest` Debug: PASS
- Vulkan validation warnings/errors: 0/0
- final Vulkan ownership: 0/0/0/0/0/0

## Stage 4 - GPU 2D Renderer

**100% complete and platform accepted.**

Accepted renderer scope:

- production backend-neutral `UiRenderer2D`
- direct immutable `UiDisplayList` replay
- filled rectangles
- rectangle strokes/borders
- uniform rounded rectangles
- full affine `ConcatTransform`
- nested `Save`/`Restore`
- cumulative device-space clipping and target-bound clipping
- deterministic convex tessellation to `Position2Color4F`
- compatible primitive batching into one draw
- persistent SPIR-V shaders, pipeline cache and reusable vertex buffer
- explicit straight-alpha SourceOver blending
- live `GpuCtrl` integration through `VulkanSurfaceSession -> VulkanGpuDevice -> UiRenderer2D`
- neutral logical surface/swapchain/frame lifecycle with session-authoritative present
- resize/out-of-date recovery without timer or busy render loop
- software-reference semantic replay of the same immutable Stage-4 display list

Key publication SHAs:

- TASK-009A renderer core: `ca972a087c63a8d54a7f2a9e1683c906b6c747a4`
- TASK-009B blending: `ba2bfcfc76c9e1fd0b6c7c5f3347a22882d41b54`
- TASK-009C live control integration: `b15c7579a0471290dc416131ebe9180a4c14be05`
- TASK-009D semantic parity: `b8a0993fe36eb87a1c99ae6a5d59c9da703f5953`
- TASK-009-W1-R1 compile correction: `17c46c69d9961a9b75da98dd4d3e8c2ff17f678a`

`TASK-009-W1-R1-W2` Stage-4 evidence:

- tested HEAD: `f8e7b24d510b4b5889370823dc1c0a5ef43a7f54`
- R1 ancestor check: PASS / exit 0
- `RenderGpu2DTest` Debug/Release: PASS
- `GpuCtrlReplayTest` Debug/Release: PASS
- `GpuCtrlPresentationTest` Debug/Release: PASS
- Vulkan validation warnings/errors: 0/0
- refresh, resize/recreation, hide/show and GPU readiness: PASS
- no crash, hang or device loss
- final Vulkan ownership: 0/0/0/0/0/0
- final tree: clean; no validator edits/commit/push

## Stage 5 - Text, Images and Vector Rendering

Active now.

Project-plan scope:

- text shaping
- glyph caching
- vector paths
- gradients
- anti-aliasing
- icon and SVG geometry support

Supervisory addition required for a useful UI renderer:

- image sampling/drawing and texture-backed content belong in Stage 5 as well
- regular non-swapchain sRGB texture semantics deferred from Stage 4 must be resolved deliberately here

Implementation direction:

1. `TASK-010A` — neutral content/image foundation and first production image path; resolve sRGB texture semantics consistently in `RenderNull` and Vulkan.
2. `TASK-010B` — shaped text + glyph cache/atlas using U++ text/font authority; GPU consumes shaped glyph placement rather than inventing a second typography system.
3. `TASK-010C` — vector paths, gradients and anti-aliasing, then icon/SVG geometry integration.
4. Stage-5 Windows acceptance across neutral/Null/software/Vulkan/live-control paths.

Do not replace U++ font/theme authority and do not leak Vulkan types into public neutral APIs.

## Recovery Log

BASE: `f8e7b24d510b4b5889370823dc1c0a5ef43a7f54` / `main`
TASK: `TASK-010A` Stage-5 neutral content/image foundation
TOUCHED: status only — `docs/ACTIVE_WORK.md`
STATUS: Stage 3 PASS / 100%; Stage 4 PASS / 100%; Stage 5 ACTIVE
PUBLISHED: Stage-3 convergence `ced346bae602ed6b9b34c8a468c19cc26ffc5c08`; Stage-4 final correction `17c46c69d9961a9b75da98dd4d3e8c2ff17f678a`; Stage-4 acceptance HEAD `f8e7b24d510b4b5889370823dc1c0a5ef43a7f54`
VALIDATION: Stage 3 and Stage 4 platform accepted; Stage 5 not yet validated

## Next Action

Refresh current `main`, inspect the complete Stage-5 dependency slice (`RenderCanvas`, `RenderCore`, `RenderRhi`, `RenderNull`, `RenderVulkan`, `RenderGpu2D`, `RenderSoftware`, tests and package files), then implement and publish `TASK-010A` as the first coherent Stage-5 checkpoint. Preserve the accepted Stage-3/Stage-4 architecture and keep image/text/vector work backend-neutral at the recording layer.
