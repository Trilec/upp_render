# Active Work Status

This file is the recovery checkpoint for active `upp_render` implementation work.
Update it whenever a coherent checkpoint is published so work can resume from repository state rather than chat history.

## Accepted Baseline

- Branch: `main`
- Stage 3 Vulkan backend/bootstrap: **PASS / 100% accepted**
- Stage-3 Windows acceptance HEAD: `6ab33a42a3421643359cabfdae7afed7628ad349`
- Stage-3 substantive convergence SHA: `ced346bae602ed6b9b34c8a468c19cc26ffc5c08`
- Stage-3 validation: graphics Debug 4/4 + Release 2/2, resource/RHI/frame/clear regressions PASS, Vulkan validation 0/0, final ownership `0/0/0/0/0/0`
- Stage 4 GPU 2D renderer: **PASS / 100% accepted**
- Stage-4 Windows acceptance HEAD: `f8e7b24d510b4b5889370823dc1c0a5ef43a7f54`
- Stage-4 final compile correction SHA: `17c46c69d9961a9b75da98dd4d3e8c2ff17f678a`
- Stage-4 validation: `RenderGpu2DTest`, `GpuCtrlReplayTest`, `GpuCtrlPresentationTest` Debug/Release PASS, Vulkan validation 0/0, lifecycle PASS, final ownership `0/0/0/0/0/0`
- Stage-5 image foundation: **PASS / accepted**
- Stage-5 image implementation SHA: `a11862d138e6b2f06d92067b4b804d8418b69d32`
- Stage-5 image Windows acceptance HEAD: `f2cd2bdf2ff7c05f7b883ef32405653ab198a98b`
- Stage-5 image validation: `RenderImageTest`, `RenderSampledRhiTest`, `RenderGpu2DTest` Debug/Release PASS; `RenderVulkanImageTest` Debug 4/4 + Release 2/2; Vulkan graphics/resource/GpuCtrl regressions PASS; Vulkan validation 0/0; adapter resources 0; final Vulkan ownership `0/0/0/0/0/0`; clean tree; no local fix

## Current Objective

Proceed directly through Stage 5 — Text, Images and Vector Rendering.

Active implementation task: `TASK-010B` — shaped text + glyph cache/atlas using U++ text/font authority and the accepted sampled-image infrastructure.

## Stage 3 - Vulkan Backend / Bootstrap

**100% complete and platform accepted.**

Accepted implementation includes runtime/device/surface/swapchain ownership, real resources, shaders, pipelines, commands, draw submission, neutral frame lifecycle, exact swapchain format identity, session-authoritative presentation, resize/recreation and deterministic cleanup.

Key SHAs:
- S17C-A: `28c303d4859b7b6fdce3380e23fcab68aa84c731`
- final substantive convergence: `ced346bae602ed6b9b34c8a468c19cc26ffc5c08`
- accepted Windows HEAD: `6ab33a42a3421643359cabfdae7afed7628ad349`

## Stage 4 - GPU 2D Renderer

**100% complete and platform accepted.**

Accepted renderer scope includes production `UiRenderer2D`, immutable display-list replay, fills/strokes/rounded rectangles, full affine transforms, nested Save/Restore, clipping, SourceOver, batching, persistent GPU resources, live `GpuCtrl` integration and software-reference semantic parity.

Key SHAs:
- renderer core: `ca972a087c63a8d54a7f2a9e1683c906b6c747a4`
- blending: `ba2bfcfc76c9e1fd0b6c7c5f3347a22882d41b54`
- live control: `b15c7579a0471290dc416131ebe9180a4c14be05`
- semantic parity: `b8a0993fe36eb87a1c99ae6a5d59c9da703f5953`
- compile correction: `17c46c69d9961a9b75da98dd4d3e8c2ff17f678a`
- accepted Windows HEAD: `f8e7b24d510b4b5889370823dc1c0a5ef43a7f54`

## Stage 5 - Text, Images and Vector Rendering

Active now.

Project-plan scope:
- images / texture-backed content
- text shaping
- glyph caching / atlas
- vector paths
- gradients
- anti-aliasing
- icon and SVG geometry support

### Accepted TASK-010A1/A2 — image recording + sampled-texture contract

Publication merge: `76f456381f5580d1399bc69975218f38c681ff68`

Accepted scope includes `UiCanvas::DrawImage`, immutable image display ops, deterministic dump, software replay, neutral UV vertex layout, bounded sampled-texture RHI contract, `RenderNull` validation authority, sRGB texture uploads, sampled-resource lifetime semantics and focused image/RHI tests with no Vulkan vocabulary in neutral APIs.

### Accepted TASK-010A3 — production sampled-image rendering

Publication merge: `a11862d138e6b2f06d92067b4b804d8418b69d32`
Windows acceptance HEAD: `f2cd2bdf2ff7c05f7b883ef32405653ab198a98b`

Accepted scope:
- production `UiRenderer2D::DrawImage` through real sampled GPU textures
- affine image geometry + UV clipping
- ordered solid/image batching while preserving Stage-4 solid-only batching
- renderer-owned immutable U++ `Image` cache
- explicit premultiplied-to-straight RGBA conversion before sRGB upload
- reusable textured buffer/pipeline cache
- one sampled slot with `Position2Uv2Color4F`, SourceOver, Linear + ClampToEdge
- backend-private Vulkan sampler/descriptors/image views without neutral API leakage
- accepted Stage-3/4 Vulkan ownership authority preserved
- real offscreen + acquired-swapchain sampled rendering and presentation

Acceptance evidence:
- required A3 SHA ancestor: PASS
- `RenderImageTest` Debug/Release: PASS
- `RenderSampledRhiTest` Debug/Release: PASS
- `RenderGpu2DTest` Debug/Release: PASS
- `RenderVulkanImageTest` Debug: 4/4 PASS
- `RenderVulkanImageTest` Release: 2/2 PASS
- Vulkan graphics/resource/GpuCtrl focused regressions: PASS
- Vulkan validation: 0 warnings / 0 errors
- adapter live resources: 0
- final Vulkan ownership: `0/0/0/0/0/0`
- final tree clean; no validator edits

### Stage-5 implementation direction

1. `TASK-010B` — shaped text + glyph cache/atlas using U++ font/text authority; reuse the accepted sampled-image path rather than inventing a second texture system.
2. `TASK-010C` — vector paths, gradients and anti-aliasing, then icon/SVG geometry integration.
3. Final Stage-5 Windows acceptance across neutral/software/Vulkan/live-control paths.

Do not replace U++ font/theme authority and do not leak backend types into public neutral APIs. Exact GPU pixel readback remains Stage-8 hardening rather than expanding the Stage-5 public contract.

## Recovery Log

BASE: `f2cd2bdf2ff7c05f7b883ef32405653ab198a98b` / `main`
TASK: `TASK-010B` shaped text + glyph cache/atlas
TOUCHED: status — `docs/ACTIVE_WORK.md`; implementation not started at this checkpoint
STATUS: Stage 3 PASS / 100%; Stage 4 PASS / 100%; Stage-5 image foundation PASS / accepted; TASK-010B ACTIVE
PUBLISHED: Stage-5 image A1/A2 `76f456381f5580d1399bc69975218f38c681ff68`; A3 `a11862d138e6b2f06d92067b4b804d8418b69d32`; A3 acceptance evidence HEAD `f2cd2bdf2ff7c05f7b883ef32405653ab198a98b`
VALIDATION: image path fully platform accepted; text/vector work not yet validated

## Next Action

Refresh current `main`, inspect U++ font/text shaping and glyph-raster authority plus current RenderCanvas/RenderSoftware/RenderGpu2D dependency slice. Implement TASK-010B in one substantial recoverable slice: neutral text recording, software parity, U++-authoritative shaping/metrics, glyph bitmap/atlas caching that reuses sampled-texture infrastructure, ordered text/image/solid GPU replay, focused neutral/renderer/Vulkan/live-control tests, source review, publish, then Windows acceptance. Proceed directly to TASK-010C after text acceptance.