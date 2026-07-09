# Project Plan

`upp_render` is being built in staged layers so the recording model can settle
before any GPU backend is bolted on with wishful thinking.

## Stage 1 - Backend-Neutral Foundation

Done or underway:

- `RenderCore` value types
- `UiCanvas` recording API
- immutable display lists
- deterministic dumps and inspection
- software replay
- unit tests and a visual demo

## Stage 2 - RHI Contract and Null Backend

Define `GpuRhi` as the minimal GPU-facing contract and implement `RenderNull`
for headless validation of command ordering, lifetime rules, and state handling.

## Stage 3 - Vulkan Bootstrap

Add the first GPU backend:

- instance and validation setup
- device and queue selection
- surface and swapchain management
- frame submission and synchronization
- resource upload and deferred destruction

## Stage 4 - GPU 2D Renderer

Build `UiRenderer2D` on top of `GpuRhi` for:

- filled rectangles
- strokes and borders
- rounded rectangles
- clipping and transforms
- opacity and batching

## Stage 5 - Text and Vector Rendering

Add:

- text shaping
- glyph caching
- vector paths
- gradients
- anti-aliasing
- icon and SVG geometry support

## Stage 6 - U++ Integration

Connect the new pipeline back into existing U++ controls and painting.

## Stage 7 - Effects, Compute, and Specialized Views

Add the follow-on pieces that depend on the earlier layers:

- effects and layers
- compute-backed helpers
- offscreen and specialized views
- broader GPU capability plumbing

## Stage 8 - Hardening and Future Backends

Stabilize the stack, compare software and GPU outputs, and then evaluate any
additional backends beyond the first ones already planned.
