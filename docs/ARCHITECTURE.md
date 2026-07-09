# Architecture

## Stage 1 Goal

Stage 1 proves the backend-neutral rendering foundation before any GPU backend.
It focuses on recording, inspection, deterministic replay, and a software
reference path.

## Pipeline

```text
Drawing caller
    ↓
UiCanvas
    ↓
UiDisplayListBuilder
    ↓
UiDisplayList
    ↓
SoftwareUiRenderer
```

The future GPU path is intentionally separate:

```text
UiDisplayList
    ↓
UiRenderer2D
    ↓
GpuRhi
    ↓
Vulkan / Metal / WebGPU / OpenGL
```

`UiRenderer2D` and `GpuRhi` are not implemented in this task.

## Package Responsibilities

### RenderCore

Small backend-neutral value types shared by the rendering packages.
It currently carries `Rgba8`, `Transform2D`, and `RoundedRect`.

### RenderCanvas

The high-level recording API and immutable display list.
`UiCanvas` is separate from the future RHI because it describes drawing intent,
not GPU resources or command buffers.

### RenderSoftware

A correctness-oriented replay path that interprets the display list using U++
software painting primitives.

### DisplayListDemo

A plain visual check that shows the direct reference rendering beside the
recorded-and-replayed result, along with the deterministic display-list dump.

### RenderCanvasTest

Unit tests for ordering, value preservation, validity handling, determinism,
and software replay.

## Why Display Lists Are Immutable

The recording model must remain stable after completion so tests and future
tools can inspect it without hidden mutation or backend coupling.

## Why Deterministic Inspection Is Required

Deterministic dumps make it possible to compare recordings, diagnose regressions,
and create stable test expectations without relying on pointer values or runtime
addresses.

## Why The Software Renderer Exists

The software renderer is the reference implementation for correctness. It lets
the project validate recording semantics, replay order, clipping, transforms,
and state restoration before a GPU backend exists.

## Deferred Features

This task deliberately defers:

- Vulkan and all other GPU backends
- text rendering
- images and textures
- gradients
- shadows
- paths beyond simple rectangle and rounded-rectangle primitives
- U++ control integration
- speculative backend packages

## Stage 2

Add the minimal `GpuRhi` contract and a headless `RenderNull` backend without
pulling Vulkan into the recording layer.
This stage now also covers surface, swapchain, and frame lifecycle so the next
step is not starting from a blank screen and a shrug.

For the broader roadmap, see `docs/PROJECT_PLAN.md`.
