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
- Earlier S17B accepted baseline retained: S17A `5d7e5e2537a7fd70bd9d344c9cb885a04014c041`; S17B `07e7870ae91316305f84a9fdc32b7488fd37eb3a`; S17B-R1 `6d36c102dcc30b79bf61156a1aec2d77bd598ecc`

## Current Objective

Complete the narrow Stage-4 Windows re-acceptance after `TASK-009-W1-R1` fixed the one production compile blocker found by the first combined run. Do not repeat Stage 3 unless a new Stage-3-affecting change is introduced.

Active validation task: `TASK-009-W1-R1-W2` — Stage-4 Windows re-acceptance on current `main`.

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

Implementation is source-complete. Platform acceptance is pending one narrow re-run after `TASK-009-W1-R1`.

Published `TASK-009A` renderer core:

- backend-neutral `render/RenderGpu2D` package with production `UiRenderer2D`
- direct replay of immutable `UiDisplayList`, not another private drawing authority
- filled rectangles
- rectangle strokes/borders
- uniform rounded rectangles
- full affine `ConcatTransform`
- nested `Save`/`Restore`
- cumulative device-space clipping and target-bound clipping
- deterministic convex tessellation to `Position2Color4F`
- compatible solid primitives batched into one draw
- persistent SPIR-V shader pair
- pipeline cache by render-target format
- grow/reuse persistent vertex buffer
- Rgba alpha retained in vertex data
- clear-only frames issue no geometry draw
- focused `RenderGpu2DTest` through `RenderNull`
- TASK-009A merge: `ca972a087c63a8d54a7f2a9e1683c906b6c747a4`

Published `TASK-009B` alpha/blending:

- explicit neutral `GpuBlendMode::Opaque` / `GpuBlendMode::SourceOver`
- `UiRenderer2D` explicitly requests source-over blending
- Vulkan mapping uses straight-alpha `SRC_ALPHA / ONE_MINUS_SRC_ALPHA` colour blending and `ONE / ONE_MINUS_SRC_ALPHA` alpha blending
- deterministic Null inspection proves renderer pipelines request SourceOver
- TASK-009B merge: `ba2bfcfc76c9e1fd0b6c7c5f3347a22882d41b54`

Published `TASK-009C` live control integration:

- retired S16 private FillRect/translation-only replay authority removed from `GpuCtrl`
- live `GpuCtrl` uses one `VulkanSurfaceSession`, a borrowing `VulkanGpuDevice`, and a borrowing `UiRenderer2D`
- live paint follows `BeginFrame -> UiRenderer2D::RenderFrame -> Submit -> Present`
- neutral logical surface/swapchain/frame handles own adapter lifecycle while Vulkan session remains the sole Vulkan ownership authority
- resize uses neutral `ResizeSwapchain`; out-of-date recovery is one bounded recreate/retry, not a timer/render loop
- teardown reverses borrowing order: renderer -> logical swapchain/surface -> adapter -> session
- live immutable scene exercises opaque/translucent fills, translucent stroke, rounded rectangle, device-space clip, Save/Restore and a genuine scale/shear/translation affine transform
- `GpuCtrlReplayTest` validates that same live scene through production `UiRenderer2D` + `RenderNull`
- existing `GpuCtrlPresentationTest` exercises the production renderer path while retaining two-control/resize/refresh/hide-show/ownership coverage
- TASK-009C merge: `b15c7579a0471290dc416131ebe9180a4c14be05`

Published `TASK-009D` semantic parity closure:

- the same representative Stage-4 immutable display list replays through `SoftwareUiRenderer` and `UiRenderer2D`
- software reference replay must succeed and visibly alter the reference image
- both replays preserve the immutable display-list dump
- GPU-contract path retains full primitive/state accounting, one-draw batching, SourceOver pipeline state and persistent resource reuse evidence
- exact GPU pixel readback remains Stage-8 hardening, where the project plan places software/GPU output comparison
- TASK-009D merge / Stage-4 source checkpoint: `b8a0993fe36eb87a1c99ae6a5d59c9da703f5953`

`TASK-009-W1` first Stage-4 attempt:

- Stage-4 SHA ancestor check: PASS / exit 0
- `RenderGpu2DTest` Debug/Release: built successfully
- first blocker: `GpuCtrlReplayTest` compile failure in production `render/GpuCtrl/GpuCtrl.cpp`
- cause: direct `RoundedRect(Rectf, radius)` construction collided with CtrlLib `RoundedRect(...)` free functions in the same `Upp` namespace
- classification: narrow production compile defect; no renderer/RHI architecture change required
- `GpuCtrlPresentationTest` was not run because validation correctly stopped at the first blocker

Published `TASK-009-W1-R1` correction:

- `render/GpuCtrl/GpuCtrl.cpp` now uses the established elaborated-type pattern: `struct RoundedRect rounded(...)`, then passes the local value to `FillRoundedRect`
- no RHI, Vulkan ownership, renderer semantics, display-list semantics, package membership, tests or public API changed
- correction SHA: `17c46c69d9961a9b75da98dd4d3e8c2ff17f678a`

Stage-4 completion gate now:

- `17c46c69d9961a9b75da98dd4d3e8c2ff17f678a` is an ancestor of tested current HEAD
- `RenderGpu2DTest` Debug/Release run and PASS
- `GpuCtrlReplayTest` Debug/Release build and PASS
- real Vulkan `GpuCtrlPresentationTest` Debug/Release build and PASS through resize/refresh/hide-show
- Vulkan validation warnings/errors = 0/0 for the real presentation path
- no crash, hang, device loss, unexpected idle recreation, or ownership leak
- final Vulkan ownership diagnostics = 0

Non-blocking observation from `TASK-009-W1`:

- `RenderNull.cpp` warns that `RGBA8Srgb` / `BGRA8Srgb` are not handled by `BytesPerPixel`
- this is intentionally not folded into the R1 compile correction: adding those cases would expand regular non-swapchain sRGB texture-upload semantics that are already deferred to Stage 5 image/texture work
- resolve that behavior deliberately in Stage 5 rather than silently changing it during Stage-4 acceptance

## Recovery Log

BASE: `6ab33a42a3421643359cabfdae7afed7628ad349` / `main`
TASK: `TASK-009-W1-R1` repair first Stage-4 Windows compile blocker and re-run Stage-4 acceptance
TOUCHED: `render/GpuCtrl/GpuCtrl.cpp`; status — `docs/ACTIVE_WORK.md`
STATUS: Stage 3 PASS / 100%; Stage 4 IMPLEMENTATION COMPLETE — PLATFORM REVALIDATION PENDING
PUBLISHED: Stage-3 convergence `ced346bae602ed6b9b34c8a468c19cc26ffc5c08`; Stage-4 A `ca972a087c63a8d54a7f2a9e1683c906b6c747a4`; B `ba2bfcfc76c9e1fd0b6c7c5f3347a22882d41b54`; C `b15c7579a0471290dc416131ebe9180a4c14be05`; D `b8a0993fe36eb87a1c99ae6a5d59c9da703f5953`; R1 `17c46c69d9961a9b75da98dd4d3e8c2ff17f678a`
VALIDATION: Stage 3 accepted by `TASK-009-W1`; Stage 4 stopped at production compile blocker on pre-R1 HEAD; corrected-tree Stage-4 revalidation pending

## Next Action

Gary runs `TASK-009-W1-R1-W2` against current `main`. Confirm `17c46c69d9961a9b75da98dd4d3e8c2ff17f678a` is an ancestor, then run Stage-4 tests only: `RenderGpu2DTest` Debug/Release, `GpuCtrlReplayTest` Debug/Release, and `GpuCtrlPresentationTest` Debug/Release. Stop on the first genuine blocker. No edits, commits or pushes are needed for this validation. If clean, mark Stage 4 100% and move directly to Stage 5.
