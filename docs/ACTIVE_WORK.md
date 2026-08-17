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
- TASK-010B-W1: **PASS** — RenderTextTest/RenderGpuTextTest Debug+Release PASS; Vulkan text Debug 4/4 + Release 2/2; GPU2D/image/GpuCtrl regressions PASS; Vulkan validation 0/0; adapter resources 0; Vulkan ownership `0/0/0/0/0/0`
- TASK-010B-W1 mechanical source corrections: `6f322ad4ecc4b2364d020a00bb3676695cbc9cab`

## Current Objective

Finish Stage 5 with `TASK-010C`: vector paths, gradients, anti-aliasing and icon/SVG rendering, then run one final Stage-5 Windows/Vulkan acceptance gate.

## Stage 5 Accepted Scope

### Images

Accepted production image path includes immutable `DrawImage`, deterministic software replay, sampled-texture RHI/Null authority, sRGB uploads, affine UV clipping, ordered image/solid batching, renderer image caching and real Vulkan offscreen/swapchain presentation.

### Text

TASK-010B is platform accepted. Scope includes immutable `DrawText`, fractional U++ software replay, U++-authoritative glyph metrics/fallback/composition, persistent sampled glyph atlas pages, padded partial uploads, affine clipping, text tint/alpha, ordered text/image/solid batching, and cross-frame atlas/buffer reuse.

Boundary honesty: current U++ does not expose a HarfBuzz-grade complex-script shaper in the inspected text path, so Stage 5 does not claim one. U++ remains font/text authority. Exact GPU pixel readback remains Stage-8 hardening.

## TASK-010C Progress

### TASK-010C-A — neutral vector + U++ Painter authority

Published: `e6367d8e72eea4803a3585680674c79784f52bef`
Status: **SOURCE IMPLEMENTATION COMPLETE — PLATFORM VALIDATION DEFERRED TO MEANINGFUL STAGE-5 GATE**

Published scope:

- copy-safe backend-neutral `UiPath` with Move/Line/Quadratic/Cubic/Close;
- `UiFillRule` NonZero/EvenOdd;
- solid, multi-stop linear and radial `UiPaint` with Pad/Repeat/Reflect spread;
- `UiStrokeStyle` width, Butt/Square/Round caps, Miter/Round/Bevel joins, miter limit and dash pattern/offset;
- immutable `FillPath`, `StrokePath` and `DrawSvg` display-list operations;
- deterministic path/paint/stroke/SVG dump evidence and deep-copy value semantics;
- shared `RenderVector` package keeps U++ Painter as vector semantic/raster authority;
- U++ curves, fill rules, multi-stop gradients, stroke style and SVG replay;
- antialiased vector raster helper with conservative path/stroke bounds and 4096-pixel safety cap;
- malformed segment-before-MoveTo streams rejected deterministically;
- `RenderSoftware` consumes the same RenderVector authority;
- focused `RenderVectorTest` covers recording, determinism, curves, even-odd holes, gradients, dashed round strokes, SVG, visible software replay and vector raster output.

### TASK-010C-B — active GPU production slice

Implement a renderer-owned vector/SVG raster cache that reuses the accepted sampled-texture pipeline rather than introducing a second tessellation/resource authority. Raster scale follows the current affine transform, U++ Painter supplies antialiased pixels on cache miss, and repeated vector content reuses uploaded GPU textures across frames. Pure pre-vector display lists must continue through the already accepted renderer path unchanged.

This is the v1 production strategy. Native GPU path tessellation remains an optimization candidate for Stage 8 after semantic parity and integration are stable.

Do not introduce Vulkan types into neutral APIs, do not replace U++ theme/font/Painter authority, and do not fork a second texture/resource ownership system.

## Recovery Log

BASE: `e6367d8e72eea4803a3585680674c79784f52bef` / `main`
TASK: `TASK-010C-B` production GPU vector/SVG raster cache + final Stage-5 acceptance surface
TOUCHED: TASK-010C-A published paths in RenderCanvas/RenderVector/RenderSoftware/RenderVectorTest; TASK-010C-B now moves into RenderGpu2D + Null/Vulkan vector tests
STATUS: Stage 3 PASS; Stage 4 PASS; Stage-5 images PASS; Stage-5 text PASS; TASK-010C-A PUBLISHED; TASK-010C-B ACTIVE
PUBLISHED: TASK-010C-A `e6367d8e72eea4803a3585680674c79784f52bef`
VALIDATION: TASK-010B-W1 PASS; TASK-010C-A source/dependency review complete; Windows vector validation intentionally grouped with TASK-010C-B/final Stage-5 gate

## Next Action

Refresh current `main`, implement the persistent GPU vector/SVG cache in `UiRenderer2D`, preserve the exact accepted path for display lists without vector content, add Null and real Vulkan vector tests, review/publish the coherent production slice, then issue one final Stage-5 Windows/Vulkan acceptance task to Gary.
