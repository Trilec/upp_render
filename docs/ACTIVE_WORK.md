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
- TASK-010B-W1 mechanical source corrections are published at `6f322ad4ecc4b2364d020a00bb3676695cbc9cab` and present in current `main`.

## Current Objective

Finish Stage 5 with `TASK-010C`: vector paths, gradients, anti-aliasing, then icon/SVG geometry integration. Keep U++/Painter as the semantic reference and preserve the accepted Stage-3/4/image/text ownership paths.

## Stage 5 Accepted Scope

### Images

Accepted production image path includes immutable `DrawImage`, deterministic software replay, sampled-texture RHI/Null authority, sRGB uploads, affine UV clipping, ordered image/solid batching, renderer image caching and real Vulkan offscreen/swapchain presentation.

### Text

TASK-010B is platform accepted. Scope includes immutable `DrawText`, fractional U++ software replay, U++-authoritative glyph metrics/fallback/composition, persistent sampled glyph atlas pages, padded partial uploads, affine clipping, text tint/alpha, ordered text/image/solid batching, and cross-frame atlas/buffer reuse.

Boundary honesty: current U++ does not expose a HarfBuzz-grade complex-script shaper in the inspected text path, so Stage 5 does not claim one. U++ remains font/text authority. Exact GPU pixel readback remains Stage-8 hardening.

## TASK-010C Direction

Implement in coherent published slices rather than operation-sized milestones:

1. neutral immutable path/fill/stroke/gradient recording with deterministic dump and software-reference replay;
2. production GPU tessellation/coverage for filled and stroked paths, including curves and fill-rule semantics required by U++;
3. linear/radial gradients with backend-neutral display intent;
4. anti-aliasing without duplicating U++ semantic authority or leaking Vulkan types;
5. icon/SVG geometry ingestion through U++ Painter/SVG facilities where practical, producing ordinary neutral path/paint intent rather than a second renderer;
6. focused Null/software/Vulkan tests plus final Stage-5 regression acceptance.

Do not introduce Vulkan types into neutral APIs, do not replace U++ theme/font authority, and do not fork a second texture/resource ownership system.

## Recovery Log

BASE: `ef476ba6dbce9b3fb38cc5ff2382d14db76126f1` / `main`
TASK: `TASK-010C` vector paths + gradients + AA + icon/SVG geometry
TOUCHED: status `docs/ACTIVE_WORK.md`; TASK-010C implementation starts from this checkpoint
STATUS: Stage 3 PASS; Stage 4 PASS; Stage-5 images PASS; Stage-5 text PASS; TASK-010C ACTIVE
PUBLISHED: text `f98cce413b1992cfaef55669d4672824fe703b5f`; text validation fixes `6f322ad4ecc4b2364d020a00bb3676695cbc9cab`
VALIDATION: TASK-010B-W1 PASS; Stage-5 vector/gradient work pending

## Next Action

Inspect current RenderCanvas/RenderSoftware/RenderGpu2D plus U++ Painter path, gradient and SVG authority. Implement TASK-010C in large recoverable checkpoints, publishing each coherent slice with source review. Use Gary only for meaningful Windows/Vulkan acceptance gates after the supervisor has completed the substantive coding.
