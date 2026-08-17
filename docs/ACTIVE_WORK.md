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
2. Close Stage 4 source semantics/parity, then run one combined Stage-3/Stage-4 Windows acceptance.

Active validation task: `TASK-008A1-S17C-W1` — accept the published neutral graphics + surface/swapchain/frame convergence on Windows.
Active implementation task: `TASK-009D` — close Stage-4 semantic parity using the same immutable display list through `SoftwareUiRenderer` and `UiRenderer2D`, then publish the final validation boundary.

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

Published in `TASK-009C`:

- the old S16 private `GpuCtrlFrameIntent` / FillRect-only / translation-only replay authority is removed from `GpuCtrl`
- live `GpuCtrl` now opens one `VulkanSurfaceSession`, constructs a borrowing `VulkanGpuDevice`, constructs a borrowing `UiRenderer2D`, and manages neutral logical surface/swapchain/frame handles through that adapter
- live painting now follows `BeginFrame -> UiRenderer2D::RenderFrame -> Submit -> Present`; presentation remains session-authoritative and no second Vulkan ownership tree is created
- resize goes through neutral `ResizeSwapchain`; out-of-date recovery is one bounded recreate/retry, not a render loop
- teardown reverses borrowing order: renderer -> logical swapchain/surface -> adapter -> session
- live immutable scene exercises opaque/translucent fills, translucent stroke, rounded rectangle, device-space clip, Save/Restore, and a genuine scale/shear/translation affine transform
- `GpuCtrlReplayTest` now validates that same live scene through production `UiRenderer2D` + `RenderNull` instead of asserting retired S16 limitations
- existing `GpuCtrlPresentationTest` remains the real two-control Vulkan lifecycle/resize/refresh/ownership regression and now naturally runs through the production renderer path
- TASK-009C merge: `b15c7579a0471290dc416131ebe9180a4c14be05`

Still required before declaring Stage 4 complete:

- one source-side semantic parity closure: the same representative Stage-4 immutable display list must replay successfully through both `SoftwareUiRenderer` and `UiRenderer2D`; exact GPU pixel readback remains Stage-8 hardening, not a Stage-4 API expansion
- focused Windows Debug/Release acceptance for `RenderGpu2DTest` and `GpuCtrlReplayTest`
- real Vulkan `GpuCtrlPresentationTest` remains clean through refresh/resize/hide-show with zero validation warnings/errors and zero final ownership

Regular non-swapchain sRGB texture-upload parity in `RenderNull` remains deferred to Stage 5 image/texture work; it does not block Stage-4 solid primitive rendering.

Do not add text/vector work until this Stage-4 primitive renderer is accepted.

## Recovery Log

BASE: `b15c7579a0471290dc416131ebe9180a4c14be05` / `main`
TASK: `TASK-009D` Stage-4 semantic parity closure while `TASK-008A1-S17C-W1` validates Stage 3 in parallel
TOUCHED: TASK-009C — `render/GpuCtrl/GpuCtrl.cpp`, `render/GpuCtrl/GpuCtrl.upp`, `render/GpuCtrl/GpuCtrlTestHooks.h`, `tests/GpuCtrlReplayTest/GpuCtrlReplayTest.upp`, `tests/GpuCtrlReplayTest/main.cpp`; status — `docs/ACTIVE_WORK.md`
STATUS: PARTIAL — Stage-4 renderer core, source-over blending and live GpuCtrl integration are published; semantic parity closure and Windows acceptance remain. Stage-3 implementation is complete but Windows acceptance remains.
PUBLISHED: S17C-B2-B1 `ced346bae602ed6b9b34c8a468c19cc26ffc5c08`; TASK-009A `ca972a087c63a8d54a7f2a9e1683c906b6c747a4`; TASK-009B `ba2bfcfc76c9e1fd0b6c7c5f3347a22882d41b54`; TASK-009C `b15c7579a0471290dc416131ebe9180a4c14be05`
VALIDATION: TASK-009A/B/C source/static and aggregate PR reviews complete; Windows/runtime validation pending; S17C-W1 pending

## Next Action

Implement and publish `TASK-009D` by extending `RenderGpu2DTest` so one representative Stage-4 immutable display list replays through the software reference (`SoftwareUiRenderer` + `ImagePainter`) and through the production GPU renderer contract (`UiRenderer2D` + `RenderNull`). Keep exact GPU pixel readback deferred to Stage 8. Then update this recovery record and give Gary one combined final Windows acceptance: Stage-3 focused Vulkan tests first, followed by Stage-4 renderer/control tests. Do not declare either stage 100% until that acceptance is clean.
