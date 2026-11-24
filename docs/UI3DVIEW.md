# Ui3DView — integration and maintenance guide

This is the single maintained guide for the 3D control, its optional native UI,
and its demo. Update this file when the API, behaviour or tested status changes;
keep detailed test output with build artifacts instead of adding handoff or qualification documents.

**Repository:** `E:\apps\github\upp_render`  
**Latest demo:** `E:\apps\github\upp_render\bin\win64\Ui3DViewDemo.exe`  
**Platform preparation checked:** 23 November 2026. Windows results and earlier qualifications are dated below.

[Quick start](#quick-start) · [API](#core-api) · [Events and selection](#events-and-selection) ·
[Native UI](#optional-native-ui) · [Gotchas](#gotchas-and-rendering-contracts) ·
[Build and run](#build-and-run) · [Tested status](#tested-status)

## Objective and boundaries

Provide a reusable native U++ control for a depth-tested Vulkan scene, with
orbit/pan/zoom, perspective and orthographic cameras, immutable textured materials,
material-aware geometric picking, immutable covariance Gaussian clouds, authored
camera roll/film gates, display modes and optional standard viewport controls. An application can embed one view
or several views of the same immutable scene.

| Layer | Responsibility and source |
| --- | --- |
| `RenderScene` | Immutable meshes/textures/Gaussian resources and material snapshots, hierarchy, transforms, bounds, camera mathematics and CPU picking. [Scene3D.h](../render/RenderScene/Scene3D.h); depends on `Core`, with no GUI or GPU ownership. |
| `Ui3DView` | One view's camera, options, selection highlight, navigation and retained GPU content. [Ui3DView.h](../render/Ui3DView/Ui3DView.h); usable without the `Ui` library. |
| `Ui3DViewUi` | Optional `Ui3DViewport` wrapper with toolbar, orientation/navigation controls and annotations. [Ui3DViewport.h](../render/Ui3DViewUi/Ui3DViewport.h). |
| Renderer | `GpuRender` owns native hosting/presentation; `RenderRhi` owns backend-neutral graphics contracts; `RenderVulkan` creates Win32 and Metal-layer surfaces through the same Vulkan renderer. Reuse these layers when extending the view. |
| Application | Document, selection decisions, saved cameras, undo, import, animation/playback clock, persistence, inspector and pane layout. The [demo](../examples/Ui3DViewDemo/) demonstrates this ownership. |

All control methods and callbacks run on the GUI thread. Immutable scene
candidates may be prepared elsewhere and published on that thread. U++ parenting
does not provide C++ deletion ownership: keep controls as members or in an owning
container, or give stack controls a scope that covers the entire window run.

## Quick start

Use `Ui3DViewport` when you want the standard native UI. Create an application
package in the Render assembly with this `.upp` file:

```text
uses
    CtrlLib,
    RenderScene,
    Ui3DViewUi;
file
    main.cpp;
mainconfig
    "" = "GUI";
```

Its `main.cpp` can start with:

```cpp
#include <CtrlLib/CtrlLib.h>
#include <Ui3DViewUi/Ui3DViewport.h>
#include <RenderScene/Scene3D.h>

using namespace Upp;

GUI_APP_MAIN
{
    TopWindow window;
    Ui3DViewport pane;
    Ui3DView& view = pane.View();
    window.Title("Ui3DView example").Sizeable().Zoomable();
    window.SetRect(0, 0, DPI(900), DPI(650));
    window.Add(pane.SizePos());
    pane.SetDark(true);

    String error;
    auto mesh = Scene3DMesh::MakeBox(Scene3DVec3(2, 2, 2), error);
    if(!mesh) { PromptOK(error); return; }

    Scene3DObjectDesc cube;
    cube.id = "demo:cube";
    cube.mesh = mesh;
    cube.local = Scene3DMatrix::Translation(Scene3DVec3(0, 1, 0));
    cube.r = 0.20f; cube.g = 0.55f; cube.b = 0.85f;
    Scene3DSceneDesc description;
    description.objects.push_back(cube);

    auto scene = Scene3DSnapshot::Create(description, error);
    if(!scene || !view.SetScene(scene, error) ||
       !view.SetOptions(Ui3DViewOptions::Dark(), error)) {
        PromptOK(error);
        return;
    }

    // The application accepts selection; this example replaces the selection.
    std::vector<String> selection;
    view.WhenSelectionRequest = [&](const Ui3DSelectionRequest& request) {
        if(!view.IsCurrentPick(request.pick)) return;
        std::vector<String> candidate;
        if(request.pick.hit) candidate.push_back(request.pick.id);
        String selection_error;
        if(view.SetSelection(candidate, selection_error)) {
            selection = candidate;
            pane.RefreshChrome();
        }
        else PromptOK(selection_error);
    };
    view.WhenNavigation = [&](const Ui3DNavigationEvent&) {
        pane.RefreshChrome();
    };

    window.Open(); // Establish the actual child size before fitting.
    if(!view.FitAll()) {
        window.Close();
        PromptOK("Unable to fit the scene.");
        return;
    }
    pane.RefreshChrome();
    window.Run();
}
```

For a bare view, depend on `Ui3DView`, include `<Ui3DView/Ui3DView.h>`, and add a
`Ui3DView` directly to your layout. The scene/selection/camera calls above stay the
same; facade configuration and `RefreshChrome()` are unnecessary.

## Core API

The public header is the authority for exact declarations. Validating setters
return `bool` and take `String& error`; always check the result. Getters return
value snapshots, except that `GetScene()` returns shared immutable ownership.

| API | Contract |
| --- | --- |
| `SetScene(scene, error)` / `GetScene()` | Binds `std::shared_ptr<const Scene3DSnapshot>`. Same pointer is a no-op except that it cancels an active pointer tool; a different pointer changes binding identity and cancels navigation. Null explicitly clears the scene. |
| `SetCamera(camera, error)` / `GetCamera()` | Validates a complete `Scene3DCamera`, cancels an active drag and advances the control's camera revision. Even identical values advance that revision; avoid calling from every paint or poll. |
| `SetOptions(options, error)` / `GetOptions()` | Controls palette, grid/axes/selection visibility, grid plane/spacing, lighting, physical film/safe guides and legacy display flags. Invalid options preserve the previous state. |
| `SetDisplayMode(mode, error)` / `GetDisplayMode()` | `Ui3DDisplayMode::{Solid, Wireframe, SolidEdges}`. Prefer this to manipulating the legacy flags; `wireframe` wins when both legacy flags are set. |
| `SetViewDirection(direction, error)` | Applies an exact canonical pose, orthographic projection and matching grid atomically: Front/Back → XY, Right/Left → YZ, Top/Bottom → XZ. `User` is a classification, not an assignable preset. |
| `SetOrthographic(on, error)` | Switches projection while preserving vertical framing at the target plane. |
| `SetNavigationTool(tool, error)` / `GetNavigationTool()` | `Ui3DNavigationTool::{Select, Orbit, Pan}` determines future left presses. An existing gesture or pending click keeps the mode selected at its original press. |
| `SetSelection(ids, error)` / `GetSelection()` | Projects accepted `std::vector<String>` IDs into this view. IDs must exist in the current scene and be unique. Does not send a selection request. |
| `FitAll()` / `FitSelection()` / `FitBounds(bounds)` | Return `bool`; require usable viewport dimensions and bounds. Selection framing uses selected objects' own visible mesh or Gaussian bounds and falls back to scene bounds if none remain. |
| `Pick(Pointf)` / `IsCurrentPick(pick)` | Returns a geometric hit or miss and validates that the result still belongs to this view, scene binding, camera and viewport size. |
| `GetSceneBindingRevision()` | Per-view binding identity; independent of the application's scene revision. |
| `IsNavigating()` / `CancelNavigation(reason)` | Queries or cancels the active navigation transaction, restoring its starting camera. |
| `GetViewStats()` / `GetViewError()` | Rendering/upload/depth diagnostics and the combined content/presenter error. Inherited `IsGpuReady()`, `RequestGpuRefresh()`, `RetryGpuInit()`, `GetGpuStats()` and `SetValidation(bool)` remain available. |

`Ui3DViewOptions` contains `background`, `grid_color`, `selection_color`,
`edge_color`; `show_grid`, `show_axes`, `show_selection`, `lighting`;
`wireframe`, `solid_edges`; `adaptive_grid`, `grid_plane`, `grid_extent` and
`grid_spacing`; `show_gate`, `show_safe`, `matte_opacity`, `action_safe`,
`title_safe`, `matte_color`, `gate_color` and `safe_color`. Decoration colours
are sRGB bytes with alpha **255**. Extent is 1–100 cells each side; fixed spacing
is finite 0.001–10000 metres. The independent matte opacity is finite 0–1;
safe fractions satisfy `0 < title_safe <= action_safe <= 1`.
`Light()` and `Dark()` return complete initial option values.

`Ui3DViewStats` exposes `rendered_frames`, `mesh_uploads`, `uploaded_bytes`,
`depth_rebuilds`, `resident_meshes`, `draws_last_frame`, `target_size` and
`depth_active`. Material diagnostics add `texture_uploads`, `resident_textures`,
`resident_texture_bytes`, `working_target_rebuilds`, `blended_draws_last_frame`,
`textured_draws_last_frame` and `gate_draws_last_frame`. Resident texture bytes
count immutable asset pixels; they exclude the neutral white texel and working
color/depth targets.
Gaussian diagnostics add `splat_uploads`, `splat_sort_updates`,
`splat_sort_us_last_frame`, `resident_splat_bytes`, `resident_splat_resources`,
`resident_splat_instances`, `splat_draws_last_frame` and `splats_last_frame`.
The byte count covers actual retained vertex/index buffers; sort duration is
native CPU time and does not measure GPU execution or scan-out.

### Scene and camera values

- Build meshes with `Scene3DMesh::Create(vertices, indices, error)` or
  `MakeBox(full_size, error)`. Factories validate and copy arrays; published data
  has no mutable alias. Reuse shared const meshes in later snapshots.
- A `Scene3DObjectDesc` contains stable `id`, optional `parent_id`, `local`
  transform, shared mesh or Gaussian resource, `visible`, `selectable`, linear
  material RGBA, independent `opacity` and mesh-only `Scene3DMaterial`. Transform-only
  parent nodes are supported. `Scene3DSceneDesc` contains a nonzero revision and
  ordered objects; `Scene3DSnapshot::Create` validates the complete candidate.
- Coordinates are **metres, right-handed, Y up**, with asset forward +Z. Matrices
  are column-major and multiply column vectors: `world = parent * local`.
  Camera yaw zero looks toward −Z. Angles are radians; screen origin is upper-left
  and projected depth is Vulkan 0–1. Do not add an extra OpenGL-style Y/depth conversion.
- `Scene3DCamera` exposes `target`, `yaw`, `pitch`, `distance`, `orthographic`,
  `fov_y`, `ortho_height`, `near_plane`, `far_plane`, `roll`, `gate_aspect` and
  `revision`. `Validate`, `GetEye`, `GetBasis`, `Project`, `WorldRay`, `Orbit`,
  `Pan`, `Dolly`, `Fit`, `AlignTo`, `SetOrthographic`, `SetLookAt`, `GetGateRect`
  and `GetViewDirection` provide the value API.
  Pan uses screen pixels; positive logarithmic dolly moves closer.
- `Scene3DCamera::AlignTo` changes pose only. The control's `SetViewDirection`
  additionally applies projection/grid policy. Projection conversion preserves
  `ortho_height = 2 * distance * tan(fov_y / 2)`. Fit expands clipping only when
  necessary; dolly preserves authored near/far planes. Neutral camera mutators
  advance revision only for actual changes; control/model revisions are separate.


### Textures, coverage and camera gates

Create a `Scene3DTexture` from a complete top-down array of straight RGBA8 bytes:

`Scene3DTexture::Create(Size, const std::vector<byte>&, Scene3DTextureColorSpace, error)`

The immutable factory copies the array. `GetSize()`, `GetPixels()` and
`GetColorSpace()` expose only read access; an importer may discard its original
buffer. `Srgb` decodes RGB for linear shading, while `Linear` leaves RGB linear.
Alpha is linear in either encoding. Mesh UV coordinates are V-up. The native
sampler uses bilinear filtering and either clamp-to-edge or `repeat_uv`.
The renderer performs no file access, image decoding or asset resolution.

A material contains `color_texture`, `alpha_mode`, `alpha_cutoff`, `unlit`,
`repeat_uv` and `double_sided`. An absent texture is white. The object and
vertex RGB factors are linear; the sampled material is multiplied by both.
Coverage has a deliberate order:

| Material mode | Material coverage before independent object opacity |
| --- | --- |
| `Opaque` | 1, ignoring texture, vertex and material-factor alpha |
| `Mask` | Discard when combined material alpha is below the cutoff; survivors have coverage 1 |
| `Blend` | Combined texture × vertex × material-factor alpha |

The resulting coverage is multiplied by the object's independent `opacity`.
This lets an opaque character fade without changing its imported material and
lets a cutout retain its holes while fading. Cutoff equality survives; at cutoff
zero, a zero-alpha texel survives. Zero final coverage neither draws nor picks.

Opaque and masked surfaces with opacity 1 write depth. Blended or faded surfaces
test depth without writing it and are stably sorted far to near by object-bounds
centre. This is ordinary surface compositing, not order-independent transparency:
split intersecting translucent meshes into suitable surfaces in the host.
A false `double_sided` culls back-facing triangles in both rasterization and CPU
picking. The default is true for compatibility with earlier snapshots.

Shading and composition happen in linear space. An sRGB target encodes in the
attachment. An UNORM display target uses a retained sRGB working attachment plus
a GPU-only encoding resolve; the renderer never substitutes a CPU-rendered image.
The standard decorations are sRGB palette bytes and are converted consistently.

`camera.SetLookAt(eye, aim, up, error)` retains authored up/roll, including exact
pole views, and rejects parallel/zero up atomically. A positive `gate_aspect`
fits a centred physical gate in the viewport; zero uses the full viewport.
`fov_y` and `ortho_height` describe the physical gate, and the viewport may show
overscan. Projection, picking rays, panning and fitting use the same gate rule.
`GetGateRect(size)` provides its bounded pixel rectangle.

Set `show_gate` for matte/film-boundary display and `show_safe` for action/title
guides. These view-only decorations do not change the camera or document.
`ComputeUi3DViewGateLayout` provides the same pure bounded rectangles used by the
GPU renderer. Guides load the completed target and use the retained native 2D
renderer; they are not GDI overlays over the 3D window.

### Gaussian covariance resources

`Scene3DSplats::Create(const std::vector<Scene3DSplat>&, String& error)` validates
and copies its input into shared immutable ownership. `GetSplats()` and
`GetBounds()` are read-only. Each `Scene3DSplat` contains its local `position`,
six covariance values in **xx, xy, xz, yy, yz, zz** order and straight linear
`r/g/b/a`. Covariance is in squared metres; it is not an axis-aligned radius
or a proxy mesh. RGB/alpha must be finite 0–1. The factory checks finite,
positive-semidefinite covariance, allowing zero/rank-deficient density.

Set `Scene3DObjectDesc::splats` to that resource. A node may contain a mesh or
splats, never both; a transform-only parent remains valid. The scene admits at
most `Scene3DMaxSplats` (150,000) **instanced** points, so two instances of an
80,000-point resource exceed the budget even though source storage is shared.
The renderer performs no PLY/file decoding or asset lookup.

Projection uses the complete local-to-camera transform and its perspective
Jacobian: parent/nonuniform scale, camera up/roll and the physical gate all affect
the same screen covariance. `Scene3DProjectSplat` provides the CPU projection
and `Scene3DSplatProjection::Coverage(Pointf)` evaluates its density.
`Scene3DSplatPixelVariance` adds 0.25 squared pixels for raster filtering.
The native shader expands the exact projected ellipse into a GPU quad and
discards density below `Scene3DSplatAlphaCutoff` (1/255). It computes eigenvalues
in scale-normalized covariance to avoid overflowing valid large float products.
Source RGB is multiplied by the object's linear factor; coverage is density ×
point alpha × object alpha × independent object opacity.

Use `Scene3DSnapshot::PickScreen(point, viewport, camera)` for a mixed mesh/cloud
scene. It applies the same projection, clipping and effective coverage and
compares visible/selectable hits with the ordinary mesh ray result.
`Scene3DPick::splat` is the original resource point index, independent of GPU
sorting; mesh hits retain `triangle`. The control's `Pick` uses this route and
also respects an opaque film matte. These remain CPU analytic picks, not GPU
readback or physical contact results.

Gaussian fragments test mesh depth without writing depth. Each instance has a
stable far-to-near index order; immutable source vertex data is reused within
a view. A changed transformed view direction re-sorts. Pure camera translation,
FOV or roll does not alter depth order. Hide/delete/replacement/close releases
unused resources even if another CPU owner still retains the source.
At the limit the current four-vertex/six-index representation uses 264 bytes per
point (39,600,000 bytes for one 150,000-point instance). Additional instances
share source vertices but need their own sorted indices.

Transparent scene objects are still ordered by their bounds centres. Separate
intersecting clouds are not globally interleaved order-independent transparency.
The finite admission limits are not precision or frame-rate guarantees: the CPU
uses doubles and GPU attributes/calculations use float32.

## Events and selection

`WhenSelectionRequest(const Ui3DSelectionRequest&)` asks the application to decide.
Its `pick` contains `hit`, `id`, `world_point`, `world_normal`, `distance`,
`triangle`, `splat`, scene/camera revisions, the scene binding and sampled viewport size.
Use `IsCurrentPick()` before accepting delayed results, including misses.
Interpret `extend` (Shift) and `toggle` (Ctrl) against your own selection model;
the quick-start example intentionally implements replacement selection only.
Publish accepted IDs to all relevant views with `SetSelection`.

`WhenNavigation(const Ui3DNavigationEvent&)` provides `phase`, `mode`,
`gesture_id`, originating `scene_revision`/`scene_binding_revision`,
`starting_camera_revision`, the copied `camera` and a `reason`.

| Phase | Application integration |
| --- | --- |
| `Begin` | Start a preview/undo transaction for this pane and gesture. |
| `Update` | Preview the event's camera value; refresh labels/inspector without repeatedly calling `SetCamera` back into the originating view. |
| `Commit` | Accept the final camera and finish the matching preview. Wheel and framing input can emit a standalone Commit without a Begin. |
| `Cancel` | Restore/cancel the matching application preview. Match gesture and binding identity so an older deferred cancellation cannot cancel newer work. |

Programmatic setters do not fabricate user Commit or selection events. Replacing
a scene/camera can cancel an existing gesture and invoke application code.
Callbacks can rebind or destroy the control: consume event values, check setter
results and use U++ `Ptr` lifetime guards when continuing after such callbacks.
Capture-loss notification is deferred until U++ completes capture release;
destruction releases capture and suppresses pending application notifications.

### Caller-owned pointer tools

`WhenToolPointer(const Ui3DToolPointerEvent&) -> bool` is offered before left
navigation/selection, including double press. False at Begin preserves existing
input behavior; true accepts an exclusive captured gesture. Update false cancels.
Commit/Cancel return values are ignored. The accepted callback is retained for
the whole gesture, independently of later callback assignment. Existing
`WhenNavigation` and `WhenSelectionRequest` remain installed.

Events contain phase, gesture ID, retained immutable scene, copied starting
camera, `start_pick` (scene/binding/camera revisions and viewport), start/current
point, current keyflags, a ray in the frozen projection with `ray_valid`, and
terminal reason. The geometric start pick need not hit a mesh: the caller can
hit its own projected handles using the supplied camera and scene. All calls
and callbacks are GUI-thread only.

Use `IsToolPointerActive()` and `CancelToolPointer(reason)` for tool switching.
Ui3DView owns capture, input routing and lifecycle; the caller owns handle
geometry, transform/pivot mathematics, authoritative placement, preview and undo.
For live geometry, call
`SetToolPointerPreviewScene(event, candidate, error)` from Begin/Update.
It accepts only an event token from this view's active gesture, with matching
scene/binding/camera/viewport context. The validated immutable candidate is retained
and used by the existing renderer and geometric picking. `GetScene()` and
`GetSceneBindingRevision()` continue to describe the authoritative scene;
gesture camera/start-pick context stays frozen. Picks from another displayed
snapshot become stale even when its numeric scene revision happens to match.
The caller must preserve document object IDs in its supplied preview.

Null clears the transient preview. Cancel restores authoritative rendering;
Commit clears the preview and releases capture before notifying the host.
A declined Begin or rejected Update clears any installed preview too.
Publish accepted document/scene edits once at Commit. Ordinary `SetScene`,
including rebinding the same authoritative pointer during a tool, cancels.
Publishing a different scene/camera or changed selection cancels the tool.
Wheel/navigation input and release-selection are suppressed during a tool gesture.
Escape, focus/capture loss, resize, hide/disable/close and renderer failure cancel.
Capture-loss notification is deferred until U++ finishes releasing capture.
Destruction releases capture and suppresses pending callbacks; the host must
discard its preview when disposing the control. Callbacks may destroy/rebind the
view. Frozen scene ownership remains valid through the terminal notification.

This seam adds no manipulation drawing or renderer behavior. Its existing
pointer-tool/preview cases passed in the 18 November Release `Ui3DViewTest`
285-check run. Those cases use direct control handlers; the separate guarded
OS-input portion records actual paired presses and releases. Historical Debug
counts below predate this coverage and the owned native host.

### Input

| Input in the core view | Action |
| --- | --- |
| Middle drag / Shift+middle drag | Orbit / pan |
| Wheel | Dolly/zoom |
| Alt+left / Alt+middle / Alt+right drag | Orbit / pan / dolly |
| Left press/drag | Current Select, Orbit or Pan tool |
| Select double-click | Request selection, then frame that hit if the callback has not invalidated it; a miss does not frame |
| F / Home | Frame selection (or scene) / frame scene |
| Escape, capture/focus loss, hide, disable, close or rebind | Cancel navigation and restore the starting camera |

The demo additionally handles V/O/H tools, 1/3/7 axis views (Ctrl chooses the
opposite), 5 projection, Q single/quad, Space maximize/restore and I inspector.
These application shortcuts are scoped to viewer focus; they are not all part
of the base control. In the demo, Escape when idle also clears selection.

## Optional native UI

`Ui3DViewport::View()` exposes the core API. The wrapper preserves the host's
`WhenNavigation` and `WhenSelectionRequest`; call `RefreshChrome()` after changing
the base view directly and from your navigation synchronization callback.

| Facade API/event | Use |
| --- | --- |
| `SetActive`, `SetDark`, `SetCompact` | Active styling, chrome theme and compact orientation control. |
| `ShowToolbar`, `ShowOrientation`, `ShowNavigation`, `ShowAnnotations` | Independent requested preferences. Small panes may temporarily hide controls that do not fit. |
| `IsToolbarShown`, `IsOrientationShown`, `IsNavigationShown`, `AreAnnotationsShown`, `IsCompact`, `IsActive`, `IsDark` | Inspect those preferences/state. |
| `SetCameraLabel(label)` | Supplies the camera tooltip label; does not bind or store a saved camera. |
| `SetSizeMin` / `SetSizeFixed` | Additional minimum dimensions above the intrinsic 175×120 DPI-scaled floor. Here `SetSizeFixed` is an alias for the minimum, not an exact layout allocation. |
| `WhenActivate` / `WhenMaximize` | Host chooses its active pane or changes its surrounding layout. |
| `WhenCameraCommand` | One notification after a successful facade camera command. Reconcile the host camera model; a successful no-op can notify too. |
| `WhenViewOptions` | Reconcile display, overlay or navigation-tool changes; do not edit a saved camera. |
| `PopulateCameraMenu(UiMenu&)` / `WhenCameraMenu` | Populate the owned native menu, or provide your own popup. The passed menu reference is borrowed and must not be retained. |
| `GetChromeStats()` | Read cumulative native/tile/axis/annotation paint counts, region/z-order updates and maximum tile pixels; these are operation counts, not GPU timing. |

`Ui3DViewportChromeStats` fields are `native_paints`, `tile_paints`, `region_updates`,
`z_order_updates`, `axis_paints`, `annotation_paints` and `max_tile_pixels`.

`SetDark` themes **chrome only**. Theme the scene through `View().SetOptions`.
For a theme change, start from `GetOptions()` and copy only the four palette
fields from `Light()`/`Dark()` so the user's grid, display and overlay choices survive.

Direction and camera selectors are always square icons; full names remain in
tooltips and native menus. Below 310 logical units, direct display buttons move
into their menu. The shared UiIcons catalogue must provide:

| Selector | UiIcons function |
| --- | --- |
| User orthographic/perspective | `ICON_UI_ACTION_DEPLOYED_CODE_OUTLINED` |
| Front/back/left/right | `ICON_UI_ACTION_RESPONSIVE_LAYOUT_OUTLINED` |
| Top/bottom | `ICON_DESIGN_BORDER_OUTER_48` |
| Free/saved camera | `ICON_IMAGE_VIDEO_CAMERA_BACK_OUTLINED` |

The orientation control supports all six signed axes, projection switching,
keyboard arrows and Enter/Space. Clicking the current canonical axis chooses its opposite.

### Saved cameras and multiple panes

Use [ViewerCameraModel.h](../examples/Ui3DViewDemo/ViewerCameraModel.h) and
[Ui3DViewDemo.cpp](../examples/Ui3DViewDemo/Ui3DViewDemo.cpp) as the application
integration example. The demo keeps four persistent pane identities and shares
scene/selection while cameras, display options and overlays remain per pane.
Single/quad changes preserve those controls and their state.

The demo model's `Save`, `Load`, `Free`, `Rename`, `UpdateFrom` and `Remove`
commands operate on stable camera IDs. Navigation detaches a changed pane from a
saved pose; cancel restores its starting pose/binding when valid. Saving/loading
does not save grid/display/chrome settings. Update affects still-bound panes;
remove preserves their poses. One-step undo covers update/remove and preserves
newer edits. Names allow 1–40 Unicode scalar values after trimming; duplicate
names are allowed, IDs remain unique. Keep IDs or copies across model mutations.

`BeginPreview`, `PreviewCamera`, `CommitPreview` and `CancelPreview` bridge the
navigation phases. Cancel or finish previews before catalogue commands. Follow
the demo's `Navigate`/`CancelGestures` identity checks. This session model does not
implement disk persistence or general document undo; those remain application responsibilities.

## Gotchas and rendering contracts

1. **Do not bind a failed factory result.** Null passed to `SetScene` clears the
   current scene. Validate the candidate first. IDs must be unique/nonempty;
   missing parents, cycles, invalid indices, degenerate triangles, nonfinite
   data and singular/reflected transforms are rejected.
2. **Selection IDs survive scene rebinding.** Clear or republish application
   selection when switching documents. A reused ID can immediately highlight a
   different document's object. Transform-only parents do not implicitly select
   or frame their descendants; expand group selection in the application.
3. **Picking is material-aware geometric picking.** CPU triangles use bounds
   rejection, the material's face-culling rule and bilinearly sampled alpha
   coverage, including in wireframe. Nonselectable objects are skipped and do
   not block selection of selectable geometry behind them. This remains CPU
   geometry picking; Gaussian hits use transformed analytic screen coverage.
   It is not GPU rendered-pixel, physics or contact picking.
4. **Fit after layout.** The control needs nonzero dimensions. `SetCamera` can
   trigger cancellation callbacks and advances the control revision; avoid
   camera feedback loops and do not equate its revision with your model revision.
5. **Keep material alpha and object opacity separate.** Vertex/object RGBA
   factors are finite linear 0–1. Opaque, Mask and Blend resolve material coverage
   before independent object opacity; see the table above. SolidEdges and
   selection respect the material's alpha test/fade and use uniform mesh-edge
   highlighting without recolouring faces. Triangulation edges can be visible;
   this is not a silhouette or authored-quad edge extractor.
6. **Keep geometry local.** CPU calculations use doubles and GPU data uses
   float32. Large coordinates with small details can diverge visually from CPU
   picking. Validation budgets are not precision or frame-rate guarantees.
   Snapshot limits are 100,000 objects, 2 million instanced vertices and 6 million
   instanced indices, 150,000 instanced Gaussian points and 512 MiB of distinct
   immutable texture pixels. Each texture
   is at most 16384 pixels per dimension and 32 million pixels total. Per-view
   color/depth targets are limited to 16,777,216 pixels.
7. **Retain resources and invalidate deliberately.** Camera/selection changes do
   not reupload unchanged meshes or immutable textures. GPU resources are cached
   per view, not shared across views merely because CPU scene values/context are
   shared. Gaussian source and per-instance ordering follow the same ownership rule. Unreferenced asset buffers/textures are released on scene rebinding. Adaptive grids use
   bounded 1/2/5 spacing and a cell-snapped centre; unchanged layout retains buffers.
   There is no idle animation timer; publish evaluated scene snapshots from the
   application's playback clock. Presentation is synchronous, including queue
   waiting; no asynchronous frames-in-flight or sustained-FPS guarantee is claimed.
8. **Keep the renderer content owned by Ui3DView.** Do not replace inherited
   `SetGpuContent` or expect inherited 2D build/paint callbacks to add 3D overlays;
   the custom content bypasses them. Extend the owning renderer layer instead.
   CPU snapshots survive GPU retry/reopen; GPU resources close while the device lives.
9. **Preserve the native overlay contract.** Windows GPU-host and overlay HWNDs
   require `WS_CLIPSIBLINGS` at creation; preserve their disabled-HWND input
   routing. The overlay uses stable bounded islands and fixed annotation slots.
   U++ `DrawCtrl` already buffers those tiles. Do not rebuild its region from
   moving axis endpoints, add a full-viewport GDI backbuffer, or repaint all chrome
   for every camera update. Layout invalidates the bounded header; ordinary chrome
   refreshes use equality guards/selective repaint. See
   [Ui3DViewport.cpp](../render/Ui3DViewUi/Ui3DViewport.cpp) and
   [GpuCtrl.cpp](../render/GpuRender/GpuCtrl.cpp).
   Both surfaces use the reusable public-`Ctrl` host
   [Win32ChildSurface](../render/RenderPlatformWin32/Win32ChildSurface.h).
   It keeps the full logical surface size and clips only its owned HWND region.
   In U++18468 the old `DHCtrl` GDI exclusion used the whole logical rectangle,
   which suppressed fixed-header painting when a child scrolled behind it.
   The owned Win32 class avoids that exclusion without modifying the installed
   framework or copying private U++ registration. Normal `WS_CLIPCHILDREN`
   clipping honors the actual region. Lifecycle and fixed chrome must be checked
   with absolute composed pixels, not only differences between two screenshots.

The view exposes host-owned pointer-tool and preview-scene seams; importer,
deformation/skinning and document commands remain host responsibilities. Native
accelerated picking, depth/normal/ID export and additional backends require
further implementation. Gaussian rendering is implemented as described above. The orientation control already exists.
The headers and this guide describe the implemented control.

## First macOS run

The first target is the embedded Ui3DView used by CineStage. The Cocoa host attaches a Metal layer to the U++ window. Vulkan creates a surface from that layer and uses the same scene, materials and SPIR-V as Windows. The Windows whole-window compositor, its DirectWrite provider and its native screen-capture tests have separate platform requirements.

1. Use the existing Cocoa U++ installation with `umk`, the `uppsrc` directory and a configured Apple Clang build method supporting C++17. The Windows reference used U++ 18468. The local handoff ZIP contains the working renderer and the Ui and Animation source packages as siblings. For separate checkouts the reference revisions are Ui `cce7ecd25f72be926204beb4ea35a6ed550991a4` and Animation `4a01b6f4e2a9f122ea1a93457b62a4054d01f970`.
2. Install the macOS Vulkan SDK with the loader and a compatible driver, then source its `setup-env.sh` in Terminal. The renderer requires Vulkan 1.3, dynamic rendering and synchronization2. On Apple Silicon with macOS 26 or later the SDK offers KosmicKrisp. MoltenVK is the SDK option for compatible older Macs. Driver choice and SDK environment setup are described in the [LunarG macOS guide](https://vulkan.lunarg.com/doc/view/latest/mac/getting_started.html).
3. From the extracted `upp_render` directory, run the following commands. Set `UMK` if the tool is outside PATH and `UPP_METHOD` if the installed build method has another name. The script also accepts `UPP_UI` and `UPP_ANIMATION` for separate checkout roots.

```bash
export UPP_SRC="/path/to/upp/uppsrc"
bash tools/test-macos.sh --validation
```

The script builds and runs the scene, camera and RHI checks, creates a Vulkan device, exercises two independent GPU controls through hide/show and teardown, and verifies depth-tested presentation in the actual demo in single- and four-pane layouts. Build output and runtime logs go under `build/macos`. Omit `--validation` to run without the SDK validation layer. A local run works before the Klick tunnel is connected.

For visual acceptance, run `build/macos/Ui3DViewDemo.app/Contents/MacOS/Ui3DViewDemo` from the same SDK shell. Check that the scene appears before clicking Fit, orbit through the orientation overlay, switch between single and four panes, resize, and reopen the window. Check label visibility, picking under the cursor and flicker on each available display scale. Capture the Mac model, OS, selected Vulkan driver and the first failing log or visual observation before promoting the build to CineStage.

## Build and run

The recorded Windows toolchain is C++17, `E:/upp-18468/umk.exe`,
`CLANGx64_Vulkan.bm` and Vulkan SDK `C:/VulkanSDK/1.4.350.0`.
The 23 November viewer build used the Ui and Animation revisions listed above. The older Windows whole-UI showcase uses a separately pinned Ui export.

```powershell
$ui3dRoot = 'E:/apps/github/upp_render'
$ui3dUmk = 'E:/upp-18468/umk.exe'
$ui3dMethod = "$ui3dRoot/CLANGx64_Vulkan.bm"
$ui3dNests = 'E:/apps/github/upp_render/render,E:/apps/github/upp_render/examples,E:/apps/github/upp_render/tests,E:/apps/github/upp_render/tools,E:/apps/github/upp_Ui,E:/apps/github/upp_animation,E:/upp-18468/uppsrc'

& $ui3dUmk $ui3dNests Ui3DViewDemo $ui3dMethod --out-dir "$ui3dRoot/build/cache-ui3d-release" -rH4 +GUI "$ui3dRoot/build/Ui3DViewDemo.exe"
```

Build into `build/`. After a successful Release build and relevant verification,
replace **`bin/win64/Ui3DViewDemo.exe`** and verify the copied hash; this is always
the latest demo location. Through Klick, use the approved U++ Builder and
hash-guarded Patch copy. Normal interactive launch takes no arguments.

### Focused verification commands

```powershell
& $ui3dUmk $ui3dNests Ui3DViewTest $ui3dMethod --out-dir "$ui3dRoot/build/cache-ui3d-release" -rH4 +GUI "$ui3dRoot/build/Ui3DViewTest.exe"
& $ui3dUmk $ui3dNests Ui3DViewTest $ui3dMethod --out-dir "$ui3dRoot/build/cache-ui3d-debug" -bH4 +GUI "$ui3dRoot/build/Ui3DViewTest-debug-blitz.exe"
& "$ui3dRoot/build/Ui3DViewTest.exe"
& "$ui3dRoot/build/Ui3DViewTest-debug-blitz.exe"

& "$ui3dRoot/build/Ui3DViewDemo.exe" --qualify --validation
& "$ui3dRoot/build/Ui3DViewDemo.exe" --qualify-overlays --validation "--overlay-output=$ui3dRoot/build/Ui3DViewOverlay"
& "$ui3dRoot/bin/win64/Ui3DViewDemo.exe" --smoke --capture-matrix --validation
```

Ordinary Debug uses `-H4` and a separate `-debug.exe` output; Debug+BLITZ uses
`-bH4`. Console suites use the same argument order with their package/output name
and `+CONSOLE`: `RenderSceneTest`, `Ui3DViewCameraTest`, `RenderRhiTest`,
`RenderVulkan3DTest`, `RenderVulkanTest` and `RenderVulkanGraphicsTest`.
Validation mode requires the developer validation layers.

`Ui3DMaterialTest` exercises the production `Ui3DView` renderer on small owned
Vulkan targets. It compares texture orientation, declared color spaces, each
coverage mode, independent fade, depth/culling, camera roll, physical guides and
retained allocations. Its Gaussian cases add covariance projection, anisotropic
parent scale/roll, density/alpha, sorting, resource lifetime and the full point
budget. `RenderSceneTest` separately checks immutable admission and analytic
projection/picking against independent arithmetic. Optional `--output <build-folder>` writes bounded target
images and a report. Those target pixels are separate evidence from desktop
composition or final-screen capture.

The five material/resolve shaders and two Gaussian shaders are checked-in GLSL
plus SPIR-V word headers.
Rebuild the headers with the existing UMK-built `RenderShaderBake` development
utility and the pinned developer shaderc SDK, then review/patch the generated
headers. The application links the word arrays and has no shaderc/SDK runtime
DLL dependency.

`--smoke` performs a finite readiness check; `--capture` saves four theme/layout
images at 1440×900; `--capture-matrix` covers all four sizes below. Reports/captures
normally go beside the executable; `--overlay-output` selects the motion-probe
directory. Native tests use actual guarded input: keep their owned windows
visible/unobstructed. Do not remove foreground/target guards to obtain a pass.

Choose verification for the changed behaviour: scene/camera tests for mathematics,
native tests for control/input/lifetime, workspace/motion checks for UI integration,
and GPU pixel/ownership tests for renderer changes. Documentation-only edits need
reference checks; isolated icon swaps need compilation, not the complete runtime matrix.

## Tested status

### 23 November 2026 — Windows verification for macOS preparation

The installed ordinary Windows Release is `bin/win64/Ui3DViewDemo.exe`, **6,893,568 bytes**, SHA-256 `e05ff750520a6cded1f9e245c4adfbdf753ba89b0886d92221ac2b063acac40b`. The ordinary and BLITZ Release builds both presented a depth-tested frame in all four panes with Vulkan validation enabled.

| Windows check | Result |
| --- | --- |
| Scene, camera catalogue and RHI | 342, 183 and 196 checks passed, respectively. |
| VulkanProbe and GpuCtrlPresentationTest | Device creation passed with validation. Embedded presentation, continuous refresh, hide/show and final resource release passed. |
| Ui3DViewTest | 285 checks passed with 12 paired native presses and releases. |
| Ui3DMaterialTest | 469 checks passed, including 105 pixel comparisons across 82 GPU frames. |
| Installed demo workspace and overlay motion | 90 workspace checks and 25 overlay checks passed. The ordinary build sampled 35 frames with zero blank projection or caption samples. An earlier overlay invocation stopped at its foreground guard before injecting input. |

The user-reported flicker remains open. The next hardware qualification is the [first macOS run](#first-macos-run).

### 18 November 2026: Gaussian features and native composition

These are actual Release UMK18468/bundled-Clang Windows runs against the current
source, with no Python, CMake or shader compiler runtime in the application.
Counts are per execution and describe distinct suites.

| Test | Result | Scope |
| --- | --- | --- |
| `RenderSceneTest` | **342 checks, zero failures** | Gaussian immutable admission, finite/PSD covariance, independent projection arithmetic, coverage picking, source indices and instance budgets, plus existing mesh/camera tests. Runtime `exec-39568F511C15FFF0BB5A9F647966F75B`. |
| `Ui3DMaterialTest` | **469 checks, 105 pixel comparisons, 82 GPU frames, zero failures** | Actual production Vulkan targets, including 38 Gaussian pixel cases, depth/sort/cache/cleanup and 150,000-point workload. Validation enabled with no warnings/errors. Runtime `exec-4EB925C1F2180EFDAF1B9459AE911D51`. |
| `Ui3DViewTest` | **285 checks, zero failures** | Existing native suite with the public Win32ChildSurface host lookup; 12 actual OS presses, 12 matching releases, one owned-caption activation, cancellation/callback destruction and Vulkan cleanup. Runtime `exec-8163B148C4851DFF5B18DCC3BF79E3E1`. |
| CineStage `UiNative` r20 | **770 checks, zero failures** | Five-mode dark/light compact/wide shell; native child clipping, actual composed pixels, four-view/close/reopen, responsive XYZ, measured scroll and shared dialogs. 94 PNGs: 64 native captures plus 30 ordinary-control reference images. Runtime `exec-CC6F1C7D0D652AAACE46FD68D232986D`. |

The first GPU run exposed covariance-product overflow; the scale-normalized
shader repair is included above. A separate uniform-mesh probe established
attachment blend-factor precision for two low-alpha pixel cases. The oracle
accepts ideal blending or an explicitly calculated 8-bit blend factor using the
same original ±2 encoded-value tolerance; it does not apply a generic tolerance
increase. See the [Vulkan blending precision contract](https://docs.vulkan.org/spec/latest/chapters/framebuffer.html#framebuffer-blending).
Both earlier failed snapshots remain retained.

The full point workload retained 39,600,000 GPU buffer bytes. Its first
publication/render/readback took 49.6723 ms; the CPU sort was 13,147 microseconds.
Twelve steady submissions totaled 104.5937 ms, with a largest submission of
8.782 ms. These are native CPU wall times for one synthetic workload, not GPU
timestamps, sustained FPS or scan-out measurements.

CineStage's 30 absolute fixed-chrome comparisons each had zero differing pixels;
60 native-region observations had no fixed-header intrusion. Quad/single,
hide/show, destruction and facade reopen retained actual GPU content. The
owned-window test observed display OFF to ON after a scoped display-awake request
and restored prior thread execution flags. No power policy was changed.
The composition suite drives handlers/projections and does not establish OS
pointer routing. The separately rerun guarded `Ui3DViewTest` closes that native
host risk: 12 inserted presses have 12 matching releases, including nine owned
cleanup releases, with one successful fixture-caption activation. All existing
foreground, target, coordinate, lifetime and paired-input guards remain intact.
Only its test host cast/include changed from `DHCtrl` to the public
`Win32ChildSurface` API. The 285-check total also includes direct handler/value
cases; no new Debug/BLITZ or other-platform execution is implied.

Detailed current reports, source hashes, executable identities and earlier
failures live under `E:/apps/github/upp_cinespace/build/CineStage/unification`
in `gaussian-review-r1`/`r2`/`r3` and `ui-review-r19`/`r20`. The installed demo was
not rebuilt by these gates. Linux/macOS, other hardware and DPI transitions are
not covered.

### Historical 1 November 2026 baseline

The following results predate the new material/Gaussian/owned-native-host work.
They are retained history, not evidence that those later changes passed these
specific configurations. Counts are per execution; each recorded pass had zero
failures.

| Test | Configurations and result | Coverage |
| --- | --- | --- |
| `Ui3DViewTest` | **Release + Debug+BLITZ: 242 checks each** | Current native harness: selection/navigation, cancellation, callback destruction, retention and Vulkan lifetime; owned-window activation, click-suppression recovery and paired input. Previous ordinary Debug run: 194 checks with the earlier harness. |
| `RenderSceneTest` | Release + ordinary Debug: 242 each | Immutable hierarchy, camera/rays, exact axes/poles, clipping, picking, normal transforms and framing. |
| `Ui3DViewCameraTest` | Release + ordinary Debug: 183 each | Saved-camera identity/validation, previews/cancel, independent panes and guarded update/remove undo. |
| `RenderRhiTest` | Release + ordinary Debug: 194 each | RHI contracts and compact 3D vertex layout. |
| `RenderVulkan3DTest` | Release + ordinary Debug: 291 checks + 1,928 oracle pixels each | Indexed/depth/edge rendering, clipping and diagnostics; validation active with zero warnings/errors. |
| `RenderVulkanTest` | Release + ordinary Debug: PASS | Ownership and validation capture through close/repeated close; no numeric check count recorded. |
| `RenderVulkanGraphicsTest` | Release: PASS | Existing graphics regression; no fresh Debug result recorded. |
| Demo workspace | Release + ordinary Debug: 90 each | Four panes, layout, actions/shortcuts, camera transactions and field containment. Pre-icon-update demo. |
| Demo overlay motion | Release + ordinary Debug: 25 each | Guarded orbit, stable HUD pixels, changing axes/text and settled idle. Pre-icon-update demo; finite sampling. |
| Installed demo captures | Release: 16 scenarios passed and visually inspected | Dark/light × single/quad × 1440×900, 1222×773, 900×650 and 720×600. Pre-icon-update demo. |

The historical Debug+BLITZ foreground/input gap was **closed**. Both recorded native
runs exercised caption activation and deliberately armed click suppression,
with **12 successful presses and 12 matching releases**. The harness now verifies
client motion, positions button events explicitly and cleans up only owned input.
Final runs: Release `exec-7BCAFBCE1865DDDCE2FE592467D8EC85`;
Debug+BLITZ `exec-23D2661FD1406845F05685B58F90FD23`.

### Historical installed demo identity and limits

The **6,789,120-byte** demo installed on 1 November was the icon-selector compile/link update:

```text
E:\apps\github\upp_render\bin\win64\Ui3DViewDemo.exe
SHA-256: 542e5bc30dc83cc95d0fd539df27764d77d5543bb09c9395da42e07ec497d15d
```

Its icon change was **compile-only**, as requested. The workspace/motion/capture
results above belong to the preceding flicker-fix demo
`2ec273141adc3ffea33e3625f69632ca3515e65c27b01945e91d815c9d145c57`.
The later native-harness correction did not rebuild or rerun the demo.

Other DPI scales, monitor transitions, other hardware, non-Windows hosting and
screen-reader behaviour are outside these runs. The 18 November CineStage gate
adds facade close/reopen and contained native composition evidence, separately
from this historical installed demo. The overlay supports its fixed annotation
vocabulary; the motion probe does not sample every presented frame.

## Continuing development

Start from the headers linked above and the existing demo/test packages. Modify
the layer that owns the behaviour: scene mathematics in `RenderScene`, view
interaction in `Ui3DView.cpp`, 3D drawing in `Ui3DViewContent.cpp`, native UI in
`Ui3DViewport.cpp`, and catalogue/inspector/workspace policy in the demo or host.
Keep application models out of the reusable controls. Preserve immutable resource
ownership, callback lifetime guards, silent programmatic updates and bounded
native overlays. Add focused regression coverage for changed behaviour and
update this guide's API or tested-status section in place.
