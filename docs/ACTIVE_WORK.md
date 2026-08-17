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
- TASK-010B-W1: **PASS** — text/software/Null Debug+Release PASS; Vulkan text Debug 4/4 + Release 2/2; GPU2D/image/GpuCtrl regressions PASS; Vulkan validation 0/0; adapter resources 0; Vulkan ownership `0/0/0/0/0/0`

## Current Objective

Finish Stage 5 with `TASK-010C`: vector paths, gradients, anti-aliasing, then icon/SVG geometry integration. Keep U++/Painter as the semantic reference and preserve the accepted Stage-3/4/image/text ownership paths.

## Stage 5 Accepted Scope

### Images

Accepted production image path includes immutable `DrawImage`, deterministic software replay, sampled-texture RHI/Null authority, sRGB uploads, affine UV clipping, ordered image/solid batching, renderer image caching and real Vulkan offscreen/swapchain presentation.

### Text

TASK-010B is platform accepted. Published scope includes:

- immutable `DrawText(Pointf, WString, Font, Rgba8)` display-list recording;
- fractional software replay through U++ Painter/DrawText;
- U++-authoritative glyph metrics, fallback/replacement/composition and missing-glyph behaviour;
- persistent sampled `RGBA8Srgb` glyph atlas with padded subregion uploads;
- realised Font + code-point glyph cache identity;
- text tint/alpha through the accepted SourceOver sampled pipeline;
- affine transform + UV-preserving clipping;
- ordered text/image/solid batching and cross-frame atlas/buffer reuse;
- exact byte-preserved no-text Stage-4/image renderer path.

Gary's Windows validation found two mechanical source omissions while all requested tests passed after local correction:

1. `RenderSoftware.cpp` missing the closing brace for local `StateGuard`.
2. `RenderNull.h` missing four const live-resource count accessors used by `RenderGpuTextTest`.

These corrections are being absorbed by the supervisor before TASK-010C and are not delegated to Gary.

Boundary honesty: current U++ does not expose a HarfBuzz-grade complex-script shaping authority in the inspected text path, so Stage 5 does not claim one. U++ remains font/text authority. Exact GPU pixel readback remains Stage-8 hardening.

## TASK-010C Direction

Implement in coherent published slices rather than operation-sized milestones:

1. neutral immutable path/fill/stroke/gradient recording with deterministic dump and software-reference replay;
2. production GPU tessellation/coverage for filled and stroked paths, including curves and holes/fill rule where required by U++ semantics;
3. linear/radial gradient rendering with backend-neutral display intent;
4. anti-aliasing strategy that does not duplicate U++ semantic authority or leak Vulkan types;
5. icon/SVG geometry ingestion using U++ Painter/SVG facilities where possible, producing ordinary neutral path/paint intent rather than a second renderer;
6. focused Null/software/Vulkan tests plus final Stage-5 regression acceptance.

Do not introduce Vulkan types into neutral APIs, do not replace U++ theme/font authority, and do not fork a second texture/resource ownership system.

## Recovery Log

BASE: `91f1fe3cad91b5afe00de4afd6398b773e8f4715` / `main` (plus the immediately absorbed TASK-010B-W1 mechanical fixes)
TASK: `TASK-010C` vector paths + gradients + AA + icon/SVG geometry
TOUCHED: acceptance fixes — `render/RenderSoftware/RenderSoftware.cpp`, `render/RenderNull/RenderNull.h`, `docs/ACTIVE_WORK.md`; TASK-010C implementation follows
STATUS: Stage 3 PASS; Stage 4 PASS; Stage-5 images PASS; Stage-5 text PASS; TASK-010C ACTIVE
PUBLISHED: text `f98cce413b1992cfaef55669d4672824fe703b5f`; text acceptance evidence HEAD `91f1fe3cad91b5afe00de4afd6398b773e8f4715`
VALIDATION: TASK-010B-W1 PASS after two local mechanical diagnostics; supervisor corrections pending publication in this checkpoint

## Next Action

Publish the two TASK-010B-W1 mechanical corrections and this acceptance checkpoint. Refresh `main`, inspect current RenderCanvas/RenderSoftware/RenderGpu2D plus U++ Painter path/gradient/SVG authority, then implement TASK-010C in large recoverable checkpoints with source review and Windows validation only at meaningful gates.
