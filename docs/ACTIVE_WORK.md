# Active Work Status

Remote `main` is authoritative for all active and accepted `upp_render` work. This repository is being developed directly on `main`; do not create supervisor feature/review branches unless Curt explicitly requests one for a genuinely isolated experiment.

## Current Baseline

Accepted UI1-A source baseline:

`67328ab9b420cbf81779481c2b2028d1cf539d27`

Latest acceptance/checkpoint commit before this workflow correction:

`ac682925bf92228a4125a1f6de734295067c6954`

The Vulkan productization baseline remains PASS / accepted:

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

## UI1-A — Representative Vulkan U++ UI — PASS

Validated and promoted to `main`:

- `GpuUiCoverageTest`: PASS — `rect=49 text=16 image=44 path=3 clip=35 transform=30`;
- `GpuUiGallery`: PASS — broad ordinary U++ controls through one root Vulkan surface;
- ordinary animated `Ctrl` remained live during interaction and resize;
- second modal `GpuTopWindow`: PASS;
- `GpuEmbeddedMotion`: PASS — normal U++ window with embedded `GpuCtrl`;
- `RenderCtrlBridgeTest`: PASS;
- `GpuTopWindowPresentationTest`: PASS;
- `GpuCtrlPresentationTest`: PASS;
- no Vulkan validation errors, crashes, assertions, blank surfaces or GPU errors;
- no source correction required during acceptance.

Popup/menu/tool-tip GPU presentation is not yet accepted.

## Active Objective — UI1-B + UI1-C

Continue directly on `main`.

### UI1-B — finish useful Draw semantics

- implement/test `DrawArc` with exact U++/Win32 semantics;
- implement/test rotated `DrawText` with exact U++ angle/origin semantics;
- keep native GDI, invert/XOR/pattern and child-window exclusion explicit unless real supported controls require them.

Known Draw-dispatch findings already verified:

- scaled/cropped/tinted image drawing resolves through `SysDrawImageOp`;
- `DrawDrawingOp` recursively replays into the recorder;
- `DrawPaintingOp` rasterizes through Painter and returns through the image path;
- `DrawDataOp` uses its `DataDrawer` and returns through the image path;
- these high-level operations do not need duplicate recorder implementations.

### UI1-C — transient and multi-window completion

- menus;
- context menus;
- DropList popup window behavior;
- tool-tips;
- additional top-level/dialog windows;
- representative `upp_Ui` gallery coverage;
- determine how U++-created transient native windows automatically acquire the GPU presentation path;
- preserve one root GPU compositor per top-level surface; never create one native GPU child per ordinary control.

### UI1-D — shared immutable resources

After UI1-B/C are stable:

- define context-owned image/glyph/vector resource identities;
- share immutable resources across compatible windows;
- prove one window can close without invalidating another;
- do not globally alias mutable presenter caches without an explicit lifetime contract.

### Backend Order After UI1

Prefer WebGPU before Metal if Windows/browser tooling gives faster executable evidence. WebGPU renderer/provider work remains separate from the browser/WebAssembly U++ platform host.

## Branch Policy

Current policy for this project:

- work directly on `main`;
- remote `main` is the source of truth;
- publish coherent checkpoints frequently;
- do not create `supervisor/*` branches for ordinary development/validation;
- create a branch only when Curt explicitly asks for one or when a genuinely isolated experiment must not touch `main`.

All existing `supervisor/*` branches are obsolete and should be deleted. In particular, `supervisor/ui1bc-transient-ui` was created unnecessarily and must not be used.

## Recovery Log

BASE: current remote `main`
TASK: UI1-B + UI1-C — complete useful Draw semantics and transient/multi-window Vulkan U++ UI behavior
WORKING BRANCH: `main`
STATUS: UI1-A PASS / ACCEPTED / ON MAIN
VALIDATION: representative control recording, root GPU gallery, modal second GPU top-level, embedded `GpuCtrl`, and bridge/presentation regressions PASS; no Vulkan validation errors
NEXT: delete all remaining `supervisor/*` branches; continue UI1-B/C directly on `main`.
