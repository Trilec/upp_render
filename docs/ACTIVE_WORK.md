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

1. Complete one focused Windows/runtime acceptance of the published S17C Stage-3 convergence checkpoint.
2. If that acceptance is clean, declare Stage 3 complete and move into Stage 4 GPU 2D rendering in larger coherent vertical slices.

Active validation task: `TASK-008A1-S17C-W1` — accept the published neutral graphics + surface/swapchain/frame convergence on Windows.

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
- `GpuFormat` now has explicit `RGBA8Srgb` / `BGRA8Srgb` identities so negotiated Vulkan sRGB swapchain images are never represented as UNORM
- `GpuSwapchainDesc` is documented as requested presentation properties; `GpuFrameInfo` reports actual negotiated size and colour format
- neutral logical `GpuSurfaceId`, `GpuSwapchainId`, `GpuFrameId` lifecycle maps onto the existing borrowed `VulkanSurfaceSession` ownership without creating a second Vulkan surface, swapchain, acquire or present authority
- acquired swapchain images are temporary borrowed `GpuTextureId` render targets and cannot be uploaded to or destroyed through the adapter
- neutral content validity is tracked separately per swapchain image from session layout initialization; `StoreOp::DontCare` does not make a later `Load` valid
- frame-owned command submission transitions the acquired image back to `PRESENT_SRC_KHR`, completes synchronously, then hands presentation back to the session
- resize recreates the session-owned swapchain and resets per-image neutral content validity
- failed backbuffer render-pass setup does not falsely bind unrelated command work to the active frame
- focused graphics coverage includes no-render present, rendered present, exact negotiated format/extent, lifetime guards, resize, content-validity reset and final logical ownership cleanup
- B2A private session handoff merge: `5357c7938dc4b0e067470fc1c188c2b8941a6d53`
- B2-B0 explicit sRGB format merge: `869ab63649ccd6bf4ee4cca2365d58e5a4c3ce86`
- B2-B1 recovery source head: `ca157a86163581709f2fba4f857a5f95bace4621`
- B2-B1 publication merge on `main`: `ced346bae602ed6b9b34c8a468c19cc26ffc5c08`

Still required before declaring Stage 3 complete:

- focused Windows Debug/Release build and runtime acceptance of the combined S17C checkpoint
- `RenderVulkanGraphicsTest` must pass repeatedly with validation 0 warnings / 0 errors
- resource/frame/clear/GpuCtrl regressions must remain clean
- final Vulkan runtime/instance/debug/surface/device/swapchain ownership diagnostics must be zero
- neutral public headers must remain free of Vulkan types

Non-blocking follow-up after the Stage-3 gate: decide whether regular non-swapchain sRGB texture uploads should be advertised consistently by `RenderNull`; current S17C uses the new sRGB identities to represent negotiated swapchain attachment formats, and swapchain backbuffer uploads remain invalid on both neutral and Vulkan paths.

## Stage 4 Acceleration Direction

After Stage 3 receives clean Windows acceptance, use larger coherent slices for:

- rectangle/stroke/rounded-rectangle primitive coverage
- general affine transform + clipping state
- opacity/blending
- batching and persistent pipeline/resource reuse
- software-vs-GPU output parity evidence

Do not add text/vector work until the Stage 4 primitive renderer is coherent.

## Recovery Log

BASE: `ced346bae602ed6b9b34c8a468c19cc26ffc5c08` / `main`
TASK: `TASK-008A1-S17C-W1` Windows acceptance of published Stage-3 convergence
TOUCHED: S17C-B2-B1 — `render/RenderRhi/RenderRhi.h`, `render/RenderVulkan/RenderVulkanRhi.h`, `render/RenderVulkan/RenderVulkanRhi.cpp`, `tests/RenderVulkanGraphicsTest/main.cpp`; status — `docs/ACTIVE_WORK.md`
STATUS: IMPLEMENTATION COMPLETE — PLATFORM VALIDATION PENDING
PUBLISHED: S17C-A `28c303d4859b7b6fdce3380e23fcab68aa84c731`; B1 merge `a1e0d84eed295fc4650710aa27d1f777bd149463`; B2A merge `5357c7938dc4b0e067470fc1c188c2b8941a6d53`; sRGB merge `869ab63649ccd6bf4ee4cca2365d58e5a4c3ce86`; B1-R2 merge `a2310a8b25ef0cfc58c2f7585f7d43359d9c3e82`; B2-B1 merge `ced346bae602ed6b9b34c8a468c19cc26ffc5c08`
VALIDATION: source/static review complete; recovery blobs and aggregate PR diff reviewed; Windows/runtime/Vulkan validation pending

## Next Action

Gary performs one focused Windows acceptance of current `main`, confirming `ced346bae602ed6b9b34c8a468c19cc26ffc5c08` is an ancestor of current HEAD. Start with `RenderVulkanGraphicsTest` Debug, then Release, then the named neutral/resource/frame/clear/GpuCtrl regressions. Stop on the first compile/runtime/validation/ownership blocker and report exact evidence. Do not edit, commit or push. Do not begin Stage 4 or declare Stage 3 complete until this acceptance passes.
