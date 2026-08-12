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

BASE: `2218e19299987149a47aa7a688cc36ef5e989037`
TASK: `TASK-008A1-S17A` neutral resource-upload contract
TOUCHED: `render/RenderRhi/RenderRhi.h`, `render/RenderNull/RenderNull.h`, `render/RenderNull/RenderNull.cpp`, `tests/RenderRhiTest/main.cpp`, `docs/PROJECT_PLAN.md`, `docs/ACTIVE_WORK.md`
STATUS: buffer/texture upload contract and Null validation authority implemented and published
PUBLISHED: `5d7e5e2537a7fd70bd9d344c9cb885a04014c041`
VALIDATION: guarded source review passed; Windows RenderRhiTest validation pending

## Next Action

Gary validates S17A from published main. On a clean PASS, implement S17B as the first production Vulkan resource slice: Vulkan GpuDevice ownership plus real buffer allocation/write/destruction and texture allocation/write/destruction, reusing accepted instance/device/session ownership rather than duplicating it. Publish that coherent slice before the command/pipeline/draw integration slice.
