# Active Work Status

This file is the recovery checkpoint for active `upp_render` implementation work.
Update it whenever a coherent checkpoint is published so work can resume from repository state rather than chat history.

## Accepted Baseline

- Branch: `main`
- Platform-accepted through: `TASK-008A1-S17B-R1`
- S17A neutral-contract SHA: `5d7e5e2537a7fd70bd9d344c9cb885a04014c041`
- S17B implementation SHA: `07e7870ae91316305f84a9fdc32b7488fd37eb3a`
- S17B-R1 correction SHA: `6d36c102dcc30b79bf61156a1aec2d77bd598ecc`
- S17B Windows acceptance HEAD: `dbd76e60ef090ca440461b8d3e7a1ab0f2a96e39`
- S17B Windows result: PASS — focused resource Debug 4/4, Release 2/2, all named regressions passed
- S17B validation: 0 Vulkan warnings, 0 Vulkan errors; final tracked Vulkan ownership counts zero

## Current Objective

Run one combined Windows/runtime acceptance of the source-complete Stage 3 Vulkan backend and Stage 4 GPU 2D renderer. If clean, mark both stages 100% and move to Stage 5.

Active validation task: `TASK-009-W1` — combined Stage-3/Stage-4 Windows acceptance.

## Stage 3 - Vulkan Backend / Bootstrap

Implementation complete, platform validation pending.

Published S17C convergence includes:

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

Stage-3 completion gate:

- `ced346bae602ed6b9b34c8a468c19cc26ffc5c08` is an ancestor of tested current HEAD
- focused Vulkan graphics Debug/Release passes
- resource/RHI/frame/clear regressions pass
- GpuCtrl real presentation path remains clean
- Vulkan validation warnings/errors = 0/0
- final runtime/instance/debug/surface/device/swapchain ownership diagnostics = 0
- neutral public headers remain free of Vulkan types

## Stage 4 - GPU 2D Renderer

Implementation complete, platform validation pending.

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
- existing `GpuCtrlPresentationTest` now exercises the production renderer path while retaining two-control/resize/refresh/hide-show/ownership coverage
- TASK-009C merge: `b15c7579a0471290dc416131ebe9180a4c14be05`

Published `TASK-009D` semantic parity closure:

- the same representative Stage-4 immutable display list replays through `SoftwareUiRenderer` and `UiRenderer2D`
- software reference replay must succeed and visibly alter the reference image
- both replays preserve the immutable display-list dump
- GPU-contract path retains full primitive/state accounting, one-draw batching, SourceOver pipeline state and persistent resource reuse evidence
- exact GPU pixel readback remains Stage-8 hardening, where the project plan places software/GPU output comparison
- TASK-009D merge / final Stage-4 source checkpoint: `b8a0993fe36eb87a1c99ae6a5d59c9da703f5953`

Stage-4 completion gate:

- `b8a0993fe36eb87a1c99ae6a5d59c9da703f5953` is an ancestor of tested current HEAD
- `RenderGpu2DTest` Debug/Release passes
- `GpuCtrlReplayTest` Debug/Release passes
- real Vulkan `GpuCtrlPresentationTest` Debug/Release passes through resize/refresh/hide-show
- Vulkan validation warnings/errors = 0/0
- no crash, hang, device loss, unexpected idle recreation, or ownership leak
- final Vulkan ownership diagnostics = 0

Regular non-swapchain sRGB texture-upload parity in `RenderNull` remains deferred to Stage 5 image/texture work. It is not part of Stage-4 solid-primitive rendering.

## Recovery Log

BASE: `b8a0993fe36eb87a1c99ae6a5d59c9da703f5953` / `main`
TASK: `TASK-009-W1` combined Stage-3/Stage-4 Windows acceptance
TOUCHED: latest source checkpoint — `tests/RenderGpu2DTest/RenderGpu2DTest.upp`, `tests/RenderGpu2DTest/main.cpp`; status — `docs/ACTIVE_WORK.md`
STATUS: IMPLEMENTATION COMPLETE — PLATFORM VALIDATION PENDING
PUBLISHED: Stage-3 convergence `ced346bae602ed6b9b34c8a468c19cc26ffc5c08`; Stage-4 A `ca972a087c63a8d54a7f2a9e1683c906b6c747a4`; B `ba2bfcfc76c9e1fd0b6c7c5f3347a22882d41b54`; C `b15c7579a0471290dc416131ebe9180a4c14be05`; D/final source checkpoint `b8a0993fe36eb87a1c99ae6a5d59c9da703f5953`
VALIDATION: source/static and aggregate PR reviews complete for Stage 3 and TASK-009A/B/C/D; Windows/runtime/Vulkan acceptance pending

## Next Action

Gary runs `TASK-009-W1` against current `main`, confirming both `ced346bae602ed6b9b34c8a468c19cc26ffc5c08` and `b8a0993fe36eb87a1c99ae6a5d59c9da703f5953` are ancestors of current HEAD. Validate Stage 3 first so its result can be classified independently, then run Stage-4 renderer/control tests. Stop on the first genuine compile/runtime/validation/ownership blocker and report exact evidence. No edits, commits or pushes. If both gates pass, mark Stage 3 and Stage 4 100% and move directly to Stage 5.
