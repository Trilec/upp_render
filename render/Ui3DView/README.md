# Ui3DView

A reusable native U++ viewport using the existing GpuCtrl Vulkan presentation
owner. It renders retained indexed meshes with depth, provides per-view camera
navigation and geometric picking, and leaves the document, selection decisions,
undo, imports and playback clock with the application.

Architecture: [UI3DVIEW_ARCHITECTURE.md](../../docs/UI3DVIEW_ARCHITECTURE.md).
Build/run evidence: [UI3DVIEW_VALIDATION.md](../../docs/UI3DVIEW_VALIDATION.md).
The original [R1 handoff](../../docs/UI3DVIEW_HANDOFF_R1.md) remains historical;
the owner subsequently authorized implementation and necessary renderer additions.

## Packages

| Package | Responsibility |
| --- | --- |
| RenderScene | Immutable scene/mesh values, validation, transforms, camera math, CPU picking; depends on Core only |
| Ui3DView | Native control, camera gestures, selection requests, view options and retained GPU resources |
| GpuRender | Existing window, compatible context and surface/swapchain ownership |
| RenderRhi / RenderVulkan | Indexed graphics, bounded parameters, depth, culling, submission and cleanup |

Use Ui3DView in your application's `.upp` dependencies and include
`<Ui3DView/Ui3DView.h>`. There are no Vulkan handles in the control or scene API.

## Publish a scene

Build and validate a candidate before replacing the active scene. Passing a null
snapshot to SetScene is an explicit clear operation; do not pass a failed factory
result directly into it.

```cpp
Ui3DView view;
Add(view.SizePos());

String error;
auto cube = Scene3DMesh::MakeBox(Scene3DVec3(2, 2, 2), error);
if(cube) {
    Scene3DSceneDesc description;
    description.revision = 1;

    Scene3DObjectDesc object;
    object.id = "document:cube";
    object.mesh = cube;
    object.local = Scene3DMatrix::Translation(Scene3DVec3(0, 1, 0));
    object.r = 0.15f; object.g = 0.45f; object.b = 0.75f;
    description.objects.push_back(object);

    auto candidate = Scene3DSnapshot::Create(description, error);
    if(candidate && view.SetScene(candidate, error))
        view.FitAll();
}
```

Factories own copies of the caller's arrays. Snapshots retain shared const meshes,
so a new evaluated scene can reuse unchanged geometry. All control methods and
callbacks run on the GUI thread; applications may prepare immutable candidates
elsewhere and publish them on that thread.

IDs are nonempty stable strings, unique within a snapshot. Parents may appear
after children. Factories reject missing parents, cycles, duplicate IDs, invalid
indices, degenerate triangles, nonfinite data, singular/reflected transforms,
unsupported transparency and exceeded budgets. A snapshot supports at most
100,000 objects, 2 million instanced vertices and 6 million instanced indices;
an individual mesh supports 1 million vertices and 3 million indices.

Coordinates are metres, right handed, Y up, with +Z as asset forward. Matrices are
column-major and multiply column vectors: world = parent * local. Camera angles
are radians. Vertex/object RGB values are linear in 0..1; alpha must be 1.
View palette bytes are sRGB and are converted independently.

CPU validation and picking use doubles; GPU vertices and draw parameters use
float32. Accepted coordinate bounds are validation limits, not an accuracy or
interactive-performance guarantee. Keep mesh coordinates local to their origin
and express placement through object transforms. Large coordinates with very
small features can lose GPU precision or collapse geometry while CPU picking
still resolves it; large-world rebasing and precision qualification are later work.

## Camera and selection API

The control keeps its own camera. SetCamera validates atomically, advances its
revision and requests a frame. FitAll and FitSelection frame visible scene bounds.
FitSelection uses the selected objects' own visible mesh bounds and falls back to
the entire scene when none remain. A transform-only parent does not implicitly
select or frame its descendants; the application can expand group selection.
Fit expands clipping ranges only when necessary; subsequent navigation preserves
authored near/far settings. Applications can set deliberate tighter clipping.

| Input | Action |
| --- | --- |
| Middle drag | Orbit |
| Shift + middle drag | Pan |
| Wheel | Dolly |
| Alt + left / middle / right drag | Orbit / pan / dolly |
| Left click | Selection request |
| F | Frame selection, or scene |
| Home | Frame scene |
| Escape | Cancel current navigation and restore its starting camera |

```cpp
view.WhenSelectionRequest = [&](const Ui3DSelectionRequest& request) {
    if(!view.IsCurrentPick(request.pick))
        return;

    std::vector<String> selection;
    if(request.pick.hit)
        selection.push_back(request.pick.id);

    String error;
    view.SetSelection(selection, error);
};
```

The application interprets `extend` and `toggle` and owns acceptance. SetSelection
validates IDs and duplicates atomically and does not emit a selection request.
A scene rebind retains the requested selection projection; IDs absent from the
new scene draw no highlight. Reused IDs in a different document can immediately
highlight that document's objects. Publish or clear the application's selection
when changing documents; stale-pick checks do not clear retained selection.

Pick returns hit/miss, stable ID, world point/normal, world distance, triangle
index and scene/camera revisions. Ui3DViewPick additionally records the scene
binding, viewport size and an opaque originating-view identity. IsCurrentPick
rejects stale results after document rebind, camera change, resize or use with
another control, including identical document revision numbers and IDs. A current
miss is valid and can clear selection. Closing/reopening GPU resources alone
does not invalidate unchanged CPU geometry.

Picking is two-sided CPU triangle intersection with object bounds as a broad
phase. Invisible and nonselectable objects are skipped; nonselectable objects do
not occlude selectable geometry behind them. Wireframe uses solid triangle hit
geometry. This is a documented selection policy, not a readback of rendered pixels.

WhenNavigation reports Begin/Update/Commit/Cancel with gesture identity, originating
scene revision/binding, starting camera revision and the current camera value.
A wheel or fit action emits one Commit. Setters do not fabricate input events;
replacing the scene or camera cancels an existing drag and can invoke application
code. A reentrant callback can supersede the setter; consume the event value and
check the setter's result. Escape, capture/focus loss,
hide, disable and close restore the drag's starting camera and cancel once.
Capture-loss notification is deferred until U++ finishes releasing capture and
is delivered before a newer semantic input. Callbacks may rebind or destroy the
control. Destruction suppresses pending callbacks and releases capture safely.

## Presentation and performance

SetOptions controls grid, axes, wireframe, lighting and explicit Light/Dark palette
snapshots. Hosts can map their theme roles into these values without changing a
global theme from the reusable control.

Unchanged immutable meshes share retained vertex/index/edge buffers within a view.
Camera movement changes only draw parameters. Grid/axis geometry is retained;
selection changes draw appearance without reuploading geometry. Depth allocation
changes with physical target size and is limited to 16 million pixels. GPU objects
close while the device is alive; CPU snapshots survive retry/reopen.

GetViewStats distinguishes frames, mesh uploads, uploaded bytes, depth rebuilds,
resident meshes and draw calls. GetViewError includes content and presenter
failures. Existing GpuCtrl retry/diagnostics remain available. There is no idle
animation clock. Application-driven animation publishes evaluated snapshots and
requests presentation through the existing invalidation path.

Ui3DView reserves the inherited `SetGpuContent` replacement seam for its own
retained 3D renderer. Applications must not replace that content or use inherited
2D build/paint callbacks to add 3D overlays; those callbacks are bypassed by the
custom content. Existing GpuCtrl validation, refresh, retry and diagnostics remain
usable. Keep application toolbars and inspectors as sibling controls.

The current GpuCtrl submission path is synchronous. This version does not claim
an asynchronous worker or a frame-rate guarantee. CPU triangle picking is bounded
but linear within candidate meshes; a BVH is a future extension for large scenes.

## Demo and test locations

Demo source: `examples/Ui3DViewDemo`.
Requested stable executable: `bin/win64/Ui3DViewDemo.exe`.

The demo has perspective/orthographic switching, fit, grid, axes, wireframe,
Light/Dark appearance, application-owned selection and resource counters.
Normal mode is interactive. `--smoke` performs a bounded readiness run;
`--capture` additionally writes dark/light/wireframe/orthographic client captures beside the
executable. `--validation` requests the installed Vulkan validation layer.

Tests are `RenderSceneTest`, `RenderVulkan3DTest` and `Ui3DViewTest`, alongside
the existing `RenderRhiTest` and `RenderVulkanGraphicsTest` regressions.
Ui3DViewTest uses a real native host and bounded mouse input injection; direct
handler checks for Escape and disabled input are identified separately.
See the validation document for actual completed runs, hashes and remaining checks.

## Current extension boundaries

Opaque untextured meshes and basic directional lighting are implemented. Texture
materials/transparency, gizmos and document edit transactions, accelerated picking,
mesh deformation/skinning, importer adapters and depth/normal/ID export are future
work. D32F is an attachment-only backend capability; it does not imply depth
sampling or readback. New indexed buffers are host-visible, non-storage and
limited to 64 MiB; the first upload must initialize the complete buffer before
checked draws. Unsupported backends reject the new paths explicitly.
