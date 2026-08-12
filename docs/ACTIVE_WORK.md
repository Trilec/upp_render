# Active Work Status

This file is the recovery checkpoint for active `upp_render` implementation work.
Update it whenever a coherent checkpoint is published so work can resume from repository state rather than chat history.

## Accepted Baseline

- Branch: `main`
- Accepted through: `TASK-008A1-S16G`
- Accepted SHA before this status-file commit: `eab3e59ac40be25d6c974224492c7d27c6850cb8`
- Windows result: clean PASS
- Validation: 0 warnings / 0 errors
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

Still to reconcile against the published Stage 3 / `GpuRhi` promises before declaring Stage 3 complete:

- production Vulkan `GpuDevice` coverage for the neutral RHI contract
- buffer/resource allocation and upload path
- texture allocation/upload path needed by later 2D/image work
- command-list/render-pass/pipeline/draw/submit path through the RHI contract
- destruction/lifetime handling appropriate for submitted GPU resources
- focused Windows acceptance proving those paths and zero final ownership

## Stage 4 Acceleration Direction

Use larger coherent slices once the Vulkan RHI path is in place:

- rectangle/stroke/rounded-rectangle primitive coverage
- general affine transform + clipping state
- opacity/blending
- batching and persistent pipeline/resource reuse
- software-vs-GPU output parity evidence

Do not add text/vector work until the Stage 4 primitive renderer is coherent.

## Recovery Log

BASE: `eab3e59ac40be25d6c974224492c7d27c6850cb8`
TASK: Stage 3 closure analysis / accelerated Vulkan RHI implementation
TOUCHED: `docs/ACTIVE_WORK.md` only at this checkpoint
STATUS: S16G accepted; Stage 3 closure slice being defined
PUBLISHED: this status checkpoint
VALIDATION: implementation validation pending for next code slice

## Next Action

Inspect the existing RenderNull implementation and current RenderVulkan ownership/session code against every `GpuDevice` method in `RenderRhi.h`. Define the smallest set of larger vertical implementation slices that can close Stage 3 without duplicating the accepted surface/session path, then implement and publish the first slice before Windows validation.
