# Active Work Status

This file is the recovery checkpoint for active `upp_render` implementation work.
Update it whenever a coherent checkpoint is published so work can resume from repository state rather than chat history.

## Accepted Baseline

- Branch: `main`
- Accepted through: `TASK-008A1-S17A`
- Accepted implementation SHA: `5d7e5e2537a7fd70bd9d344c9cb885a04014c041`
- Current published recovery SHA before S17B: `597587d956b708a551d6672cf0ba506b21646213`
- Windows result: clean PASS
- Validation: S17A neutral contract/regressions pass; accepted Vulkan regressions remain clean
- Final Vulkan ownership: all tracked ownership counts zero

## Current Objective

Accelerate completion of:

1. Stage 3 - Vulkan Bootstrap to 100% against the published `GpuRhi` contract.
2. Stage 4 - GPU 2D Renderer through larger coherent vertical slices instead of one display operation per milestone.

## Stage 3 Closure Gate

Already accepted:

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

Current S17B slice:

- production `VulkanGpuDevice` adapter borrowing the accepted live `VulkanSurfaceSession` device/queue ownership
- real Vulkan buffer creation, host-visible/coherent-preferred allocation, writes and destruction
- real optimal-image texture allocation with device-local-preferred memory
- row-pitch-aware texture upload through tight staging buffers and one-time graphics submissions
- tracked resource ownership with explicit and destructor cleanup
- focused validation-enabled Windows resource acceptance

Still required before declaring Stage 3 complete:

- command-list / render-pass / pipeline / draw / submit through `VulkanGpuDevice`
- neutral surface / swapchain / frame methods bridged to the accepted session implementation
- destruction/lifetime handling for resources referenced by submitted command work
- focused Windows acceptance of the complete Vulkan `GpuDevice` contract with zero final ownership

## Stage 4 Acceleration Direction

Use larger coherent slices once the Vulkan RHI path is in place:

- rectangle/stroke/rounded-rectangle primitive coverage
- general affine transform + clipping state
- opacity/blending
- batching and persistent pipeline/resource reuse
- software-vs-GPU output parity evidence

Do not add text/vector work until the Stage 4 primitive renderer is coherent.

## Recovery Log

BASE: `597587d956b708a551d6672cf0ba506b21646213`
TASK: `TASK-008A1-S17B` production Vulkan resource allocation/upload/destruction
TOUCHED: `render/RenderVulkan/RenderVulkan.cpp`, `render/RenderVulkan/RenderVulkanSurfaceSession.h`, `render/RenderVulkan/RenderVulkanRhi.h`, `render/RenderVulkan/RenderVulkanRhi.cpp`, `render/RenderVulkan/RenderVulkan.upp`, `tests/RenderVulkanResourceTest/RenderVulkanResourceTest.upp`, `tests/RenderVulkanResourceTest/main.cpp`, `docs/PROJECT_PLAN.md`, `docs/ACTIVE_WORK.md`
STATUS: real Vulkan buffer/texture resource path implemented on isolated candidate; validation-sensitive memory selection corrected; final source/diff review in progress
PUBLISHED: S17B candidate not yet published to `main`
VALIDATION: Windows S17B resource validation pending

## Next Action

Finish final nine-file diff review, squash S17B into one coherent commit, publish by fast-forwarding current `main`, and update this checkpoint with the exact implementation SHA. Gary then validates the focused real Vulkan resource path. On a clean PASS, implement S17C as the remaining Vulkan `GpuDevice` convergence slice: command lists, render pass, pipeline/draw/submit plus neutral surface/swapchain/frame bridging, with submitted-resource lifetime rules.
