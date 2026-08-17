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

1. Complete one focused Windows/runtime acceptance of the published S17C Stage-3 convergence checkpoint and then mark Vulkan bootstrap complete if clean.
2. Complete Stage 4 GPU 2D rendering in large recoverable slices while that platform validation runs in parallel.

Active validation task: `TASK-008A1-S17C-W1` — accept the published neutral graphics + surface/swapchain/frame convergence on Windows.
Active implementation task: `TASK-009` — complete the production `UiRenderer2D`, alpha blending, real Vulkan/GpuCtrl integration and Stage-4 acceptance.

## Stage 3 Closure Gate

Accepted and retained from the platform-validated baseline:

- runtime / loader / validation, instance/debug messenger, device/queues, Win32 surface
- grouped/default Vulkan ownership and deterministic cleanup
- session-owned swapchain creation/recreation and frame acquire/present
- embedded `GpuCtrl` lifecycle and presentation path
- neutral buffer/texture upload contract plus `RenderNull` validation authority
- real Vulkan buffers and row-pitch-aware texture uploads
- S17B-R1 Windows acceptance with validation 0/0 and final Vulkan ownership zero

Published in S17C-A, platform validation pending final S17C acceptance:

- neutral vertex/fragment shader lifecycle and SPIR-V payload descriptor
- `Position2Color4F` vertex layout
- render-pass clear colour
- pipeline shader/layout binding
- Null validation and `RenderRhiTest` coverage
- S17C-A implementation SHA: `28c303d4859b7b6fdce3380e23fcab68aa84c731`

Published in S17C-B1, platform validation pending final S17C acceptance:

- real `VkShaderModule` creation/destruction with deterministic neutral validation
- real dynamic-rendering graphics pipelines for `Position2Color4F`
- transient command-list ownership and recording
- dynamic render passes against adapter-owned colour textures
- viewport/scissor setup, pipeline binding, vertex-buffer binding and real `vkCmdDraw`
- synchronous correctness-first queue submission; `Submit` consumes command-list ownership
- resource destruction refused while command work is live; shader destruction refused while a pipeline references it
- command-local texture layout/initialization tracking supports multiple render passes in one command list without stale bindings or image-view leaks
- focused `RenderVulkanGraphicsTest` with embedded deterministic SPIR-V; no shader compiler dependency
- B1 implementation SHA: `080ba9a8ee59b64ce686365c22c9039ca5ef7b48`
- B1-R1 correction SHA: `c0e739afde819d1c2957db91304b17f30f719003`
- B1 publication merge on `main`: `a1e0d84eed295fc4650710aa27d1f777bd149463`
- B1-R2 focused resource-test correction merge on `main`: `a2310a8b25ef0cfc58c2f7585f7d43359d9c3e82`

Published in S17C-B2, platform validation pending final S17C acceptance:

- private session handoff exposes the currently acquired swapchain image only to the Vulkan adapter and allows session-authoritative presentation after externally completed synchronous rendering
- `GpuFormat` has explicit `RGBA8Srgb` / `BGRA8Srgb` identities so negotiated Vulkan sRGB swapchain images are never represented as UNORM
- `GpuSwapchainDesc` is requested presentation properties; `GpuFrameInfo` reports actual negotiated size and colour format
- neutral logical surface/swapchain/frame lifecycle maps onto the existing borrowed `VulkanSurfaceSession` ownership without creating a second Vulkan surface, swapchain, acquire or present authority
- acquired swapchain images are temporary borrowed `GpuTextureId` render targets and cannot be uploaded to or destroyed through the adapter
- neutral content validity is tracked separately per swapchain image from session layout initialization; `StoreOp::DontCare` does not make a later `Load` valid
- frame-owned command submission transitions the acquired image back to `PRESENT_SRC_KHR`, completes synchronously, then hands presentation back to the session
- resize recreates the session-owned swapchain and resets per-image neutral content validity
- failed backbuffer render-pass setup does not falsely bind unrelated command work to the active frame
- focused graphics coverage includes no-render present, rendered present, exact negotiated format/extent, lifetime guards, resize, content-validity reset and final logical ownership cleanup
- B2A private session handoff merge: `5357c7938dc4b0e067470fc1c188c2b8941a6d53`
- B2-B0 explicit sRGB format merge: `869ab63649ccd6bf4ee4cca2365d58e5a4c3ce86`
- B2-B1 publication merge on `main`: `ced346bae602ed6b9b34c8a468c19cc26ffc5c08`

Still required before declaring Stage 3 complete:

- focused Windows Debug/Release build and runtime acceptance of the combined S17C checkpoint
- `RenderVulkanGraphicsTest` repeated passes with validation 0 warnings / 0 errors
- resource/frame/clear/GpuCtrl regressions clean
- final Vulkan runtime/instance/debug/surface/device/swapchain ownership diagnostics zero
- neutral public headers remain free of Vulkan types

## Stage 4 GPU 2D Renderer

Published in `TASK-009A`:

- new backend-neutral `render/RenderGpu2D` package with `UiRenderer2D`
- direct replay of existing immutable `UiDisplayList` operations rather than another private drawing authority
- filled rectangles, rectangle strokes and uniform rounded rectangles
- full affine `ConcatTransform` geometry, nested `Save`/`Restore` and cumulative device-space clipping
- target-bound polygon clipping and deterministic convex tessellation
- one `Position2Color4F` vertex stream and one draw/batch for compatible solid primitives
- persistent SPIR-V shader pair, pipeline cache by render-target format and grow/reuse vertex buffer
- exact Rgba alpha retained in the vertex stream
- clear-only frames issue no geometry draw
- focused `RenderGpu2DTest` through `RenderNull` covers geometry accounting, clipping, full affine transforms, stroke/rounded paths, one-draw batching, persistent resource reuse and cleanup
- TASK-009A publication merge on `main`: `ca972a087c63a8d54a7f2a9e1683c906b6c747a4`

Still required before declaring Stage 4 complete:

- explicit neutral source-over alpha blend state and real Vulkan mapping; current S17C pipeline has blending disabled
- close regular non-swapchain sRGB texture-upload parity in `RenderNull`
- route the live `GpuCtrl` Vulkan presentation proof through `VulkanGpuDevice` + `UiRenderer2D` instead of the old private S16 FillRect/translation-only path
- broaden the live control proof to exercise stroke, rounded rectangle, affine transform, clipping and translucent rendering
- focused Windows Debug/Release acceptance for `RenderGpu2DTest` and the real Vulkan/control path
- software-vs-GPU semantic/parity evidence for the complete Stage-4 primitive/state surface

Do not add text/vector work until this Stage-4 primitive renderer is accepted.

## Recovery Log

BASE: `ca972a087c63a8d54a7f2a9e1683c906b6c747a4` / `main`
TASK: `TASK-009` complete Stage-4 GPU 2D renderer while `TASK-008A1-S17C-W1` validates Stage 3 in parallel
TOUCHED: TASK-009A — `render/RenderGpu2D/RenderGpu2D.h`, `render/RenderGpu2D/RenderGpu2D.cpp`, `render/RenderGpu2D/RenderGpu2D.upp`, `tests/RenderGpu2DTest/main.cpp`, `tests/RenderGpu2DTest/RenderGpu2DTest.upp`; status — `docs/ACTIVE_WORK.md`
STATUS: PARTIAL — Stage-4 renderer core published; alpha blending, real GpuCtrl integration and Windows acceptance remain. Stage-3 implementation is complete but Windows acceptance is still pending.
PUBLISHED: S17C-B2-B1 `ced346bae602ed6b9b34c8a468c19cc26ffc5c08`; TASK-009A `ca972a087c63a8d54a7f2a9e1683c906b6c747a4`
VALIDATION: TASK-009A source/static review and aggregate PR review complete; `RenderGpu2DTest` Windows/runtime validation pending; S17C-W1 pending

## Next Action

Implement and publish the Stage-4 alpha-blend/format-parity slice: add explicit neutral source-over blending to `GpuPipelineDesc`, map it to Vulkan pipeline blend state, align `RenderNull` sRGB upload validation/logging, make `UiRenderer2D` request blending and extend focused tests. Then replace the old private `GpuCtrl` rectangle presentation route with the real neutral `VulkanGpuDevice` + `UiRenderer2D` frame path, publish and run one combined Stage-3/Stage-4 Windows acceptance. Do not declare either stage 100% until the relevant Windows evidence is clean.
