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

Active implementation task: `TASK-008A1-S17C` — complete the remaining Vulkan `GpuDevice` convergence as one coherent Stage-3 closure slice.

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

Still required before declaring Stage 3 complete:

- command-list lifecycle through `VulkanGpuDevice`
- render-pass state and real Vulkan recording
- pipeline creation/destruction and draw submission
- vertex-buffer binding and draw validation
- reconcile neutral surface / swapchain / frame methods with the accepted per-session `VulkanSurfaceSession` ownership model without duplicating Vulkan ownership
- destruction/lifetime handling for resources referenced by submitted command work
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

BASE: `dbd76e60ef090ca440461b8d3e7a1ab0f2a96e39`
TASK: `TASK-008A1-S17C` Vulkan `GpuDevice` Stage-3 convergence/closure
TOUCHED: status checkpoint only: `docs/ACTIVE_WORK.md`; S17C production files not yet changed
STATUS: S17B-R1 accepted on Windows; S17C inspection/implementation is next
PUBLISHED: S17B `07e7870ae91316305f84a9fdc32b7488fd37eb3a`; R1 `6d36c102dcc30b79bf61156a1aec2d77bd598ecc`
VALIDATION: S17B-R1 PASS — Debug 4/4, Release 2/2, named regressions pass, Vulkan validation 0/0, final ownership zero

## Next Action

Refresh current `main`, inspect the complete neutral `GpuDevice`/`RenderNull` contract plus accepted Vulkan surface/swapchain/frame/session implementation, then implement S17C in larger coherent publishable checkpoints. Preserve `VulkanSurfaceSession` as the Vulkan ownership authority; `VulkanGpuDevice` must borrow and converge with it rather than create duplicate runtime/device/swapchain state. Do not declare Stage 3 complete until the full neutral contract is implemented coherently and Windows-validated.
