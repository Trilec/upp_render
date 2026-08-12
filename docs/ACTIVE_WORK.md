# Active Work Status

This file is the recovery checkpoint for active `upp_render` implementation work.
Update it whenever a coherent checkpoint is published so work can resume from repository state rather than chat history.

## Accepted Baseline

- Branch: `main`
- Accepted through: `TASK-008A1-S17A`
- Accepted implementation SHA: `5d7e5e2537a7fd70bd9d344c9cb885a04014c041`
- S17B implementation SHA now published: `07e7870ae91316305f84a9fdc32b7488fd37eb3a`
- Windows result: S17A clean PASS; S17B platform validation pending
- Validation: S17A neutral contract/regressions pass; S17B guarded source review passes
- Final accepted Vulkan ownership: all tracked ownership counts zero

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

Published in S17B, Windows acceptance pending:

- production `VulkanGpuDevice` adapter borrowing the accepted live `VulkanSurfaceSession` device/queue ownership
- real Vulkan buffer creation, host-visible/coherent-preferred allocation, writes and destruction
- real optimal-image texture allocation with device-local-preferred memory
- row-pitch-aware texture upload through tight staging buffers and one-time graphics submissions
- tracked resource ownership with explicit and destructor cleanup
- focused validation-enabled Vulkan resource test

Still required before declaring Stage 3 complete:

- command-list / render-pass / pipeline / draw / submit through `VulkanGpuDevice`
- reconcile neutral surface / swapchain / frame methods with the accepted per-session Vulkan device ownership model
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
STATUS: real Vulkan buffer/texture resource path implemented, guarded, squashed and published
PUBLISHED: `07e7870ae91316305f84a9fdc32b7488fd37eb3a`
VALIDATION: final source guard passed; Windows S17B resource validation pending

## Next Action

Gary validates S17B from current published `main`. On a clean PASS, implement S17C as the remaining Vulkan `GpuDevice` convergence slice: command lists, render pass, pipeline/draw/submit plus resolution of the neutral surface/swapchain/frame ownership model, with submitted-resource lifetime rules. Keep S17C as a coherent published checkpoint and do not declare Stage 3 complete until that contract is genuinely satisfied and Windows-validated.
