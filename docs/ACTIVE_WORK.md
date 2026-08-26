# Active Work Status

Remote `main` is authoritative for accepted/published state. This file is the recovery checkpoint for active `upp_render` work.

## Accepted Baseline

Current accepted `main`: `67328ab9b420cbf81779481c2b2028d1cf539d27`.

The Vulkan productization baseline remains **PASS / accepted**:

- backend-neutral display-list/software foundation: PASS;
- RHI + Null backend: PASS;
- Vulkan bootstrap/resources/presentation: PASS;
- GPU 2D geometry/clipping/transforms: PASS;
- images: PASS;
- text/glyph atlas: PASS;
- vector/gradient/AA/SVG: PASS;
- shared presenter/root boundary: PASS;
- embedded `GpuCtrl` frame source: PASS;
- CtrlCore semantic recording bridge: PASS for the currently supported Draw contract;
- `GpuTopWindow` root compositor: PASS;
- backend registry/decoupling: PASS;
- compatible Vulkan surfaces share runtime/instance/logical device/queue domain and pipeline cache while retaining independent surface/swapchain/frame state: PASS;
- Renderer Showcase final smoke: PASS;
- final live Vulkan ownership: ZERO.

Productization source acceptance: `7e9b5cdacf76acef56da4943a228f0281d1dde95`.
Productization acceptance-doc checkpoint: `feab4d9a0f24bed8f9226ff169aabf5ef565a8d8`.

## UI1 — Complete Vulkan U++ Experience

Priority is the complete Windows/Vulkan U++ UI story before another backend.

Required end state:

1. broad ordinary U++/upp_Ui control trees render through one root Vulkan surface;
2. common Draw semantics are covered; remaining unsupported semantics are rare/legacy/native boundaries and explicit;
3. multiple `GpuTopWindow` top-level/dialog surfaces coexist and share the application GPU context/device domain;
4. popup/dropdown/menu/tool-tip behavior is explicitly handled and validated rather than inferred;
5. ordinary `TopWindow` + embedded `GpuCtrl` remains a first-class bounded accelerated-content path;
6. shared immutable resources evolve only through explicit identity/lifetime rules.

## UI1-A — Representative Gallery / Acceptance — PASS

Validated and promoted to `main` at:

`67328ab9b420cbf81779481c2b2028d1cf539d27`

Added:

- `examples/GpuUiGallery` — `GpuTopWindow` with Button, Option, EditString, DropList, SliderCtrl, ProgressIndicator, ArrayCtrl, modal `GpuTopWindow`, and an animated ordinary U++ `Ctrl` rendered through the root recorder/compositor;
- `examples/GpuEmbeddedMotion` — ordinary `TopWindow` with normal U++ controls around an animated embedded `GpuCtrl`;
- `tests/GpuUiCoverageTest` — representative standard-control recording + software replay authority.

Windows/Vulkan UI1-A acceptance:

- `GpuUiCoverageTest`: PASS — `rect=49 text=16 image=44 path=3 clip=35 transform=30`;
- `GpuUiGallery`: Debug build/run PASS;
- ordinary-control interactions exercised: EditString, Option, DropList, Slider, ProgressIndicator, Apply, ArrayCtrl;
- calm-particle ordinary `Ctrl` animation remained active during interaction/resize;
- modal second `GpuTopWindow` open/close exercised; parent remained live;
- `GpuEmbeddedMotion`: Debug build/run PASS; pause/resume/edit/resize exercised;
- `RenderCtrlBridgeTest`: PASS;
- `GpuTopWindowPresentationTest`: PASS;
- `GpuCtrlPresentationTest`: PASS;
- no Vulkan validation errors, crashes, assertions, blank surfaces or GPU errors;
- no source corrections were needed.

Accepted labels:

- **UI1-A REPRESENTATIVE CONTROL RECORDING: PASS**
- **UI1-A ROOT GPU UI GALLERY: PASS**
- **UI1-A MULTI-TOPLEVEL GPU DIALOG: PASS**
- **UI1-A EMBEDDED GPU CONTROL: PASS**

Popup/menu/tool-tip GPU composition is **not yet claimed**. Visibility of a native popup is not proof that it is root-composited or GPU-presented.

## Recorder Gap Audit

Known explicit boundaries in `RenderCtrlBridge`:

- `DrawArc`;
- rotated `DrawText`;
- `ExcludeClip` / native child-window cutouts;
- native `BeginNative` / direct GDI drawing;
- invert/XOR/pattern legacy drawing modes.

Full upstream U++ `Draw` dispatch was inspected:

- scaled/cropped/tinted image drawing already resolves through `SysDrawImageOp`;
- default `DrawDrawingOp` recursively replays into the recorder's virtual operations;
- default `DrawPaintingOp` rasterizes through Painter and returns through the image path;
- default `DrawDataOp` uses its `DataDrawer` and returns through the image path;
- those high-level operations do not need duplicate recorder implementations.

`Escape` remains an extension/no-op boundary to audit against real usage before supporting a specific extension.

Do not implement obscure semantics merely for percentage coverage. Prioritize real U++/upp_Ui output. Native/GDI and child-HWND exclusion are architectural boundaries, not just missing primitives.

## Next Heavy Block — UI1-B + UI1-C

Proceed without another planning-only cycle:

### UI1-B — finish useful neutralizable Draw semantics

- implement and test `DrawArc` with exact U++/Win32 direction semantics;
- implement and test rotated `DrawText` with exact U++ angle/origin semantics;
- keep invert/XOR/pattern/native GDI explicit unless a real supported control requires them.

### UI1-C — transient/multi-window U++ UI completion

- broaden the gallery to menus, context menus, DropList popup behavior, tool-tips and multiple top-level/dialog windows;
- determine how U++-created transient native windows automatically acquire the GPU presentation path;
- add representative `upp_Ui` control-gallery coverage;
- preserve one root GPU compositor per top-level surface; do not create a GPU child for each ordinary control.

### UI1-D — shared immutable resources

After UI1-B/C are stable, define context-owned identities/lifetimes for shareable images/glyph atlases/vector resources and validate cross-window reuse without coupling surface lifetimes.

### Backend Order After UI1

Prefer WebGPU before Metal if Windows/browser tooling provides faster executable evidence. WebGPU renderer work remains separate from the browser/WebAssembly U++ platform host.

## Branch Hygiene

UI1-A is now on `main`; `supervisor/ui1-vulkan-ui-gallery` is obsolete and may be deleted.

The old productization branches `supervisor/h1-provider-boundary`, `supervisor/h1b-final-cleanup`, `supervisor/p6a-shared-device`, and `supervisor/p6b-pipeline-cache` are superseded development histories. Their intended work was incorporated and subsequently accepted through the later P6/H1/W2-R1 `main` history; their current tips are not authoritative and must not be merged back.

## Recovery Log

BASE: `67328ab9b420cbf81779481c2b2028d1cf539d27` / `main`
TASK: UI1-B + UI1-C — finish useful Draw semantics and transient/multi-window Vulkan U++ UI behavior
BRANCH: none yet; create a fresh branch from current `main`
STATUS: **UI1-A PASS / ACCEPTED / ON MAIN**
VALIDATION: representative control recording, root GPU gallery, modal second GPU top-level, embedded GpuCtrl and baseline bridge/presentation regressions PASS; no Vulkan validation errors
NEXT: delete superseded supervisor branches; create a fresh UI1-B/C branch from current `main`; implement arcs + rotated text and immediately continue into popup/menu/tool-tip/upp_Ui integration.
