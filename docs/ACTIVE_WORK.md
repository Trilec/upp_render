# Active Work Status

This file is the recovery checkpoint for active `upp_render` implementation work.
Update it whenever a coherent checkpoint is published so work can resume from repository state rather than chat history.

## Accepted Baseline

- Branch: `main`
- Accepted through: `TASK-008A1-S17B-R1`
- S17A neutral-contract SHA: `5d7e5e2537a7fd70bd9d344c9cb885a04014c041`
- S17B implementation SHA: `07e7870ae91316305f84a9fdc32b7488fd37eb3a`
- S17B-R1 test-only correction SHA: `6d36c102dcc30b79bf61156a1aec2d77bd598ecc`
- Windows acceptance HEAD: `dbd76e60ef090ca440461b8d3e7a1ab0f2a96e39`
- Windows result: PASS — focused resource Debug 4/4, Release 2/2, all named regressions passed
- Validation: 0 Vulkan warnings, 0 Vulkan errors; final tracked Vulkan ownership counts zero

## Current Objective

Accelerate completion of:

1. Stage 3 - Vulkan Bootstrap to 100% against the published `GpuRhi` contract.
2. Stage 4 - GPU 2D Renderer through larger coherent vertical slices instead of one display operation per milestone.

Active implementation task: `TASK-008A1-S17C-B` — implement the real Vulkan side of the completed neutral graphics contract and close Stage 3.

## Stage 3 Closure Gate

Accepted:

- runtime / loader / validation
- instance and debug messenger
- physical-device and queue selection
- logical device ownership
- Win32 surface integration
- shared-instance groups and RAII leases
- private swapchain ownership and resize recreation
- frame acquisition / synchronization / presentation
- visible clear and ordered rectangle presentation
- embedded `GpuCtrl` lifecycle, multi-control isolation and clean shutdown
- neutral `GpuDevice::WriteBuffer` / `WriteTexture` upload contract and Null validation authority
- production `VulkanGpuDevice` adapter borrowing the accepted live `VulkanSurfaceSession` device/queue ownership
- real Vulkan buffer creation, host-visible/coherent-preferred allocation, writes and destruction
- real optimal-image texture allocation with device-local-preferred memory
- row-pitch-aware texture upload through tight staging buffers and one-time graphics submissions
- tracked resource ownership with explicit and destructor cleanup
- S17B-R1 Windows acceptance: resource Debug 4/4, Release 2/2, `RenderRhiTest` Debug/Release, `RenderVulkanFrameTest`, `RenderVulkanClearFrameTest`, and `GpuCtrlPresentationTest` all pass; validation 0/0 and final ownership zero

Published in S17C-A, platform validation pending:

- explicit neutral vertex/fragment shader lifecycle with capability-gated optional `GpuDevice` methods
- neutral SPIR-V shader payload descriptor and shader-stage identity
- minimal `Position2Color4F` vertex-layout contract for the first real graphics pipeline
- explicit render-pass clear colour
- pipeline descriptors now bind vertex/fragment shaders and vertex layout
- `RenderNull` is the validation authority for shader handles/stages, pipeline layout, render-pass load/store/clear state, vertex-buffer usage and draw preconditions
- `RenderRhiTest` covers the new neutral contract and deterministic logging
- S17C-A implementation SHA: `28c303d4859b7b6fdce3380e23fcab68aa84c731`
- Vulkan capability flags remain buffers/textures only until S17C-B implements the new operations for real

Still required before declaring Stage 3 complete:

- real Vulkan shader-module creation/destruction
- real Vulkan graphics-pipeline creation/destruction for the S17C-A neutral contract
- command-list lifecycle and dynamic-rendering command recording through `VulkanGpuDevice`
- vertex-buffer binding and real draw submission
- reconcile neutral surface / swapchain / frame methods with the accepted per-session `VulkanSurfaceSession` ownership model without duplicating Vulkan ownership
- explicit submitted-resource lifetime rule and cleanup/error rollback
- focused Windows acceptance of the complete Vulkan `GpuDevice` contract with zero final ownership

## Stage 4 Acceleration Direction

Use larger coherent slices once Stage 3 is closed:

- rectangle/stroke/rounded-rectangle primitive coverage
- general affine transform + clipping state
- opacity/blending
- batching and persistent pipeline/resource reuse
- software-vs-GPU output parity evidence

Do not add text/vector work until the Stage 4 primitive renderer is coherent.

## Recovery Log

BASE: `3eb5fbdca12b06bd04258f116ede95b1bafbaffd`
TASK: `TASK-008A1-S17C-A` neutral graphics contract completion; continuing with `TASK-008A1-S17C-B`
TOUCHED: `render/RenderRhi/RenderRhi.h`, `render/RenderRhi/RenderRhi.cpp`, `render/RenderNull/RenderNull.h`, `render/RenderNull/RenderNull.cpp`, `tests/RenderRhiTest/main.cpp`, then status-only `docs/ACTIVE_WORK.md`
STATUS: S17C-A published and source-reviewed; S17C-B Vulkan implementation active
PUBLISHED: S17C-A `28c303d4859b7b6fdce3380e23fcab68aa84c731`
VALIDATION: source review complete; Windows/platform validation pending and will be included in the meaningful S17C acceptance checkpoint

## Next Action

Implement S17C-B against current `main`: preserve `VulkanSurfaceSession` as the Vulkan surface/swapchain/frame ownership authority, add real shader/pipeline/command/draw support to `VulkanGpuDevice`, converge neutral surface/swapchain/frame handles onto the borrowed session rather than creating duplicate Vulkan state, use a correctness-first submitted-resource lifetime rule, add focused full-contract Vulkan coverage, then hand the complete S17C checkpoint to Gary for Windows acceptance. Do not declare Stage 3 complete until that validation passes.
