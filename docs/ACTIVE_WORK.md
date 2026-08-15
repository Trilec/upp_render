# Active Work Status

This file is the recovery checkpoint for active `upp_render` implementation work.
Update it whenever a coherent checkpoint is published so work can resume from repository state rather than chat history.

## Accepted Baseline

- Branch: `main`
- Accepted through: `TASK-008A1-S17B-R1`
- S17A neutral-contract SHA: `5d7e5e2537a7fd70bd9d344c9cb885a04014c041`
- S17B implementation SHA: `07e7870ae91316305f84a9fdc32b7488fd37eb3a`
- S17B-R1 correction SHA: `6d36c102dcc30b79bf61156a1aec2d77bd598ecc`
- Windows acceptance HEAD: `dbd76e60ef090ca440461b8d3e7a1ab0f2a96e39`
- Windows result: PASS — focused resource Debug 4/4, Release 2/2, all named regressions passed
- Validation: 0 Vulkan warnings, 0 Vulkan errors; final tracked Vulkan ownership counts zero

## Current Objective

1. Close Stage 3 - Vulkan Bootstrap against the neutral `GpuDevice` contract.
2. Move directly into Stage 4 - GPU 2D Renderer using larger coherent vertical slices.

Active implementation task: `TASK-008A1-S17C-B2` — converge neutral surface/swapchain/frame handles onto the accepted `VulkanSurfaceSession` ownership path and close Stage 3.

## Stage 3 Closure Gate

Accepted and retained:

- runtime / loader / validation, instance/debug messenger, device/queues, Win32 surface
- grouped/default Vulkan ownership and deterministic cleanup
- session-owned swapchain creation/recreation and frame acquire/present
- embedded `GpuCtrl` lifecycle and presentation path
- neutral buffer/texture upload contract plus `RenderNull` validation authority
- real Vulkan buffers and row-pitch-aware texture uploads
- S17B-R1 Windows acceptance with validation 0/0 and final Vulkan ownership zero

Published in S17C-A, platform validation pending with final S17C acceptance:

- neutral vertex/fragment shader lifecycle and SPIR-V payload descriptor
- `Position2Color4F` vertex layout
- render-pass clear colour
- pipeline shader/layout binding
- Null validation and `RenderRhiTest` coverage
- S17C-A implementation SHA: `28c303d4859b7b6fdce3380e23fcab68aa84c731`

Published in S17C-B1, platform validation pending with final S17C acceptance:

- real `VkShaderModule` creation/destruction with deterministic neutral validation
- real dynamic-rendering graphics pipelines for `Position2Color4F`
- transient command-list ownership and recording
- dynamic render passes against adapter-owned colour textures
- viewport/scissor setup, pipeline binding, vertex-buffer binding and real `vkCmdDraw`
- synchronous correctness-first queue submission; `Submit` consumes command-list ownership
- resource destruction is refused while command work is live; shader destruction is refused while a pipeline references it
- command-local texture layout/initialization tracking supports multiple render passes in one command list without stale bindings or image-view leaks
- adapter capabilities now advertise buffers, textures, render passes, pipelines and shaders
- focused `RenderVulkanGraphicsTest` with embedded deterministic SPIR-V; no shader compiler dependency
- B1 implementation SHA: `080ba9a8ee59b64ce686365c22c9039ca5ef7b48`
- B1-R1 correction SHA: `c0e739afde819d1c2957db91304b17f30f719003`
- B1 publication merge on `main`: `a1e0d84eed295fc4650710aa27d1f777bd149463`

Still required before declaring Stage 3 complete:

- neutral logical surface/swapchain/frame handles mapped onto the borrowed session without duplicate Vulkan ownership
- frame colour-target mapping to the acquired session swapchain image
- render submission transition back to `PRESENT_SRC_KHR` and session-authoritative presentation
- resize/teardown/error rollback for the neutral session-bound path
- focused Windows acceptance of S17C-A+B with zero validation warnings/errors and zero final ownership

## Stage 4 Acceleration Direction

Once Stage 3 is accepted, use larger coherent slices for:

- rectangle/stroke/rounded-rectangle primitive coverage
- general affine transform + clipping state
- opacity/blending
- batching and persistent pipeline/resource reuse
- software-vs-GPU output parity evidence

Do not add text/vector work until the Stage 4 primitive renderer is coherent.

## Recovery Log

BASE: `a1e0d84eed295fc4650710aa27d1f777bd149463` / `main`
TASK: `TASK-008A1-S17C-B1` real Vulkan graphics command engine + multi-pass correction; continuing with `TASK-008A1-S17C-B2`
TOUCHED: B1 — `render/RenderVulkan/RenderVulkanRhi.h`, `render/RenderVulkan/RenderVulkanRhi.cpp`, `tests/RenderVulkanGraphicsTest/RenderVulkanGraphicsTest.upp`, `tests/RenderVulkanGraphicsTest/main.cpp`; status — `docs/ACTIVE_WORK.md`
STATUS: S17C-B1 implementation/source review published on main; B2 session/frame convergence next
PUBLISHED: S17C-A `28c303d4859b7b6fdce3380e23fcab68aa84c731`; B1 `080ba9a8ee59b64ce686365c22c9039ca5ef7b48`; B1-R1 `c0e739afde819d1c2957db91304b17f30f719003`; main merge `a1e0d84eed295fc4650710aa27d1f777bd149463`
VALIDATION: B1 source review and whitespace checks complete; Windows/runtime/platform validation pending final S17C checkpoint

## Next Action

Refresh current `main` and implement S17C-B2. Preserve `VulkanSurfaceSession` as the sole Vulkan surface/swapchain/frame ownership authority. Add logical neutral surface/swapchain/frame IDs and borrowed acquired-image render targets to `VulkanGpuDevice`, make RHI submission transition the acquired image to present layout, hand presentation back to the session without a second Vulkan runtime/device/swapchain owner, cover resize/lifetime/error paths, publish and verify, then give Gary one complete Stage-3 Windows acceptance task. Do not declare Stage 3 complete until that validation passes.
