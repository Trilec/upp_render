# Active Work Status

This file is the recovery checkpoint for active `upp_render` implementation work.
Update it whenever a coherent checkpoint is published so work can resume from repository state rather than chat history.

## Accepted Baseline

- Branch: `main`
- Stage 3 Vulkan backend/bootstrap: **PASS / 100% accepted**
- Stage-3 Windows acceptance HEAD: `6ab33a42a3421643359cabfdae7afed7628ad349`
- Stage-3 substantive convergence: `ced346bae602ed6b9b34c8a468c19cc26ffc5c08`
- Stage-3 validation: graphics Debug 4/4 + Release 2/2, resource/RHI/frame/clear regressions PASS, Vulkan validation 0/0, ownership `0/0/0/0/0/0`
- Stage 4 GPU 2D renderer: **PASS / 100% accepted**
- Stage-4 Windows acceptance HEAD: `f8e7b24d510b4b5889370823dc1c0a5ef43a7f54`
- Stage-4 validation: `RenderGpu2DTest`, `GpuCtrlReplayTest`, `GpuCtrlPresentationTest` Debug/Release PASS, Vulkan validation 0/0, lifecycle PASS, ownership `0/0/0/0/0/0`
- Stage-5 image foundation: **PASS / accepted**
- Stage-5 image implementation: `a11862d138e6b2f06d92067b4b804d8418b69d32`
- Stage-5 image Windows acceptance HEAD: `f2cd2bdf2ff7c05f7b883ef32405653ab198a98b`
- Stage-5 image validation: image/sampled/GPU2D Debug+Release PASS; Vulkan image Debug 4/4 + Release 2/2; regressions PASS; validation 0/0; adapter resources 0; ownership `0/0/0/0/0/0`

## Current Objective

Finish Stage 5 as quickly as possible without weakening the accepted Stage-3/4/image ownership and renderer contracts.

Active validation task: `TASK-010B-W1` — Windows/Vulkan acceptance of the published U++-authoritative text + glyph-atlas slice.

Next implementation task after text acceptance: `TASK-010C` — vector paths, gradients, anti-aliasing and icon/SVG geometry integration.

## Stage 3 - Vulkan Backend / Bootstrap

**100% complete and platform accepted.**

Accepted implementation includes runtime/device/surface/swapchain ownership, real resources, shaders, pipelines, commands, draw submission, neutral frame lifecycle, exact swapchain format identity, session-authoritative presentation, resize/recreation and deterministic cleanup.

## Stage 4 - GPU 2D Renderer

**100% complete and platform accepted.**

Accepted renderer scope includes production `UiRenderer2D`, immutable display-list replay, fills/strokes/rounded rectangles, affine transforms, nested Save/Restore, clipping, SourceOver, batching, persistent GPU resources, live `GpuCtrl` integration and software-reference semantic parity.

## Stage 5 - Text, Images and Vector Rendering

Active now.

### Accepted image foundation

- A1/A2 publication: `76f456381f5580d1399bc69975218f38c681ff68`
- A3 production image publication: `a11862d138e6b2f06d92067b4b804d8418b69d32`
- Windows acceptance HEAD: `f2cd2bdf2ff7c05f7b883ef32405653ab198a98b`
- status: **PASS / accepted**

Image scope includes immutable `DrawImage`, deterministic software replay, sampled-texture RHI/Null authority, sRGB uploads, production GPU image replay, affine UV clipping, ordered image/solid batching, image texture caching and real Vulkan offscreen/swapchain presentation.

### Published TASK-010B — U++-authoritative text + glyph atlas

Publication merge: `f98cce413b1992cfaef55669d4672824fe703b5f`

Status: **IMPLEMENTATION COMPLETE — PLATFORM VALIDATION PENDING**

Published scope:

- neutral immutable `UiCanvas::DrawText(Pointf, WString, Font, Rgba8)` / `UiDisplayOpType::DrawText`
- deterministic text dump records origin, code-point count/hash, Font value and colour without pointer/runtime identity
- software reference preserves fractional origin via Painter transform then U++ `DrawText`, retaining U++ replacement/composed/missing-glyph behavior
- GPU glyph rasterization uses U++ `ImagePainter::DrawText`; no second font engine or platform-specific font ownership
- glyph advance uses U++ `Font::GetWidth`, which resolves through U++ glyph metrics
- renderer-owned persistent 1024x1024 sampled `RGBA8Srgb` glyph-atlas pages
- one transparent padding texel around each uploaded glyph; only padded glyph subregions are uploaded, not whole-page clears
- atlas cache key is realized Font value + code point; underline/strikeout are excluded from glyph cache identity and rendered as geometry
- text colour/alpha is carried as sampled-vertex tint through the accepted SourceOver textured pipeline
- text, image and solid operations retain immutable display-list order; compatible glyphs on one atlas page batch into one sampled draw
- full affine transform + UV-preserving device-space clipping reuse the accepted image geometry path
- atlas and textured vertex-buffer reuse persists across frames; cached repeated glyphs perform no new raster/upload work
- impossible oversized font metrics fail before allocating an unbounded raster canvas
- accepted Stage-4/image `RenderGpu2D.cpp` implementation is byte-preserved as private `RenderGpu2DBase.inc`; every display list without `DrawText` executes that exact accepted implementation
- renderer/package gains `Painter` only for U++ glyph raster authority; no Vulkan or platform font types leak into neutral APIs

Focused tests added:

- `RenderTextTest` — recording/dump/immutability/software visibility, equivalent value determinism, empty text, fractional origin and non-ASCII replay
- `RenderGpuTextTest` — Null GPU text path, ABBA distinct-glyph caching, one atlas page, two padded uploads on first frame, zero on second, ordered solid/text/solid batching, buffer reuse, explicit cleanup
- `RenderVulkanTextTest` — real Vulkan offscreen glyph-atlas rendering, cached repeat frame, acquired swapchain text render/present, validation counters and zero final ownership

Boundary honesty:

- this slice follows the text capabilities actually present in current U++; it does **not** claim HarfBuzz-grade complex-script shaping because no such shaper exists in the inspected U++ text path
- U++ remains authoritative for Font, glyph metrics, fallback/replacement/composition/missing-glyph raster behavior
- exact GPU pixel readback remains Stage-8 hardening
- no TASK-010B PASS claim until Windows/Vulkan validation succeeds

### Remaining Stage-5 work

1. `TASK-010B-W1` — focused Windows acceptance of text/glyph-atlas path.
2. `TASK-010C` — vector paths, gradients, anti-aliasing, icon/SVG geometry.
3. Final Stage-5 regression/acceptance pass.

## Recovery Log

BASE: `f98cce413b1992cfaef55669d4672824fe703b5f` / `main`
TASK: `TASK-010B-W1` Windows/Vulkan acceptance of U++-authoritative GPU text + glyph atlas
TOUCHED: `render/RenderCanvas/RenderCanvas.h`, `render/RenderCanvas/RenderCanvas.cpp`, `render/RenderSoftware/RenderSoftware.cpp`, `render/RenderGpu2D/RenderGpu2D.h`, `render/RenderGpu2D/RenderGpu2D.cpp`, `render/RenderGpu2D/RenderGpu2DBase.inc`, `render/RenderGpu2D/RenderGpu2DText.cpp`, `render/RenderGpu2D/RenderGpu2D.upp`, `tests/RenderTextTest/*`, `tests/RenderGpuTextTest/*`, `tests/RenderVulkanTextTest/*`; status `docs/ACTIVE_WORK.md`
STATUS: Stage 3 PASS / 100%; Stage 4 PASS / 100%; Stage-5 image PASS; TASK-010B IMPLEMENTATION COMPLETE — PLATFORM VALIDATION PENDING
PUBLISHED: text `f98cce413b1992cfaef55669d4672824fe703b5f`
VALIDATION: source/dependency/full PR review complete; Windows compile/runtime/Vulkan validation pending

## Next Action

Gary runs `TASK-010B-W1` against current `main`, confirming `f98cce413b1992cfaef55669d4672824fe703b5f` is an ancestor. Validate `RenderTextTest`, `RenderGpuTextTest`, `RenderVulkanTextTest`, then Stage-4/image regressions, Vulkan validation 0/0, adapter resources zero and final Vulkan ownership zero. Stop on first genuine blocker. On PASS, record acceptance and proceed immediately to `TASK-010C` vector/gradient/AA/icon-SVG implementation.