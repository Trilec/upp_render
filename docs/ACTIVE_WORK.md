# Active Work Status

This file is the recovery checkpoint for active `upp_render` implementation work.
Update it whenever a coherent checkpoint is published so work can resume from repository state rather than chat history.

## Accepted Baseline

- Branch: `main`
- Stage 3 Vulkan backend/bootstrap: **PASS / 100% accepted**
- Stage-3 Windows acceptance HEAD: `6ab33a42a3421643359cabfdae7afed7628ad349`
- Stage 4 GPU 2D renderer: **PASS / 100% accepted**
- Stage-4 Windows acceptance HEAD: `f8e7b24d510b4b5889370823dc1c0a5ef43a7f54`
- Stage-5 image foundation: **PASS / accepted**
- Stage-5 image implementation: `a11862d138e6b2f06d92067b4b804d8418b69d32`
- Stage-5 image Windows acceptance HEAD: `f2cd2bdf2ff7c05f7b883ef32405653ab198a98b`
- Stage-5 text/glyph-atlas implementation: `f98cce413b1992cfaef55669d4672824fe703b5f`
- Stage-5 text Windows acceptance HEAD: `91f1fe3cad91b5afe00de4afd6398b773e8f4715`
- TASK-010B-W1: **PASS** — text/software/Null Debug+Release PASS; Vulkan text Debug 4/4 + Release 2/2; GPU2D/image/GpuCtrl regressions PASS; validation 0/0; adapter resources 0; Vulkan ownership `0/0/0/0/0/0`
- TASK-010B-W1 mechanical source corrections: `6f322ad4ecc4b2364d020a00bb3676695cbc9cab`

## Current Objective

Stage 5 is **IMPLEMENTATION COMPLETE — FINAL WINDOWS/VULKAN ACCEPTANCE PENDING**.
Proceed in parallel with Stage 6 U++ integration so validation latency does not stall the project.

Active validation task to issue: `TASK-010-W1` — final Stage-5 vector/gradient/AA/SVG plus image/text regression acceptance.
Active implementation task: `TASK-011A` — first production-quality root U++/GPU composition boundary, preserving one root GPU surface rather than one native child per control.

## Stage 5 - Text, Images and Vector Rendering

### Images — accepted

Production scope includes immutable `DrawImage`, deterministic software replay, sampled-texture RHI/Null authority, sRGB uploads, affine UV clipping, ordered image/solid batching, renderer image caching and real Vulkan offscreen/swapchain presentation.

### Text — accepted

Production scope includes immutable `DrawText`, fractional U++ software replay, U++-authoritative glyph metrics/fallback/composition, persistent sampled glyph atlas pages, padded partial uploads, affine clipping, text tint/alpha, ordered text/image/solid batching and cross-frame atlas/buffer reuse.

Boundary honesty: current U++ does not expose a HarfBuzz-grade complex-script shaper in the inspected text path, so Stage 5 does not claim one. U++ remains font/text authority.

### TASK-010C-A — neutral vector + U++ Painter authority

Published: `e6367d8e72eea4803a3585680674c79784f52bef`

Scope:
- copy-safe backend-neutral `UiPath` Move/Line/Quadratic/Cubic/Close;
- NonZero/EvenOdd fill rules;
- solid, multi-stop linear/radial paints with Pad/Repeat/Reflect spreads;
- stroke width, caps, joins, miter, dash pattern/offset;
- immutable FillPath/StrokePath/DrawSvg recording and deterministic dumps;
- shared `RenderVector` U++ Painter semantic/raster authority;
- U++ curves, gradients, stroke style and SVG replay;
- antialiased raster helper with conservative bounds, malformed-stream rejection and 4096-pixel safety cap;
- `RenderSoftware` uses the same vector authority;
- focused `RenderVectorTest`.

### TASK-010C-B — production GPU vector/SVG path

Published: `0d37b2472c4d49e6908f6acbf5f85cc523193006`
Status: **IMPLEMENTATION COMPLETE — PLATFORM VALIDATION PENDING**

Production design deliberately avoids a second GPU vector/resource authority:
- vector/SVG ops are rasterized by U++ Painter into cached antialiased `Image` values;
- raster scale follows the largest singular value of the active affine transform, clamped 1..8;
- vector cache identity is exact `UiDisplayOp` value equality + raster scale, not an approximate dump/hash;
- materialized vector operations become ordinary `DrawImage` operations;
- the accepted image path owns GPU upload, sRGB conversion, texture cache, descriptors, affine UV clipping, batching and destruction;
- vector sidecar owns CPU Images only and owns zero Vulkan/GpuTexture resources;
- mixed vector + text scenes preserve original order and reuse both image and glyph caches;
- no-vector display lists retain the accepted prior renderer path unchanged.

Coverage added:
- `RenderGpuVectorTest`: exact first-frame vector raster/image uploads, second-frame zero raster/upload work, mixed vector/text ordering, glyph reuse, RenderNull resource cleanup;
- `RenderVulkanVectorTest`: real Vulkan offscreen vector/text rendering, cached repeat, acquired swapchain presentation, zero adapter resources and final ownership diagnostics;
- `RenderVectorTest` strengthened to exercise Reflect and Repeat gradients plus explicit partial-alpha edge evidence from an opaque curved path proving antialiased raster coverage.

Native GPU path tessellation remains a Stage-8 optimization candidate. For v1, U++ Painter is the semantic authority and the accepted sampled image pipeline is the GPU transport.

## Stage 6 - U++ Integration

Active now while final Stage-5 platform validation runs.

Architecture constraints:
- one root GPU/native presentation boundary for a GPU-composited interface; do not create one native child per ordinary control;
- U++ retains control tree, layout, input, focus, state and theme authority;
- controls emit resolved neutral drawing intent/display lists;
- software replay remains available as correctness/fallback path;
- `GpuCtrl` remains appropriate for explicitly embedded accelerated content, not as the implementation mechanism for every normal control.

## Recovery Log

BASE: `0d37b2472c4d49e6908f6acbf5f85cc523193006` / `main`
TASK: Stage-5 final validation + `TASK-011A` Stage-6 root composition boundary
TOUCHED: Stage-5 B — RenderGpu2D vector wrapper/cache + RenderGpuVectorTest/RenderVulkanVectorTest + strengthened RenderVectorTest; status `docs/ACTIVE_WORK.md`
STATUS: Stage 3 PASS; Stage 4 PASS; Stage-5 images/text PASS; Stage-5 vector IMPLEMENTATION COMPLETE / PLATFORM VALIDATION PENDING; Stage 6 inspection/implementation ACTIVE
PUBLISHED: TASK-010C-A `e6367d8e72eea4803a3585680674c79784f52bef`; TASK-010C-B `0d37b2472c4d49e6908f6acbf5f85cc523193006`
VALIDATION: source/dependency/full PR review complete; final Windows/Vulkan Stage-5 gate pending

## Next Action

Issue one bounded Stage-5 Windows/Vulkan acceptance task to Gary, but do not wait for it. Refresh `main`, inspect `docs/UI_GPU_RENDERING_ARCHITECTURE.md`, `docs/ARCHITECTURE.md`, current `GpuCtrl`/presentation tests and examples, then implement `TASK-011A` as a coherent root U++/GPU composition slice with neutral display-list input, software fallback/reference and one presentation surface.
