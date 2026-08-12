# Project Plan

`upp_render` is being built in staged layers so the recording model can settle
before any GPU backend is bolted on with wishful thinking.

## Workflow Rules

- Curt is the project manager and publisher.
- Contributors implement assigned work only.
- Work currently happens directly on local `main`.
- No task branches unless Curt later changes the workflow.
- Contributors may commit locally.
- Contributors must not push, publish, tag, release or create pull requests.
- Completion reports should include summary, files changed, validation,
  remaining issues and the recommended next step.

## Architecture Rules

- no Vulkan, Metal, WebGPU or OpenGL types in public Stage 1/2 APIs
- `UiCanvas` records drawing intent
- `GpuRhi` is the lower-level GPU contract
- display lists are immutable after build
- software replay remains the correctness reference

## Stage 1 - Backend-Neutral Foundation

Done:

- `RenderCore` value types
- `UiCanvas` recording API
- immutable display lists
- deterministic dumps and inspection
- software replay
- unit tests and a visual demo

## Stage 2 - RHI Contract and Null Backend

Define `GpuRhi` as the minimal GPU-facing contract and implement `RenderNull`
for headless validation of command ordering, lifetime rules, and state handling.

- TASK-002 completed.
- TASK-003 completed.
- TASK-004 completed.
- TASK-004A completed.

## Stage 3 - Vulkan Bootstrap

Add the first GPU backend:

- instance and validation setup
- device and queue selection
- surface and swapchain management
- frame submission and synchronization
- resource upload and deferred destruction

TASK-005 and TASK-005A are complete.
TASK-006 and TASK-006A are complete: Vulkan loader, instance, physical-device
selection, logical-device creation, and graphics-queue bootstrap.
It uses Vulkan 1.3 as the baseline, loads the runtime through `vulkan-1.dll`,
and keeps the SDK header path local via a build method with `INCLUDE` extended
by `%VULKAN_SDK%\Include`.

Surface/platform bring-up, grouped ownership, private swapchain lifecycle,
explicit frame acquisition/presentation, and the first visible clear frame are
accepted. The active closure work is converging those accepted paths behind the
neutral `GpuDevice` contract rather than adding more bootstrap-only APIs.

TASK-007 completed the surface and platform bridge layer, with a Win32
native-window contract, bridge test coverage, a live Vulkan surface probe, and
a ten-cycle validation gate that now passes.

TASK-007A3 restored the backend-neutral `GpuCtrl` public boundary.
TASK-007A4 completed the control/session foundation, removing duplicate
surface bring-up code, documenting the usage and future UI rendering shape, and
adding practical embedded-control demos.

S17A added explicit neutral buffer/texture upload operations and made
`RenderNull` authoritative for upload-range and texture-layout validation.
S17B adds the first production `VulkanGpuDevice` slice by borrowing the accepted
live `VulkanSurfaceSession` device/queue ownership and providing real Vulkan
buffer allocation/writes plus optimal-image texture allocation and staging
uploads. Command/pipeline/draw and neutral surface/swapchain/frame bridging
remain the final Stage 3 convergence work.

Successful local build command:

```text
<upp-root>\umk.exe render,examples,tests,tools,<upp-root>\uppsrc VulkanProbe <local-vulkan-build-method> --out-dir build
```

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

`GpuCtrl` is the intended application-facing boundary for future embedded GPU
content.

## Stage 7 - Effects, Compute, and Specialized Views

Add the follow-on pieces that depend on the earlier layers:

- effects and layers
- compute-backed helpers
- offscreen and specialized views
- broader GPU capability plumbing

Compute is an architectural consideration here, not an implementation yet.

## Stage 8 - Hardening and Future Backends

Stabilize the stack, compare software and GPU outputs, and then evaluate any
additional backends beyond the first ones already planned.

## Current Status

- TASK-007 surface bring-up passes the ten-cycle validation gate
- TASK-007A4 control/session cleanup is accepted and no longer the active focus
- TASK-008A1 S9 private shared-instance registry is accepted
- TASK-008A1 S10 private RAII lease is accepted after the release-build
  destructor fix
- TASK-008A1 S11 grouped surface-session integration is accepted
- TASK-008A1 S12 private Vulkan swapchain ownership is accepted
- TASK-008A1 S13 explicit Vulkan frame acquisition and presentation is accepted
- TASK-008A1 S14 first visible Vulkan clear-colour frame is accepted
- TASK-008A1 S15 GpuCtrl Vulkan presentation integration is accepted
- grouped sessions share runtime and instance state while logical devices,
  queues, surfaces, swapchains, and frame state remain owned per session
- ordinary GpuCtrl instances remain isolated through their default session groups
- S14 uses a temporary dynamic-rendering clear submission and deliberately keeps
  the accepted S13 present-only implementation unchanged
- S15 drives private swapchain recreation and S14 clear presentation from native
  paint invalidation without adding a timer/render loop or Vulkan public API
- S16A adds the first backend-private filled rectangle through the real GpuCtrl
  presentation lifecycle; S16B moves its background/rectangle description into
  backend-neutral private frame intent so the Vulkan backend no longer invents
  control content or geometry
- S16C records the orange rectangle as one existing UiDisplayList FillRect and
  replays that neutral operation into the private frame intent
- S16D extends the FillRect-only replay proof to ordered operations and carries
  two fills through one Vulkan dynamic-rendering frame
- S16E adds persistent ClipRect replay above the Vulkan boundary: clips intersect
  cumulatively and affect only later FillRects
- S16F adds Save/Restore scoping for that private replay state so ClipRect state
  can be restored deterministically
- S16G adds translation-only ConcatTransform replay for FillRect geometry, scoped by
  Save/Restore; scale, rotation, shear and general renderer state remain deferred
- S17A aligns the neutral GpuDevice contract with explicit buffer and texture upload
  operations and makes RenderNull the validation authority for upload range/layout rules
- S17B adds a production VulkanGpuDevice resource slice using the accepted surface-session
  device ownership: real buffer allocation/write/destruction and optimal-image texture
  allocation/staging upload/destruction, while command/pipeline/surface RHI methods remain
  explicitly Unsupported until the next convergence slice
- general 2D rendering, shaders, painter callbacks, and shared control device
  ownership remain deferred
- GPU-backed U++ and upp_Ui rendering is a future stage
- compute remains an architectural topic only
