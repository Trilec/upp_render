# Architecture

`upp_render` is layered to keep upstream validation, application-facing APIs, testing, and diagnostics separate.

# upp_render Project Plan

**Status:** Initial architecture plan
**Repository:** `Trilec/upp_render`
**First platform:** Windows 11, CLANGx64
**First GPU backend:** Vulkan
**Initial Vulkan baseline:** Vulkan 1.3
**Project manager and publisher:** Repository owner
**Technical supervisor:** ChatGPT
**Initial implementer:** Gary, intern programmer

---

## 1. Project purpose

`upp_render` will provide a modern rendering architecture for Ultimate++ while preserving the existing U++ control, layout, input, event and theming systems.

The intended pipeline is:

```text
Ctrl::Paint
    ↓
UiPaintContext
    ↓
UiCanvas
    ↓
UiDisplayList
    ↓
UiRenderer2D
    ↓
GpuRhi
    ↓
Vulkan / Metal / WebGPU / OpenGL
```

Existing U++ controls should not need to understand Vulkan or another native graphics API.

The existing `Draw` and software rendering paths will remain useful for:

* compatibility;
* printing;
* image generation;
* reference rendering;
* unsupported hardware;
* testing and diagnostics.

The GPU renderer is an additional modern rendering path, not an immediate deletion of the existing U++ rendering system.

---

## 2. Project roles

### Project manager and publisher

The manager:

* approves scope and architectural direction;
* decides when work is published;
* owns repository releases and public communication;
* may assign additional reviewers or agents;
* has final authority over changes.

### Technical supervisor

The supervisor:

* maintains the architecture and roadmap;
* writes implementation tasks;
* reviews Gary’s work;
* checks tests, documentation and architecture boundaries;
* accepts, rejects or returns work for correction;
* prepares the next task after acceptance.

### Gary — intern programmer

Gary:

* implements only the assigned task;
* works from the written specification;
* reports uncertainties rather than inventing architecture;
* builds and tests locally;
* may make local commits;
* must not push, publish, merge, tag or create releases;
* supplies a patch, changed files and validation report to the manager.

### Additional programmers or agents

Additional contributors may review or implement isolated tasks, but they must follow the same architecture and task boundaries. Architectural changes require manager and supervisor approval.

---

## 3. Architectural rules

### 3.1 Backend-neutral public API

No Vulkan, Metal, WebGPU, OpenGL or native platform type may appear in:

* `RenderCore`;
* `RenderCanvas`;
* `Render2D`;
* U++ control APIs;
* public display-list commands.

Native API types remain inside their backend package.

### 3.2 Recorded drawing

Controls record drawing operations into an immutable `UiDisplayList`.

A display list must be:

* deterministic;
* replayable;
* inspectable in tests;
* independent of the active GPU backend;
* immutable once recording is finished.

### 3.3 Separate high-level and low-level layers

`UiCanvas` describes 2D drawing intent.

`GpuRhi` describes low-level GPU resources and commands.

These are deliberately separate. A button should request a rounded rectangle and text; it should not create pipelines, descriptors or Vulkan command buffers.

### 3.4 Resource ownership

GPU resources will use explicit handles and well-defined ownership.

The design must account for:

* resource creation and destruction;
* resources still being used by queued GPU work;
* deferred destruction;
* device loss;
* surface destruction;
* swapchain recreation;
* multiple top-level windows.

### 3.5 One renderer, multiple surfaces

The expected model is:

```text
GpuInstance
    └── GpuDevice
          ├── GpuSurface — window 1
          ├── GpuSurface — window 2
          └── OffscreenRenderTarget
```

There should normally be one selected GPU device for the application and one surface/swapchain for each GPU-rendered top-level window.

Ordinary child controls do not receive separate swapchains.

### 3.6 Capability-based features

Backends expose capabilities rather than pretending every API and GPU behaves identically.

Examples include:

* timestamp queries;
* compute support;
* texture formats;
* maximum texture size;
* multisampling;
* storage images;
* descriptor limits;
* optional Vulkan 1.4 features.

Higher layers must test capabilities before using optional functionality.

### 3.7 Predictable shaders

Shaders will be compiled offline during development or packaging.

The initial path will be:

```text
Shader source
    ↓
Offline compiler
    ↓
SPIR-V and reflection metadata
    ↓
Packaged shader assets
```

Runtime shader compilation is not part of the normal rendering path.

The first implementation may use GLSL and `glslc`. Slang may be evaluated later, but it must not become an early dependency before the basic architecture is proven.

### 3.8 Correct colour and alpha handling

The renderer must define and test:

* premultiplied alpha;
* linear versus sRGB operations;
* texture colour-space metadata;
* blending behaviour;
* transparent window and layer behaviour.

These rules must not be left to accidental backend defaults.

---

## 4. Repository structure

The anticipated package structure is:

```text
README.md
docs/
    PROJECT_PLAN.md
    ARCHITECTURE.md
    ROADMAP.md
    decisions/
    tasks/

RenderCore/
RenderCanvas/
RenderSoftware/
RenderRhi/
RenderNull/
RenderVulkan/
Render2D/
RenderUpp/
RenderShaders/

examples/
    DisplayListDemo/
    VulkanSurfaceDemo/
    Render2DDemo/
    UppRenderDemo/

tests/
    RenderCanvasTest/
    RenderRhiTest/
    RenderVulkanTest/
    Render2DGoldenTest/

thirdparty/
```

Packages will be added only when their stage begins. Empty speculative packages should not be generated merely to make the repository look busy.

---

## 5. Development stages

## Stage 1 — Rendering foundation

This stage contains no Vulkan implementation.

Its purpose is to prove the high-level architecture before committing to a native backend.

Deliverables:

* `RenderCore`;
* `RenderCanvas`;
* `RenderSoftware`;
* immutable `UiDisplayList`;
* recording `UiCanvas`;
* deterministic display-list dump;
* software reference replay;
* unit tests;
* small visual demonstration;
* initial architecture documentation.

Exit criteria:

* display lists record and replay correctly;
* state, clipping and transforms are tested;
* the software reference image is stable;
* no native graphics API leaks into the interfaces;
* all packages compile under CLANGx64.

## Stage 2 — RHI contracts and Null backend

Define the narrow GPU abstraction without implementing Vulkan details in higher layers.

Initial concepts:

* adapter;
* device;
* queue;
* surface;
* swapchain;
* buffer;
* texture;
* sampler;
* shader;
* graphics pipeline;
* compute pipeline;
* command encoder;
* render pass;
* resource upload;
* synchronization token;
* capabilities.

`RenderNull` will validate command ordering and resource lifetime without requiring a GPU.

Exit criteria:

* invalid resource usage is detected;
* command sequences can be tested headlessly;
* no Vulkan types appear in RHI-facing clients;
* the RHI remains intentionally smaller than Vulkan.

## Stage 3 — Vulkan backend bootstrap

This is the first GPU implementation stage.

Deliverables:

* Vulkan instance and validation setup;
* physical-device enumeration and selection;
* logical device and queue creation;
* Windows surface creation;
* swapchain creation and recreation;
* resize, minimise and restoration handling;
* frames in flight;
* command pools and command buffers;
* resource upload path;
* deferred resource destruction;
* Dynamic Rendering;
* Synchronization2;
* debug names and debug markers;
* pipeline cache support;
* GPU timestamps where available;
* clean shutdown;
* basic offscreen target;
* validation-clean clear, rectangle and textured-quad demonstrations.

Rules:

* no `vkDeviceWaitIdle()` during ordinary frame rendering;
* no per-frame recreation of stable resources;
* no CPU framebuffer readback for normal presentation;
* swapchain out-of-date and surface-lost states must be handled;
* validation-layer errors are release blockers during development.

## Stage 4 — GPU 2D renderer

Implement `UiRenderer2D` over `GpuRhi`.

Initial functionality:

* solid rectangles;
* borders;
* rounded rectangles;
* clipping rectangles;
* transforms;
* opacity;
* images;
* batching;
* texture atlases;
* offscreen layers;
* premultiplied-alpha compositing.

The first target is a reliable UI renderer, not a general 3D engine.

## Stage 5 — Text and vector rendering

Add:

* font shaping integration;
* glyph caching;
* grayscale glyph atlas;
* subpixel positioning where appropriate;
* vector paths;
* fills and strokes;
* gradients;
* anti-aliasing;
* icons and SVG-derived geometry.

Software and GPU outputs should be compared using controlled golden-image tolerances.

## Stage 6 — U++ integration

Add the bridge to existing U++ painting.

Expected components:

```text
UiPaintContext
LegacyDrawRecorder
RenderUppSurface
GpuTopWindow integration
Software fallback
```

The integration must preserve:

* control hierarchy;
* layout;
* focus;
* input;
* invalidation;
* themes;
* existing software rendering;
* printing.

Migration will begin with selected controls and demonstrations rather than converting the entire U++ control library at once.

## Stage 7 — Effects, compute and specialised views

After the UI core is stable:

* shadows and blur;
* image-processing passes;
* colour transforms;
* playback surfaces;
* waveform and scope rendering;
* custom GPU views;
* optional compute operations.

Compute is for operations that benefit from it. It should not be used simply because the GPU has a compute queue and looks lonely.

## Stage 8 — Hardening and additional backends

Hardening includes:

* device-loss recovery;
* memory-pressure behaviour;
* multi-window testing;
* high-DPI testing;
* Intel, AMD and NVIDIA testing;
* performance regression tests;
* Linux Vulkan surface support;
* packaging and deployment documentation.

Later backends may include:

* Metal;
* WebGPU;
* OpenGL fallback.

They must implement the established RHI rather than altering the public rendering model around their individual preferences.

---

## 6. Testing strategy

The project will use several types of tests.

### Unit tests

Cover:

* display-list ordering;
* state-stack balance;
* clipping;
* transforms;
* resource-handle validation;
* capability handling;
* deterministic output;
* invalid command sequences.

### Reference and golden tests

The software renderer provides a reference output.

Golden tests will cover:

* geometry;
* alpha blending;
* rounded corners;
* text;
* clipping;
* gradients;
* images;
* shadows.

GPU comparisons will use a documented tolerance where exact pixel equivalence is unrealistic.

### Vulkan validation

Development builds will use Vulkan validation layers.

A test is not considered successful merely because a window appeared. Validation errors, incorrect lifetime handling and synchronization faults must be resolved.

### Behaviour tests

Test at minimum:

* initial window creation;
* resize;
* minimise and restore;
* maximise;
* repeated open and close;
* multiple windows;
* DPI changes;
* swapchain recreation;
* unavailable optional capabilities;
* clean application shutdown.

### Performance baselines

Before broad integration, record:

* frame CPU time;
* GPU time where supported;
* draw and batch counts;
* uploaded bytes;
* transient allocations;
* texture-atlas use;
* resource creation counts.

Performance work should be based on measurements rather than graphics folklore.

---

## 7. Dependency policy

Third-party dependencies must be:

* necessary;
* actively maintained;
* licence-compatible;
* pinned to a known version;
* documented;
* isolated from public project APIs.

Likely Vulkan-stage dependencies may include:

* Vulkan headers and loader;
* an approved Vulkan memory allocator;
* an offline shader compiler;
* validation layers for development.

No dependency is added during Stage 1 unless it is required for the software reference implementation.

---

## 8. Deliberate exclusions from the first stages

The initial project is not intended to become:

* a full 3D game engine;
* a scene-graph replacement;
* a ray-tracing framework;
* a general render graph;
* a Vulkan tutorial collection;
* a wrapper exposing every Vulkan function;
* an immediate rewrite of every U++ control.

Those areas may be revisited only when the 2D rendering and U++ integration foundations are stable.

---

## 9. Contribution and publication workflow

Each implementation task will contain:

* task identifier;
* objective;
* allowed files;
* expected interfaces;
* exclusions;
* build requirements;
* tests;
* acceptance criteria;
* reporting requirements.

Gary must work on a local task branch:

```text
task/NNN-short-description
```

Gary must not:

* push to GitHub;
* alter the published `main` branch;
* merge branches;
* create tags or releases;
* close tasks;
* modify another repository unless explicitly instructed.

At task completion Gary supplies:

* local branch name;
* local commit hash or hashes;
* complete patch against `main`;
* list of changed files;
* build commands used;
* test results;
* screenshots where applicable;
* known limitations;
* questions or recommended follow-up work.

The supervisor reviews the result. The manager alone decides whether and how it is published.

---

## 10. Definition of done

A task is complete only when:

* its stated scope is implemented;
* affected packages build under CLANGx64;
* required tests pass;
* no unapproved dependencies were introduced;
* no native backend types leaked across architecture boundaries;
* documentation matches the implementation;
* temporary files and generated build output are excluded;
* validation results are reported honestly;
* the completion report is supplied;
* the supervisor accepts the work;
* the manager approves publication.
