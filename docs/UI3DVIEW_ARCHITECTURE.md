# Ui3DView architecture and implementation contract

Date: 1 November 2026. Implementation work authorized by the owner after the
planning-only R1 handoff. This document governs the new viewport work; existing
whole-UI qualification and gallery changes remain separate.

## Goal

A reusable native U++ control for a real depth-tested Vulkan scene, usable in an
ordinary layout. The application owns its document, selection, playback clock,
undo, asset import and evaluated animation. The viewport owns camera navigation,
input capture and presentation through the existing GpuCtrl. It does not create
a second windowing system, timeline, rig authority or application framework.

## Package boundary

| Package | Owns | Must not own |
| --- | --- | --- |
| RenderScene | Validated immutable meshes and scene snapshots, transforms, bounds, camera/projection and CPU ray/triangle picking | Ctrl, native windows, GPU resources, app documents |
| Ui3DView | Public control API, per-view camera/options/selection projection, navigation events, retained GPU content | Source assets, app clock, undo, import parsing |
| GpuRender | Existing native host, shared compatible context, surface/swapchain, presentation and explicit retry | Scene semantics and object selection |
| RenderRhi | Neutral 3D vertex/index/parameter/depth/culling contracts and capability/error reporting | Vulkan handles in public view APIs |
| RenderVulkan | Implemented graphics/depth resources, synchronization and cleanup | Application or control state |

RenderScene uses Core and explicit immutable shared ownership. Mutable descriptions
are validated into a candidate before publication; invalid candidates never replace
a usable scene. A mesh is created through a factory with copied owned arrays, so
later changes to caller vectors cannot change a published mesh. Shared const mesh
references permit reuse between snapshots without copying geometry per camera move.

## Coordinates and numeric contract

Canonical world coordinates are metres, right handed, Y up. +Z is the semantic
forward direction of source assets; camera forward points from its eye to target.
Matrices use column-major storage and column vectors: world = parent * local;
clip = projection * view * world. Angles are radians. CPU geometry/picking uses doubles, while GPU vertices and
parameters use float32. Coordinate validation bounds do not promise precision at
large magnitudes or interactive performance at maximum budgets. Keep geometry
local and use transforms for placement; large-world rebasing remains later work.
Camera projection has Vulkan
depth 0..1 and upper-left screen origin. The projection includes the required Y
orientation once; input and picking use the same convention. Positive nonsingular
affine transforms are supported initially; reflections, singular matrices and
nonfinite values are rejected explicitly. Normals use inverse transpose and
normalization, including positive nonuniform scale.

Stable object IDs are nonempty strings in an application namespace, for example
character:alice or prop:bench. The snapshot validates uniqueness, parent existence,
acyclic hierarchy, indices, resource budgets and transforms. Object disappearance
does not transfer selection authority to the viewport.

## Camera, input and requests

Per-control camera state supports orbit around target, pan, dolly, perspective /
orthographic projection and fit bounds. No global current viewport exists.
Fit expands existing clip planes outward only as required; it does not tighten the
default useful navigation range. Dolly preserves authored near/far planes. A future
automatic clipping mode would be an explicit policy rather than silent camera edits.
Default navigation is middle-drag orbit, Shift+middle-drag pan, wheel dolly;
Alt+left/middle/right offer Maya-style orbit/pan/dolly. Left click issues a
selection request. F fits selection (or the scene), Home fits the scene.

A drag has idle / begin / update / commit / cancel transitions and a saved starting
camera. Escape, capture/focus loss, disable, hide, close and rebind cancel exactly
once. Cancel restores the starting camera while retaining a monotonic camera
revision. Programmatic setters do not fabricate user camera or selection events.
Callbacks receive value payloads and a copied callback. Release capture and complete
state transitions before dispatch. Any access after application callbacks must
check a destruction-aware Ptr because a callback can destroy or rebind the view.
U++ still reads its captured Ctrl after CancelMode returns, so capture-loss
notification is copied and deferred through an owned one-shot callback. A newer
semantic input flushes that pending cancellation first. Destruction kills pending
callbacks and releases capture without calling the application. Events retain the
scene revision and binding identity from the start of their gesture.

The application receives a selection request with hit/miss, stable ID, world
point/normal and scene/camera revisions; it accepts by calling SetSelection.
Ui3DViewPick also records a per-view scene-binding revision and sampled viewport
size, with an opaque weak reference to the originating view state. IsCurrentPick
checks all of these, including misses, so reusing an app revision/ID, resizing or
passing a pick between views cannot accidentally apply a stale selection. Reopening
the same view/device does not invalidate unchanged CPU geometry by itself.
Selection IDs deliberately survive rebind; the app must republish or clear its
selection when switching documents, especially when IDs are reused. FitSelection
uses each selected object's own mesh bounds; a transform-only parent does not
implicitly frame descendants. Inherited GpuCtrl::SetGpuContent is reserved for the
3D renderer; inherited 2D build/paint hooks are bypassed by that custom content.

CPU triangle picking is geometric picking of supported opaque visible objects;
it is not physics/contact, a GPU ID pass or material-alpha evaluation.
Nonselectable objects are skipped as occluders, and wireframe still picks solid
triangle geometry. Mesh/object RGB is linear in 0..1; decoration palette bytes are
sRGB. Both material and vertex alpha must be 1 in this opaque initial path.

The full native child covers the control. Input must be forwarded by the existing
GpuCtrl host to the owning Ctrl using current U++ event semantics and verified by
a real native input test, not just direct method calls. The native DHCtrl HWND
stays disabled as required by U++'s top-level event routing; semantic enabled state
belongs to the Ctrl tree. Host::MouseEvent forwards once to its owning control.

## Renderer additions and compatibility

The first real mesh path needs a complete vertical slice:

- Position3 / normal3 / UV2 / color4 float vertex data and indexed draw.
- Bounded graphics parameters for camera and object data, with explicit size and
  alignment validation. Existing compute parameters remain independent.
- Supported depth format/capability and depth-attachment usage, allocation/view,
  render-pass attachment and clear/load/store behavior.
- Depth test/write/compare, front-face winding and culling in pipeline identity.
- Correct attachment transitions, pass/pipeline compatibility, buffer validation,
  and resource cleanup on failed creation, resize and host Close.
- Default descriptor values preserve existing color-only 2D callers. Unsupported
  providers reject unsupported 3D work; they must not report successful depth.
- Explicit pass viewport orientation: legacy color-only callers keep
  flip_viewport_y=true; the mesh pass sets false because its projection already
  applies the Vulkan Y orientation. Depth attachment presence does not change
  coordinate conventions implicitly.

The detailed implementation and supported limits are recorded alongside the
public headers. No raw Vulkan handles enter RenderScene or the Ui3DView API.
D32F supports attachment-only usage, with capability flags restricted accordingly;
sampling, readback and stencil are not implemented. Graphics push parameters are
0 or 4..128 bytes, a multiple of four, and must be set completely after pipeline
changes. Index buffers are host-visible, non-storage, at most 64 MiB, with one
complete initial upload followed by permitted partial writes. Up to 64 validated
range maxima per buffer avoid scanning retained indices on every camera frame.
A failed native upload invalidates that CPU mirror because a failed flush may
follow a successful mapped-memory copy. Unsubmitted commands can be discarded;
uncertain submitted work cannot be discarded or replayed.

## Retention and performance

Camera movement updates camera parameters and requests an invalidated frame.
It does not revalidate scene hierarchy, parse assets or upload unchanged meshes.
Keep vertex/index storage and shader/pipeline resources retained; reuse grid and
axis geometry. Selection highlighting changes per-draw appearance, not mesh data.
GPU-owned depth storage changes only with physical target dimensions. All view
resources close while GpuDevice is alive; CPU snapshots remain available for
explicit retry/reopen. No anonymous global graphics cache or permanent idle timer.

Embedded GpuCtrl currently presents synchronously. This implementation does not
claim an asynchronous GPU worker, independent queues or guaranteed frame rates.
Stats distinguish uploads/frames from timing, and CPU time is not GPU timestamp
time. Real dynamic deformation and measured GPU skinning are later work.

## Initial test gates

1. Independent expected-value camera/project/ray tests; perspective/orthographic,
   hierarchy, normal transforms, invalid inputs and immutable snapshot lifetime.
2. Backend indexed/depth tests with intersecting geometry in both draw orders,
   known pixel expectations, near/far and invalid state/range validation.
3. Native control navigation, click selection, cancellation, rebind, callback
   destruction, disabled state, resize, hide/show, close/reopen, and two views.
4. Retained uploads during camera-only changes and bounded resource lifecycle.
5. Existing color-only RHI and Vulkan graphics regression after shared additions.
6. Debug and Release builds, including ordinary non-BLITZ/header coverage where
   practical. New regression executables report a nonzero check count and fail on
   failure; existing regressions retain their established reporting.

GPU checks are finite functional checks, serialized through Klick. The handoff
records a prior host blackout; this tranche does not introduce a sustained stress
loop or automatic retry. Visual screenshots and automated input checks are distinct
from the owner's physical mouse/keyboard and monitor-DPI acceptance.

## Demo and evidence

Source: examples/Ui3DViewDemo. Stable requested deliverable:
bin/win64/Ui3DViewDemo.exe.
Compiler caches and test executables remain under build. Runtime checks use the
owner-approved build output scope; the exact tested executable bytes are copied
to the stable release path and hashed again. Keep the current Ui gallery and its
release executable intact.

The demo shows overlapping objects, ground/grid/axes, perspective/orthographic,
fit, selection, Light/Dark appearance and live upload/frame diagnostics. A bounded
self-test and capture mode closes itself; normal interactive mode has no animation
clock. The application demo, not the reusable control, owns toolbars and inspector.

## Reviewed precedents

SculptorTools at d46e0ebaaf3d5d11a570ea06a34913b3e625b823:
[SculptorCtrl](https://github.com/ismail-yilmaz/sculptor-tools/tree/d46e0ebaaf3d5d11a570ea06a34913b3e625b823/SculptorCtrl).
Useful: Ctrl/native-host separation, retained model buffers, inverse-transpose
normals. Avoid adopting ambient global viewport/shader maps, per-overlay buffer
creation, repeated uniform-name lookups and public mutable geometry/dirty flags.
Its example navigation is not a complete reusable cancellation contract.

Blender at 613a6eb8f8cb4778e87c31b755ecb3e5e1bcad52:
[navigation state](https://github.com/blender/blender/blob/613a6eb8f8cb4778e87c31b755ecb3e5e1bcad52/source/blender/editors/space_view3d/view3d_navigate.hh).
Useful: initial immutable gesture state, current working state and explicit
apply/confirm/cancel with restoration. This design is independently implemented;
no Sculptor or Blender source was copied or adopted.

## Explicit later work

Evaluated animated/deformed mesh publication, transform gizmos and document-edit
transactions, acceleration structures for very large scenes, transparency/material
textures, aligned depth/normal/ID export, general import adapters, collision/contact,
GPU skinning and additional backends require separately qualified extensions.
A clean first control is not a claim that these future capabilities already exist.
