# Active Work Status

This file is the recovery checkpoint for active `upp_render` implementation work.
Update it whenever a coherent checkpoint is published so work can resume from repository state rather than chat history.

## Accepted Baseline

- Branch: `main`
- Platform-accepted through: `TASK-008A1-S17B-R1`
- S17A neutral-contract SHA: `5d7e5e2537a7fd70bd9d344c9cb885a04014c041`
- S17B implementation SHA: `07e7870ae91316305f84a9fdc32b7488fd37eb3a`
- S17B-R1 correction SHA: `6d36c102dcc30b79bf61156a1aec2d77bd598ecc`
- Windows acceptance HEAD: `dbd76e60ef090ca440461b8d3e7a1ab0f2a96e39`
- Windows result: PASS — focused resource Debug 4/4, Release 2/2, all named regressions passed
- Validation: 0 Vulkan warnings, 0 Vulkan errors; final tracked Vulkan ownership counts zero

## Current Objective

1. Complete one focused Windows/runtime acceptance of the published S17C Stage-3 convergence checkpoint and mark Vulkan bootstrap complete if clean.
2. Complete Stage 4 GPU 2D rendering in large recoverable slices while that platform validation runs in parallel.

Active validation task: `TASK-008A1-S17C-W1` — accept the published neutral graphics + surface/swapchain/frame convergence on Windows.
Active implementation task: `TASK-009C` — migrate the live `GpuCtrl` proof to `VulkanGpuDevice` + `UiRenderer2D`, then run combined Stage-3/Stage-4 acceptance.

## Stage 3 Closure Gate

Accepted and retained from the platform-validated baseline:

- runtime / loader / validation, instance/debug messenger, device/queues, Win32 surface
- grouped/default Vulkan ownership and deterministic cleanup
- session-owned swapchain creation/recreation and frame acquire/present
- embedded `GpuCtrl` lifecycle and presentation path
- neutral buffer/texture upload contract plus `RenderNull` validation authority
- real Vulkan buffers and row-pitch-aware texture uploads
- S17B-R1 Windows acceptance with validation 0/0 and final Vulkan ownership zero

Published in S17C-A/B, platform validation pending final S17C acceptance:

- neutral vertex/fragment shader lifecycle and SPIR-V payload descriptor
- `Position2Color4F` vertex layout and render-pass clear colour
- real `VkShaderModule`, dynamic-rendering graphics pipelines, command recording, vertex binding and `vkCmdDraw`
- synchronous correctness-first queue submission and deterministic resource lifetime guards
- neutral logical surface/swapchain/frame lifecycle mapped onto the existing borrowed `VulkanSurfaceSession`
- acquired swapchain images exposed only as temporary borrowed neutral render targets
- exact negotiated sRGB/UNORM swapchain identity, present-layout transition, session-authoritative presentation and resize recreation
- S17C-A SHA: `28c303d4859b7b6fdce3380e23fcab68aa84c731`
- B1 publication merge: `a1e0d84eed295fc4650710aa27d1f777bd149463`
- B1-R2 resource-test update merge: `a2310a8b25ef0cfc58c2f7585f7d43359d9c3e82`
- B2A session handoff merge: `5357c7938dc4b0e067470fc1c188c2b8941a6d53`
- B2-B0 explicit sRGB format merge: `869ab63649ccd6bf4ee4cca2365d58e5a4c3ce86`
- B2-B1 publication merge: `ced346bae602ed6b9b34c8a468c19cc26ffc5c08`

Still required before declaring Stage 3 complete:

- focused Windows Debug/Release build and runtime acceptance of the combined S17C checkpoint
- `RenderVulkanGraphicsTest` repeated passes with validation 0 warnings / 0 errors
- resource/frame/clear/GpuCtrl regressions clean
- final Vulkan runtime/instance/debug/surface/device/swapchain ownership diagnostics zero
- neutral public headers remain free of Vulkan types

## Stage 4 GPU 2D Renderer

Published in `TASK-009A`:

- new backend-neutral `render/RenderGpu2D` package with `UiRenderer2D`
- direct replay of immutable `UiDisplayList` operations rather than another private drawing authority
- filled rectangles, rectangle strokes and uniform rounded rectangles
- full affine `ConcatTransform`, nested `Save`/`Restore`, cumulative device-space clipping and target-bound clipping
- deterministic convex tessellation into one `Position2Color4F` batch/draw
- persistent SPIR-V shader pair, pipeline cache by render-target format and grow/reuse vertex buffer
- exact Rgba alpha retained in the vertex stream; clear-only frames issue no geometry draw
- focused `RenderGpu2DTest` through `RenderNull` covers geometry/state, one-draw batching, persistent resource reuse and cleanup
- TASK-009A merge: `ca972a087c63a8d54a7f2a9e1683c906b6c747a4`

Published in `TASK-009B`:

- explicit neutral `GpuBlendMode::Opaque` / `GpuBlendMode::SourceOver` pipeline state
- `UiRenderer2D` explicitly requests source-over blending
- Vulkan maps source-over to straight-alpha `SRC_ALPHA / ONE_MINUS_SRC_ALPHA` colour blending and `ONE / ONE_MINUS_SRC_ALPHA` alpha blending
- `RenderNull` exposes read-only pipeline descriptor inspection for deterministic contract tests
- focused test proves both cached 2D pipelines request SourceOver
- TASK-009B merge: `ba2bfcfc76c9e1fd0b6c7c5f3347a22882d41b54`

Still required before declaring Stage 4 complete:

- replace the old private S16 `GpuCtrl` FillRect/translation-only presentation route with the real neutral `VulkanGpuDevice` + `UiRenderer2D` frame path
- live control proof exercises fill, stroke, rounded rectangle, affine transform, clipping and translucent source-over rendering
- focused Windows Debug/Release acceptance for `RenderGpu2DTest` plus the real Vulkan/control path
- semantic/parity evidence covering the complete Stage-4 primitive/state surface

Regular non-swapchain sRGB texture-upload parity in `RenderNull` is deferred to Stage 5 image/texture work; it does not block Stage-4 solid primitive rendering.

Do not add text/vector work until this Stage-4 primitive renderer is accepted.

## Recovery Log

BASE: `ba2bfcfc76c9e1fd0b6c7c5f3347a22882d41b54` / `main`
TASK: `TASK-009C` real GpuCtrl neutral renderer migration while `TASK-008A1-S17C-W1` validates Stage 3 in parallel
TOUCHED: TASK-009B — `render/RenderRhi/RenderRhi.h`, `render/RenderVulkan/RenderVulkanRhi.cpp`, `render/RenderNull/RenderNull.h`, `render/RenderGpu2D/RenderGpu2D.cpp`, `tests/RenderGpu2DTest/main.cpp`; status — `docs/ACTIVE_WORK.md`
STATUS: PARTIAL — Stage-4 renderer core and source-over blending published; live GpuCtrl migration and Windows acceptance remain. Stage-3 implementation is complete but Windows acceptance is still pending.
PUBLISHED: S17C-B2-B1 `ced346bae602ed6b9b34c8a468c19cc26ffc5c08`; TASK-009A `ca972a087c63a8d54a7f2a9e1683c906b6c747a4`; TASK-009B `ba2bfcfc76c9e1fd0b6c7c5f3347a22882d41b54`
VALIDATION: TASK-009A/B source/static and aggregate PR reviews complete; Windows/runtime validation pending; S17C-W1 pending

## Next Action

Refresh current `main`, fetch the complete `GpuCtrl` package plus presentation/replay tests, and implement `TASK-009C`: session opens once, `VulkanGpuDevice` borrows it, neutral surface/swapchain IDs own the logical frame lifecycle, `UiRenderer2D` replays the control's immutable display list, and presentation returns through the session without duplicate Vulkan ownership. Publish and verify that checkpoint, then run one combined Stage-3/Stage-4 Windows acceptance. Do not declare either stage 100% until the relevant Windows evidence is clean.
