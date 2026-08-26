# Active Work Status

Remote `main` is authoritative for all active and accepted `upp_render` work. This repository is developed directly on `main`; do not create supervisor feature/review branches unless Curt explicitly requests one for a genuinely isolated experiment.

## Current Baseline

Accepted UI1-A source baseline:

`67328ab9b420cbf81779481c2b2028d1cf539d27`

Main-only workflow checkpoint:

`70c28f7660214af3d7edcf8fbb3be7601d3c61b3`

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
- CtrlCore semantic recording bridge: PASS for the validated Draw contract;
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
- no Vulkan validation errors, crashes, assertions, blank surfaces or GPU errors.

Popup/menu/tool-tip GPU presentation is not yet accepted.

## UI1-B — Useful Draw Semantics — IMPLEMENTED / WINDOWS VALIDATION PENDING

Current source/test checkpoint:

`79783630d320f88b38f99bc379e5520ded1587f6`

Published directly on `main`:

- `DrawArc` now records to the existing neutral `UiPath` / `StrokePath` contract instead of failing;
- the arc path follows Win32/U++ default counter-clockwise semantics and converts the radial start/end points to the ellipse intersection angles;
- arcs are split into <= 90-degree cubic Bézier segments; equal start/end draws a full ellipse as Win32 documents;
- existing U++ pen widths/dash constants continue through `ConfigureStroke`;
- rotated `DrawText` now remains semantic text: a local neutral transform is applied around U++'s `(x,y)` origin using U++'s tenth-degree angle convention, followed by normal `DrawText` operations;
- no raster fallback was added for text;
- native GDI, child-window exclusion, invert/XOR/pattern modes remain explicit boundaries.

Focused `RenderCtrlBridgeTest` now checks:

- ordinary recursive control recording remains deterministic;
- arc recording succeeds and produces a neutral stroke path;
- the right-to-left half-ellipse follows the Win32/U++ counter-clockwise/top-half direction;
- software replay produces visible arc output;
- 90-degree rotated text records as neutral transform + text operations;
- software replay produces visible rotated text output;
- native GDI and native-child exclusion still fail explicitly.

Platform compile/runtime validation of this checkpoint is still required before UI1-B is marked PASS.

## UI1-C — Transient / Multi-window Completion — ACTIVE NEXT BLOCK

Upstream U++ popup architecture inspection has started.

Important finding:

- standard `DropList` uses `PopUpList::Popup`, which is an internal `Ctrl` opened as its own native popup window via `Ctrl::PopUp`;
- it is therefore not part of the parent `GpuTopWindow` root surface;
- making transient UI fully GPU-presented requires a per-native-top-level/popup presentation decision, not another child-control compositor and not one GPU HWND per ordinary control.

Next work:

- inspect the Win32 `Ctrl::PopUp` / native-window creation seam and choose the smallest reusable way for U++-created popup windows owned by a GPU top-level to acquire GPU presentation;
- cover DropList popup, menus/context menus and tool-tips explicitly;
- retain ordinary separate `GpuTopWindow` dialogs as the already validated multi-top-level model;
- broaden to representative `upp_Ui` controls after the transient-window seam is proven.

## UI1-D — Shared Immutable Resources

After UI1-B/C are stable:

- define context-owned image/glyph/vector resource identities;
- share immutable resources across compatible windows;
- prove one window can close without invalidating another;
- do not globally alias mutable presenter caches without an explicit lifetime contract.

## Backend Order After UI1

Prefer WebGPU before Metal if Windows/browser tooling gives faster executable evidence. WebGPU renderer/provider work remains separate from the browser/WebAssembly U++ platform host.

## Branch Policy / Cleanup

Current policy:

- work directly on `main`;
- remote `main` is the source of truth;
- publish coherent checkpoints frequently;
- do not create `supervisor/*` branches for ordinary development/validation.

All previously created `supervisor/*` branches were deleted after UI1-A acceptance. No supervisor branch is required for current work.

## Recovery Log

BASE: current remote `main`
TASK: UI1-B + UI1-C — complete useful Draw semantics and transient/multi-window Vulkan U++ UI behavior
WORKING BRANCH: `main`
STATUS: UI1-A PASS / ACCEPTED; UI1-B source implementation published, Windows validation pending; UI1-C popup architecture audit active
SOURCE CHECKPOINT: `79783630d320f88b38f99bc379e5520ded1587f6`
VALIDATION: previous UI1-A and Vulkan productization acceptance remain authoritative; no UI1-B runtime PASS claimed yet
NEXT: Windows validate `RenderCtrlBridgeTest` plus UI1 gallery regression; meanwhile continue transient-window design from U++ `Ctrl::PopUp`/Win32 creation path.
