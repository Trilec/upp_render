# Active Work Status

This file is the recovery checkpoint for active `upp_render` implementation work.
Update it whenever a coherent checkpoint is published so work can resume from repository state rather than chat history.

## Accepted Baseline

- Branch: `main`
- Accepted through: `TASK-008A1-S17A`
- Accepted implementation SHA: `5d7e5e2537a7fd70bd9d344c9cb885a04014c041`
- S17B implementation SHA: `07e7870ae91316305f84a9fdc32b7488fd37eb3a`
- S17B-R1 test-only correction SHA: `6d36c102dcc30b79bf61156a1aec2d77bd598ecc`
- Windows result: S17A clean PASS; original S17B-W1 stopped at focused-test compile blocker; S17B-R1 re-acceptance pending
- Validation: S17A neutral contract/regressions pass; S17B production source guard passes; original S17B-W1 did not reach runtime validation
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

S17B-R1 correction:

- the original focused Windows test incorrectly defined `WIN32_LEAN_AND_MEAN` before the RenderVulkan/U++ include chain, suppressing the declaration needed by `Core/Profile.h::timeGetTime`
- R1 changes only `tests/RenderVulkanResourceTest/main.cpp`
- RenderVulkan/U++ headers are now included first, matching the accepted Vulkan test pattern; the direct Win32 include remains after `WIN32_LEAN_AND_MEAN`
- no production source, package, neutral API, resource logic, Vulkan lifetime logic or acceptance semantics changed in R1

Still required before declaring Stage 3 complete:

- Windows acceptance of S17B-R1 real Vulkan resource allocation/upload/destruction
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

BASE: `fffc67137459bd8c8ba9352f64dfea72aa6f476b`
TASK: `TASK-008A1-S17B-R1` focused Windows test include-order correction
TOUCHED: `tests/RenderVulkanResourceTest/main.cpp`, then status-only `docs/ACTIVE_WORK.md`
STATUS: original S17B production implementation unchanged; focused test include ordering corrected to match accepted Vulkan tests
PUBLISHED: S17B implementation `07e7870ae91316305f84a9fdc32b7488fd37eb3a`; S17B-R1 `6d36c102dcc30b79bf61156a1aec2d77bd598ecc`
VALIDATION: S17B-R1 Windows re-acceptance pending

## Next Action

Gary pulls current `main` and runs the reduced S17B-R1 re-acceptance beginning with `RenderVulkanResourceTest` Debug. If the focused test compiles and passes, continue the remaining original S17B runtime/regression matrix. On a clean PASS, accept S17B and implement S17C as the remaining Vulkan `GpuDevice` convergence slice: command lists, render pass, pipeline/draw/submit plus resolution of the neutral surface/swapchain/frame ownership model, with submitted-resource lifetime rules. Keep S17C as a coherent published checkpoint and do not declare Stage 3 complete until that contract is genuinely satisfied and Windows-validated.
