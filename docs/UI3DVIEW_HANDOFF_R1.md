# Ui3DView programmer handoff R1

Status: planning and source audit, 1 November 2026. No implementation, build,
GPU job or inference was run. Names below describe proposed behavior, not
existing public APIs. This is a starting contract for programmer review.

## Purpose and scope

Provide a reusable renderer-owned 3D viewport for ordinary U++ layouts. The
suggested name and location are `Ui3DView` and `render/Ui3DView`: they fit
the existing renderer package organization. That directory is a planning
scaffold, not a package. A separate neutral data package is an implementation
decision if dependencies justify one.

Motion Studio v3 uses one Stage, one app-owned transport/clock, one optional
UiTimeline and one collapsible Inspector with one shared scene tree. A viewport
is only the Stage rendering and interaction boundary; app shell, tree, jobs,
document, playback, selection authority and undo remain with the application.
The supplied image and HTML are illustrative interaction design: their
procedural motion and imported bounding proxies are not real animated mesh,
inference, production import, collision or physics capabilities.

## Inspected repository facts

| Surface | Evidence and limitation |
| --- | --- |
| Host | [GpuCtrl.h](../render/GpuRender/GpuCtrl.h) exposes `SetGpuContent(unique_ptr<GpuSurfaceContent>)`, selected before opening; it bypasses the painter and frame-builder paths. `RequestGpuRefresh()` invalidates; `SetContinuousRefresh(0)` is invalidation-only, 1–240 requests bounded host refresh. `RetryGpuInit()` is explicit. |
| Content lifetime | [RenderPresentation.h](../render/GpuRender/RenderPresentation.h) defines `Render(GpuDevice&, const GpuFrameInfo&, String&)` and `Close(GpuDevice&)`. Render completes its commands without presenting, closing or re-entering the owner; Close occurs while the device is alive, and content can later render on a new device. [RenderPresentation.cpp](../render/GpuRender/RenderPresentation.cpp) closes content before the session. Keep the existing surface/swapchain owner. |
| Context | [GPU_CTRL_USAGE.md](GPU_CTRL_USAGE.md) describes shared compatible device context but separate presenter surface, swapchain and frame state. Embedded GpuCtrl presentation is synchronous; this does not guarantee independent GPU queues or frame rate. |
| RHI | [RenderRhi.h](../render/RenderRhi/RenderRhi.h) has buffers, textures, shader/pipeline and commands, plus optional compute. Public vertex layouts are only Position2Color4F and Position2Uv2Color4F. The render pass has a color target only; pipeline has no depth-test/write/compare state; texture usage exposes no depth-attachment usage. D24S8 as a format enum is not a depth path. |
| Further RHI gaps | Index buffer and uniform usage flags exist, but the public graphics commands expose vertex binding and non-indexed Draw only, with no index binding/indexed draw or general graphics uniform binding. Pipeline descriptions have no public culling/front-face configuration. Define the necessary full path and backend capabilities, including validation and cleanup, rather than assuming flags imply support. |
| Qualification | [ACTIVE_WORK.md](ACTIVE_WORK.md) records Windows/Vulkan UI checkpoints and outstanding acceptance separately. Those results do not establish a depth-tested 3D mesh, animated skinning, picking or viewport lifecycle. |

These findings are source inspection, not a new runtime qualification. The
existing UI/refactoring work and release artifacts remain owned by their current
workstreams. Any shared RenderRhi/RenderVulkan changes require explicit path
ownership before implementation.

## Proposed neutral seam

The following concepts are behavioral requirements; exact C++ types, ownership
primitives and API names must be reviewed against current U++ and renderer code.

- **Scene snapshot:** monotonic scene revision; stable, namespace-safe actor and
  node IDs; parent IDs, local transforms, bounds, visibility/display flags,
  material descriptors and immutable shared or owned mesh resources. A snapshot
  and everything it references outlive every in-flight frame. Validate hierarchy,
  resource sizes and invalid transforms before replacing a usable snapshot.
- **Coordinates:** adapter converts source units/axes into a documented canonical
  convention: metres, Y up, +Z forward. Specify handedness, matrix storage and
  multiplication order, view/projection and clip-depth convention, winding,
  origin, normal transforms and conversion back for edits before implementation.
  Do not infer these from the prototype; test round trips, nonuniform scale and
  parent transforms. Source geometry and source units remain retained by the app.
- **Camera:** revisioned perspective or orthographic state, target/orientation,
  orbit/pan/zoom and fit-to-selection, near/far, and logical viewport dimensions
  plus DPI/physical target size. Reject invalid projection ranges. App may persist
  camera state; the control owns navigation interaction.
- **Pose:** stable actor/profile/bind identities, exact source time with tick rate,
  revision, and evaluated joint transforms or an owned CPU-deformed mesh. The app
  samples once and supplies the snapshot; the viewport has no animation clock.
  A 30000-tick/second application timebase is a proposal, not a renderer rule;
  use checked conversion for rates it cannot represent.
- **Selection and picking:** app supplies authoritative stable IDs, primary and
  hover state; visibility does not delete objects. Hit results report target ID,
  world point/normal and depth where actually available, along with scene, pose
  and camera revisions; define miss, unsupported, stale and multi-hit behavior.
  Return a selection *request*, never silently mutate document authority.
- **Gesture requests:** begin/update/commit/cancel carry stable target IDs,
  transaction/gesture identity, starting revisions and local/world coordinate
  convention. Preview updates are reversible; app validates and owns undo/redo.
  Define rejection and rebind behavior. Escape, capture/focus loss, destruction,
  target removal and device failure cancel an active gesture exactly once. Copy
  callback payloads before invoking application code, which may synchronously
  rebind or destroy the control.
- **Render options/capabilities:** proxy, skeleton or both, grid/axes,
  shaded/wireframe, highlight and path/pose/contact overlays are requests;
  report actual supported features and errors. An unsupported overlay or backend
  must not be presented as successfully rendered.
- **Capture:** return dimensions, color format/space, origin, scene/pose/camera
  revisions and completion/error status. Later aligned depth, normals and actor
  IDs require explicit units, coordinate spaces, background/invalid values,
  occlusion, row layout and identity mapping. Define bounded readback and
  cancellation; an image alone is not a live document identity.

### Ownership and lifecycle

The application owns document state, selection, clock, undo, source assets and
edits. The control owns input capture, navigation and render-host lifetime.
The backend owns device resources and releases them through the host's
`Close` before device teardown. Keep retained CPU data and snapshots valid
across in-flight work; never retain borrowed transient vectors. Bound upload,
pending-frame and readback work, discard stale revisions deliberately, and
keep parsing, I/O, skinning and large evaluations off Paint and responsive GUI
paths. Define threading and publication/acknowledgement rules before coding.

Resize, zero-size, hide/show, reopen, close, callback destruction, focus/capture
loss, failed initialization and device loss need deterministic transitions.
Report loss, stop/cancel outstanding work safely, invalidate device handles and
recreate from retained snapshots on an explicit recovery path. Existing
Close/new-device behavior is a useful host seam; it does not by itself prove
complete device-loss recovery. No automatic retry or GPU stress is authorized
in this tranche.

A CPU Stage backend can implement this **same** scene/camera/selection/gesture
contract, with its capabilities clearly reported. Existing CineRig CPU
triangle/depth/skinning and Rig motion code in `E:/apps/upp_3rdparty/Rig`
are external integration candidates, not files to copy or migrate here.
Application-specific adapters remain outside this reusable package.

## Ordered milestones and acceptance

**V0 — Contract and standalone review harness.** Agree conventions, ownership,
callback and error semantics and a minimal backend seam. Only then add real
packages and a runnable harness: box, ground and stable actor/node selection;
orbit/pan/zoom, fit, resize/DPI, Light/Dark, focus/capture/cancel and lifecycle.
A labelled CPU backend is acceptable. Verify independent coordinate/ray tests,
snapshot lifetime, callback destruction and rejected/cancelled gestures. This
milestone does not claim Vulkan mesh support.

**V1 — Vulkan mesh and depth baseline.** Extend the neutral RHI and Vulkan
implementation together: position3/normal/UV/color vertex input, needed index
binding/draw, camera/object graphics parameters, depth format and capability,
usage, attachment/load/store/clear, test/write/compare, and deliberate
winding/culling state. Include resource barriers, resize/recreation and cleanup;
retain existing 2D behavior and avoid raw Vulkan/GL handles in the control API.
Render intersecting cubes in both submission orders; verify near/far clipping,
winding, normals under nonuniform positive scale and occlusion against a small
independent CPU depth/ID oracle. Check bounded steady-state/resize resources and
hide/show/reopen/close. Future GPU tests must be coordinated, finite and safe
after the reported host blackout; no automatic stress or retries.

**V2 — Real animated actor.** Integrate externally evaluated retained motion and
CPU skinning, e.g. existing RigSkinBinding, through owned snapshots and a
dynamic vertex path. Verify deterministic seek/step and exact time mapping,
visibly nonstatic deformation, alignment and stable selection. GPU skinning is
a later measured optimization, not a prerequisite or another rig authority.

**V3 — Picking and editing.** Start with CPU ray/bounds/triangles and a bounded
acceleration cache. Test projection/world-ray round trips, overlap, occlusion
and stale revisions independently. Add move/rotate handles, XZ ground-plane
waypoint drag and object-relative target position/orientation; prove transaction,
undo, rejection, cancel and capture-loss behavior. Geometry remains visual
unless separately qualified as collision/contact input.

**V4 — Export parity and bounded imports.** Produce aligned color, metric depth,
normals and actor IDs with captured revisions; verify units, background,
occlusion and parity against the existing CPU export without replacing it.
Add one real import adapter at a time with retained source, format, units/up
axis and explicit material support. FBX skin/animation, glTF/USD, broad format
coverage and collision/physics are separate scoped work.

## Programmer TODOs before implementation

1. Re-read current source, repository guidance and uncommitted changes; secure
   path ownership for shared RHI/backend work. Review neutral model placement,
   U++ ownership, threading, lifetimes and error semantics with the owners.
2. Write convention examples and independent expected-value tests, including
   source-to-canonical transforms and world/screen ray round trips. Decide camera
   representation, stable ID scope, revision conflict and frame/capture semantics.
3. Specify the smallest RHI graphics/depth/index parameter additions, backend
   capability negotiation and compatibility impact on existing implementations.
   Define resource recreation, failure injection and finite validation gates.
4. Implement V0 first; keep CPU and eventual Vulkan implementations behind the
   same seam. Add actual package/harness and run appropriate focused tests only
   when implementation work and safe execution are authorized.
5. Integrate app adapters using existing Rig sources without editing/moving
   sibling source here. Keep one app clock and one UiTimeline. The bounded
   Motion Studio audit found no public seconds formatter/playhead seam; have the
   Timeline owner review a narrow extension, not a duplicate timeline.
6. Preserve existing release artifacts, working CPU export and unrelated edits.
   Track CPU fallback, Vulkan depth, animated mesh, picking and export as
   independently qualified capabilities.

Do not silently add an importer zoo, contact/physics solver, GPU skinning,
database dependency, generation stack or another document/timeline. No source
has been adopted from SculptorTools. The supplied prior audit identifies
[SculptorTools at d46e0ebaaf3d5d11a570ea06a34913b3e625b823](https://github.com/ismail-yilmaz/sculptor-tools/tree/d46e0ebaaf3d5d11a570ea06a34913b3e625b823):
Sculptor is CPU-oriented and SculptorCtrl uses GLEW/OpenGL. Camera/matrix/mesh
ideas may inform a reviewed design; GL resources, context and shaders are not
Vulkan by renaming. Verify provenance and preserve the applicable BSD-3-Clause
license and notices if any code is later reused.

## Design inputs

Owner's [Motion Studio v3 native plan](../../upp_cinespace/CineRig/docs/MOTION_STUDIO_V3_NATIVE_PLAN_R1.md)
and preserved `CineRig/designs/motion-studio-v3` HTML, handoff and stage image
are design data, not instructions overriding the owner. Their SHA-256 hashes
matched the plan during this audit. The HTML has simulated Generate and largely
labelled proxy imports; its visual design does not establish native features.
The native shell, timeline/tree and jobs remain with the Motion Studio workstream.
