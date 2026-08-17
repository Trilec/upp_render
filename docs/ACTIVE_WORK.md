# Active Work Status

This file is the recovery checkpoint for active `upp_render` implementation work.
Update it whenever a coherent checkpoint is published so work can resume from repository state rather than chat history.

## Accepted Baseline

- Branch: `main`
- Stage 3 Vulkan backend/bootstrap: **PASS / 100% accepted**
- Stage-3 Windows acceptance HEAD: `6ab33a42a3421643359cabfdae7afed7628ad349`
- Stage-3 substantive convergence SHA: `ced346bae602ed6b9b34c8a468c19cc26ffc5c08`
- Stage-3 validation: `RenderVulkanGraphicsTest` Debug 4/4, Release 2/2; resource Debug/Release; RHI Debug/Release; frame and clear regressions all PASS
- Stage-3 Vulkan validation: 0 warnings / 0 errors
- Stage-3 final ownership: runtime/instance/debug messenger/surface/device/swapchain = `0/0/0/0/0/0`
- Stage 4 GPU 2D renderer: **PASS / 100% accepted**
- Stage-4 Windows acceptance HEAD: `f8e7b24d510b4b5889370823dc1c0a5ef43a7f54`
- Stage-4 final compile correction SHA: `17c46c69d9961a9b75da98dd4d3e8c2ff17f678a`
- Stage-4 validation: `RenderGpu2DTest`, `GpuCtrlReplayTest`, and `GpuCtrlPresentationTest` Debug/Release all PASS
- Stage-4 Vulkan validation: 0 warnings / 0 errors
- Stage-4 lifecycle: refresh, resize/recreation, hide/show and GPU readiness PASS; no crash/hang/device loss
- Stage-4 final ownership: runtime/instance/debug messenger/surface/device/swapchain = `0/0/0/0/0/0`

## Current Objective

Proceed directly through Stage 5 — Text, Images and Vector Rendering.

Active implementation task: `TASK-010A3` — implement Vulkan sampled-texture binding and production `UiRenderer2D::DrawImage` on top of the published neutral/software/RHI image foundation.

## Stage 3 - Vulkan Backend / Bootstrap

**100% complete and platform accepted.**

Accepted implementation includes runtime/device/surface/swapchain ownership, real resources, shaders, pipelines, commands, draw submission, neutral frame lifecycle, exact swapchain format identity, session-authoritative presentation, resize/recreation and deterministic cleanup.

Key publication SHAs:

- S17C-A: `28c303d4859b7b6fdce3380e23fcab68aa84c731`
- final Stage-3 substantive convergence: `ced346bae602ed6b9b34c8a468c19cc26ffc5c08`
- final Stage-3 acceptance HEAD: `6ab33a42a3421643359cabfdae7afed7628ad349`

## Stage 4 - GPU 2D Renderer

**100% complete and platform accepted.**

Accepted renderer scope includes production `UiRenderer2D`, immutable display-list replay, fills/strokes/rounded rectangles, full affine transforms, nested Save/Restore, clipping, SourceOver, batching, persistent GPU resources, live `GpuCtrl` integration and software-reference semantic parity.

Key publication SHAs:

- TASK-009A renderer core: `ca972a087c63a8d54a7f2a9e1683c906b6c747a4`
- TASK-009B blending: `ba2bfcfc76c9e1fd0b6c7c5f3347a22882d41b54`
- TASK-009C live control integration: `b15c7579a0471290dc416131ebe9180a4c14be05`
- TASK-009D semantic parity: `b8a0993fe36eb87a1c99ae6a5d59c9da703f5953`
- TASK-009-W1-R1 compile correction: `17c46c69d9961a9b75da98dd4d3e8c2ff17f678a`
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

### Published TASK-010A1/A2 — image recording + sampled-texture contract

Publication merge: `76f456381f5580d1399bc69975218f38c681ff68`

Published scope:

- `UiCanvas::DrawImage(Rectf, Image)` and immutable `UiDisplayOpType::DrawImage`
- display lists retain U++ `Image` values without duplicating a backend-specific image authority
- deterministic image dump includes destination, size and stable pixel hash rather than pointer/runtime identity
- `SoftwareUiRenderer` replays DrawImage through U++ Painter
- focused `RenderImageTest` covers recording, deterministic dump, immutability and visible software replay
- neutral `GpuVertexLayout::Position2Uv2Color4F`
- bounded one-slot sampled-texture pipeline contract with explicit filter/address mode
- neutral `GpuDevice::SetSampledTexture`
- `RenderNull` is the validation authority for sampled slot/pipeline/usage/lifetime semantics
- regular `RGBA8Srgb` / `BGRA8Srgb` uploads are deliberately resolved as four-byte 8-bit texture uploads
- sampled textures remain alive through recorded unsubmitted command work and become destroyable after synchronous submission
- focused `RenderSampledRhiTest` covers sRGB upload, sampled pipeline validation, binding, missing-binding failure and lifetime
- no Vulkan types or descriptor vocabulary leaked into neutral public APIs

Boundary honesty:

- TASK-010A1/A2 does **not** claim real GPU DrawImage support yet
- Vulkan sampler/descriptor/image-view binding and production textured `UiRenderer2D` replay are `TASK-010A3`

### Stage-5 implementation direction

1. `TASK-010A3` — Vulkan sampled descriptors/sampler binding + production GPU DrawImage with ordered solid/image batches and texture cache.
2. `TASK-010B` — shaped text + glyph cache/atlas using U++ font/text authority; reuse the sampled-image path rather than inventing a second texture system.
3. `TASK-010C` — vector paths, gradients and anti-aliasing, then icon/SVG geometry integration.
4. Stage-5 Windows acceptance across neutral/Null/software/Vulkan/live-control paths.

Do not replace U++ font/theme authority and do not leak Vulkan types into public neutral APIs. Exact GPU pixel readback remains Stage-8 hardening rather than expanding the Stage-5 public contract.

## Recovery Log

BASE: `76f456381f5580d1399bc69975218f38c681ff68` / `main`
TASK: `TASK-010A3` Vulkan sampled textures + production `UiRenderer2D::DrawImage`
TOUCHED: published A1/A2 — `render/RenderCanvas/RenderCanvas.h`, `render/RenderCanvas/RenderCanvas.cpp`, `render/RenderSoftware/RenderSoftware.cpp`, `render/RenderRhi/RenderRhi.h`, `render/RenderRhi/RenderRhi.cpp`, `render/RenderNull/RenderNull.h`, `render/RenderNull/RenderNull.cpp`, `tests/RenderImageTest/*`, `tests/RenderSampledRhiTest/*`; status — `docs/ACTIVE_WORK.md`
STATUS: Stage 3 PASS / 100%; Stage 4 PASS / 100%; Stage 5 A1/A2 PUBLISHED; A3 ACTIVE
PUBLISHED: Stage-5 A1/A2 `76f456381f5580d1399bc69975218f38c681ff68`
VALIDATION: A1/A2 source/static + aggregate PR review complete; Windows/platform validation pending; real Vulkan sampled-image path not yet claimed

## Next Action

Refresh current `main`, implement `TASK-010A3` on a recovery branch: add Vulkan sampler/descriptor binding without changing ownership authority, extend `UiRenderer2D` with deterministic sampled-image shaders, transformed/clipped UV geometry, ordered solid/image batches and renderer-owned immutable-image texture caching, add focused Null/Vulkan image tests, publish/verify the checkpoint, then give Gary one focused Stage-5 image acceptance before moving immediately into shaped text/glyph atlas work.
