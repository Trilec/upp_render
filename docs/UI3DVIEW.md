# Ui3DView — integration and maintenance guide

This is the single maintained guide for the 3D control, its optional native UI,
and its demo. Update this file when the API, behaviour or tested status changes;
keep detailed test output with build artifacts instead of adding handoff or qualification documents.

**Repository:** `E:\apps\github\upp_render`  
**Latest demo:** `E:\apps\github\upp_render\bin\win64\Ui3DViewDemo.exe`  
**Implementation and test status checked:** 1 November 2026.

[Quick start](#quick-start) · [API](#core-api) · [Events and selection](#events-and-selection) ·
[Native UI](#optional-native-ui) · [Gotchas](#gotchas-and-rendering-contracts) ·
[Build and run](#build-and-run) · [Tested status](#tested-status)

## Objective and boundaries

Provide a reusable native U++ control for a depth-tested Vulkan scene, with
orbit/pan/zoom, perspective and orthographic cameras, geometric picking, display
modes and optional standard viewport controls. An application can embed one view
or several views of the same immutable scene.

| Layer | Responsibility and source |
| --- | --- |
| `RenderScene` | Immutable meshes/snapshots, hierarchy, transforms, bounds, camera mathematics and CPU picking. [Scene3D.h](../render/RenderScene/Scene3D.h); depends on `Core`, with no GUI or GPU ownership. |
| `Ui3DView` | One view's camera, options, selection highlight, navigation and retained GPU content. [Ui3DView.h](../render/Ui3DView/Ui3DView.h); usable without the `Ui` library. |
| `Ui3DViewUi` | Optional `Ui3DViewport` wrapper with toolbar, orientation/navigation controls and annotations. [Ui3DViewport.h](../render/Ui3DViewUi/Ui3DViewport.h). |
| Renderer | `GpuRender` owns native hosting/presentation; `RenderRhi` owns backend-neutral graphics contracts; `RenderVulkan` implements the current Windows backend. Reuse these layers when extending the view. |
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
| `SetOptions(options, error)` / `GetOptions()` | Controls palette, grid/axes/selection visibility, grid plane/spacing, lighting and legacy display flags. Invalid options preserve the previous state. |
| `SetDisplayMode(mode, error)` / `GetDisplayMode()` | `Ui3DDisplayMode::{Solid, Wireframe, SolidEdges}`. Prefer this to manipulating the legacy flags; `wireframe` wins when both legacy flags are set. |
| `SetViewDirection(direction, error)` | Applies an exact canonical pose, orthographic projection and matching grid atomically: Front/Back → XY, Right/Left → YZ, Top/Bottom → XZ. `User` is a classification, not an assignable preset. |
| `SetOrthographic(on, error)` | Switches projection while preserving vertical framing at the target plane. |
| `SetNavigationTool(tool, error)` / `GetNavigationTool()` | `Ui3DNavigationTool::{Select, Orbit, Pan}` determines future left presses. An existing gesture or pending click keeps the mode selected at its original press. |
| `SetSelection(ids, error)` / `GetSelection()` | Projects accepted `std::vector<String>` IDs into this view. IDs must exist in the current scene and be unique. Does not send a selection request. |
| `FitAll()` / `FitSelection()` / `FitBounds(bounds)` | Return `bool`; require usable viewport dimensions and bounds. Selection framing uses selected objects' own visible mesh bounds and falls back to scene bounds if none remain. |
| `Pick(Pointf)` / `IsCurrentPick(pick)` | Returns a geometric hit or miss and validates that the result still belongs to this view, scene binding, camera and viewport size. |
| `GetSceneBindingRevision()` | Per-view binding identity; independent of the application's scene revision. |
| `IsNavigating()` / `CancelNavigation(reason)` | Queries or cancels the active navigation transaction, restoring its starting camera. |
| `GetViewStats()` / `GetViewError()` | Rendering/upload/depth diagnostics and the combined content/presenter error. Inherited `IsGpuReady()`, `RequestGpuRefresh()`, `RetryGpuInit()`, `GetGpuStats()` and `SetValidation(bool)` remain available. |

`Ui3DViewOptions` contains `background`, `grid_color`, `selection_color`,
`edge_color`; `show_grid`, `show_axes`, `show_selection`, `lighting`;
`wireframe`, `solid_edges`; `adaptive_grid`, `grid_plane`, `grid_extent` and
`grid_spacing`. Decoration colours are sRGB bytes with alpha **255**. Extent is
1–100 cells each side; fixed spacing is finite 0.001–10000 metres.
`Light()` and `Dark()` return complete initial option values.

`Ui3DViewStats` exposes `rendered_frames`, `mesh_uploads`, `uploaded_bytes`,
`depth_rebuilds`, `resident_meshes`, `draws_last_frame`, `target_size` and `depth_active`.

### Scene and camera values

- Build meshes with `Scene3DMesh::Create(vertices, indices, error)` or
  `MakeBox(full_size, error)`. Factories validate and copy arrays; published data
  has no mutable alias. Reuse shared const meshes in later snapshots.
- A `Scene3DObjectDesc` contains stable `id`, optional `parent_id`, `local`
  transform, shared mesh, `visible`, `selectable` and linear RGBA. Transform-only
  parent nodes are supported. `Scene3DSceneDesc` contains a nonzero revision and
  ordered objects; `Scene3DSnapshot::Create` validates the complete candidate.
- Coordinates are **metres, right-handed, Y up**, with asset forward +Z. Matrices
  are column-major and multiply column vectors: `world = parent * local`.
  Camera yaw zero looks toward −Z. Angles are radians; screen origin is upper-left
  and projected depth is Vulkan 0–1. Do not add an extra OpenGL-style Y/depth conversion.
- `Scene3DCamera` exposes `target`, `yaw`, `pitch`, `distance`, `orthographic`,
  `fov_y`, `ortho_height`, `near_plane`, `far_plane` and `revision`.
  `Validate`, `GetEye`, `GetBasis`, `Project`, `WorldRay`, `Orbit`, `Pan`, `Dolly`,
  `Fit`, `AlignTo`, `SetOrthographic` and `GetViewDirection` provide the value API.
  Pan uses screen pixels; positive logarithmic dolly moves closer.
- `Scene3DCamera::AlignTo` changes pose only. The control's `SetViewDirection`
  additionally applies projection/grid policy. Projection conversion preserves
  `ortho_height = 2 * distance * tan(fov_y / 2)`. Fit expands clipping only when
  necessary; dolly preserves authored near/far planes. Neutral camera mutators
  advance revision only for actual changes; control/model revisions are separate.

## Events and selection

`WhenSelectionRequest(const Ui3DSelectionRequest&)` asks the application to decide.
Its `pick` contains `hit`, `id`, `world_point`, `world_normal`, `distance`,
`triangle`, scene/camera revisions, the scene binding and sampled viewport size.
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

This seam adds no manipulation drawing or renderer behavior. **Source-only:
new pointer-tool coverage has been authored in Ui3DViewTest but has not yet
been compiled or executed.** Earlier qualification counts below predate it.

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
3. **Picking is geometric.** It is two-sided CPU triangle picking with bounds
   rejection, including in wireframe. Nonselectable objects are skipped and do
   not block selection of selectable geometry behind them. It is not rendered-pixel,
   material-alpha, physics or contact picking.
4. **Fit after layout.** The control needs nonzero dimensions. `SetCamera` can
   trigger cancellation callbacks and advances the control revision; avoid
   camera feedback loops and do not equate its revision with your model revision.
5. **Rendering is currently opaque and untextured.** Vertex/object RGB is linear
   0–1 and alpha must be 1. UVs are retained but unused by this renderer. SolidEdges
   draws all solid depth before neutral edges; selection adds uniform mesh-edge
   highlighting without recolouring faces. Triangulation edges can be visible;
   this is not a silhouette or authored-quad edge extractor.
6. **Keep geometry local.** CPU calculations use doubles and GPU data uses
   float32. Large coordinates with small details can diverge visually from CPU
   picking. Validation budgets are not precision or frame-rate guarantees.
   Snapshot limits are 100,000 objects, 2 million instanced vertices and 6 million
   instanced indices. Per-view depth targets are limited to 16,777,216 pixels.
7. **Retain resources and invalidate deliberately.** Camera/selection changes do
   not reupload unchanged meshes. GPU buffers are cached per view, not shared
   across views merely because CPU meshes/context are shared. Adaptive grids use
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

Transform/manipulation gizmos, textures/transparency, import adapters,
deformation/skinning, accelerated picking, depth/normal/ID export and additional
backends require further implementation. The orientation control already exists.
The [HTML mockup](ui3dview-design/Ui3DView-Mockup.html) is a visual reference;
current headers and this guide describe the implemented contract.

## Build and run

The recorded Windows toolchain is C++17, `E:/upp-18468/umk.exe`,
`CLANGx64_Vulkan.bm` and Vulkan SDK `C:/VulkanSDK/1.4.350.0`.
The demo builds against `build/qualified-ui-main-text`, including its refreshed
UiIcons catalogue. Confirm those paths exist when moving to another machine.

```powershell
$ui3dRoot = 'E:/apps/github/upp_render'
$ui3dUmk = 'E:/upp-18468/umk.exe'
$ui3dMethod = "$ui3dRoot/CLANGx64_Vulkan.bm"
$ui3dNests = 'E:/apps/github/upp_render/render,E:/apps/github/upp_render/examples,E:/apps/github/upp_render/tests,E:/apps/github/upp_render/tools,E:/apps/github/upp_render/build/qualified-ui-main-text,E:/apps/github/upp_animation,E:/upp-18468/uppsrc'

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

These are observed results as of 1 November 2026, not tests rerun by this
documentation consolidation. Counts are per execution and all listed passes had
zero failures. Windows qualification covers the connected system at its session DPI.

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

The earlier Debug+BLITZ foreground/input gap is **closed**. Both current native
runs exercised caption activation and deliberately armed click suppression,
with **12 successful presses and 12 matching releases**. The harness now verifies
client motion, positions button events explicitly and cleans up only owned input.
Final runs: Release `exec-7BCAFBCE1865DDDCE2FE592467D8EC85`;
Debug+BLITZ `exec-23D2661FD1406845F05685B58F90FD23`.

### Current demo identity and limits

The installed **6,789,120-byte** demo is the icon-selector compile/link update:

```text
E:\apps\github\upp_render\bin\win64\Ui3DViewDemo.exe
SHA-256: 542e5bc30dc83cc95d0fd539df27764d77d5543bb09c9395da42e07ec497d15d
```

Its icon change was **compile-only**, as requested. The workspace/motion/capture
results above belong to the preceding flicker-fix demo
`2ec273141adc3ffea33e3625f69632ca3515e65c27b01945e91d815c9d145c57`.
The later native-harness correction did not rebuild or rerun the demo.

Other DPI scales, monitor transitions, other hardware, non-Windows hosting and
screen-reader behaviour are outside these runs. Core close/reopen tests do not
establish a separate facade close/reopen result. The overlay supports its fixed
annotation vocabulary; the motion probe does not sample every presented frame.

## Continuing development

Start from the headers linked above and the existing demo/test packages. Modify
the layer that owns the behaviour: scene mathematics in `RenderScene`, view
interaction in `Ui3DView.cpp`, 3D drawing in `Ui3DViewContent.cpp`, native UI in
`Ui3DViewport.cpp`, and catalogue/inspector/workspace policy in the demo or host.
Keep application models out of the reusable controls. Preserve immutable resource
ownership, callback lifetime guards, silent programmatic updates and bounded
native overlays. Add focused regression coverage for changed behaviour and
update this guide's API or tested-status section in place.
