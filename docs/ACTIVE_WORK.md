# Active Work Status

Remote `main` is authoritative for accepted/published state. This file is the recovery checkpoint for active `upp_render` work.

## Accepted Baseline

Accepted `main` before UI1: `feab4d9a0f24bed8f9226ff169aabf5ef565a8d8`.

The Vulkan productization baseline is **PASS / accepted**:

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
- `GpuTopWindow` root compositor: PASS for the accepted representative control slice;
- backend registry/decoupling: PASS;
- compatible Vulkan surfaces share runtime/instance/logical device/queue domain and pipeline cache while retaining independent surface/swapchain/frame state: PASS;
- Renderer Showcase final smoke: PASS;
- final live Vulkan ownership: ZERO.

Accepted productization source head: `7e9b5cdacf76acef56da4943a228f0281d1dde95`.
Final acceptance-doc checkpoint: `feab4d9a0f24bed8f9226ff169aabf5ef565a8d8`.

## Active Objective — UI1 Complete Vulkan U++ Experience

Priority is now the complete Windows/Vulkan U++ UI story before another backend.

Required end state:

1. `GpuTopWindow` can host a broad ordinary U++/upp_Ui control tree through one root Vulkan surface.
2. Standard control painting does not fail on common Draw semantics; remaining unsupported semantics must be rare/legacy/native boundaries and explicitly documented.
3. A separate `GpuTopWindow` modal dialog can coexist with the parent and reuse the application GPU context/device domain.
4. Popup/dropdown/menu/top-level behavior is audited explicitly; automatic GPU treatment for U++-created popup windows must not be claimed until it is implemented and validated.
5. Ordinary `TopWindow` + embedded `GpuCtrl` remains a first-class path for bounded accelerated content surrounded by normal U++/GDI controls.
6. Add convincing low-cost demos rather than isolated probes.
7. Expand shared image/glyph/resource ownership only with explicit identity/lifetime rules; do not globally alias mutable renderer caches.

## UI1-A — Gallery / Acceptance Slice

Active remote branch:

`supervisor/ui1-vulkan-ui-gallery`

Published implementation checkpoint:

`cb1a2ab6b7470c7839add8b73b09b72911b38e55`

Subsequent branch documentation checkpoints include:

- `bc82f2140502d4676b900224608f4d07bf352a4c` — UI1 recovery checkpoint;
- `7ff51bcd01b7284a78464711e8da0c9d46af7bff` — example index;
- `86866b7eea2cc3d1dff1e3d0c2ab1c1ee5b50d86` — project plan reprioritized around complete Vulkan UI, then shared resources, then WebGPU/Metal.

Added:

- `examples/GpuUiGallery` — `GpuTopWindow` with Button, Option, EditString, DropList, SliderCtrl, ProgressIndicator, ArrayCtrl, a modal `GpuTopWindow` dialog, and an animated custom U++ `Ctrl`. The animated panel uses only ordinary `Draw` calls; successful GPU presentation therefore proves the root recorder/compositor path rather than a direct Vulkan widget.
- `examples/GpuEmbeddedMotion` — ordinary `TopWindow` with normal U++ controls around an animated `GpuCtrl`. The bounded panel uses `GpuPainter`; it is the separate embedded-surface use case.
- `tests/GpuUiCoverageTest` — representative standard-control tree recording + software replay authority. It must fail with the exact unsupported operation if a common control hits a missing Draw semantic.

The animation is intentionally light: a small deterministic calm-particle field at about 30 Hz using circles/rounded rectangles, simple grid lines and text. It is intended to remain useful on low-end GPUs and makes resize/redraw stalls obvious.

## Recorder Gap Audit

Current known explicit unsupported boundaries in `RenderCtrlBridge`:

- `DrawArc`;
- rotated `DrawText`;
- `ExcludeClip` / native child-window cutouts;
- native `BeginNative` / direct GDI drawing;
- invert/XOR/pattern legacy drawing modes.

Full upstream U++ `Draw` dispatch was inspected before adding duplicate adapter code:

- scaled/cropped/tinted `DrawImageOp` already resolves through `SysDrawImageOp`, which the recorder implements;
- default `DrawDrawingOp` recursively replays the serialized `Drawing` into the recorder's existing virtual operations, preserving its scaling/clip semantics;
- default `DrawPaintingOp` rasterizes through Painter and feeds the result back through the recorder's image path;
- default `DrawDataOp` uses the registered `DataDrawer` and feeds the rendered result back through the image path;
- those high-level operations therefore do **not** need duplicate recorder implementations merely to claim coverage.

`Escape` remains an extension/no-op boundary and should be audited against real control usage before deciding whether to reject it explicitly or support a specific registered extension.

Do not implement obscure semantics merely to reach a numeric percentage. First run the representative gallery/coverage test and prioritize operations actually emitted by U++/upp_Ui controls. Native/GDI and child-HWND exclusion remain architectural boundaries rather than missing drawing primitives.

## Near-Term Sequence

### UI1-A

Build/run the new gallery, embedded-motion demo and coverage test on Windows. Capture the first real unsupported Draw operation, if any.

### UI1-B

Implement the common missing recorder semantics revealed by UI1-A. Independently add low-risk support for broadly useful neutralizable operations such as arcs and rotated text once their U++ direction/angle semantics are preserved exactly.

### UI1-C

Broaden gallery/acceptance coverage to dialogs, multiple GPU top-level windows, menus/dropdowns/popups and a representative upp_Ui control gallery. Distinguish root-composited content from separately-created native popup windows.

### UI1-D

Resource evolution: define explicit context-owned immutable resource identities for shareable images/glyph atlases before moving those caches out of presenters. Validate two windows sharing resources without coupling surface lifetimes.

### Backend Order After UI1

Prefer WebGPU before Metal if Windows/browser tooling gives us executable evidence faster. The backend registry/RHI seam already supports this direction. A browser/WebAssembly U++ host remains a separate platform task from the WebGPU renderer provider.

## Recovery Log

BASE: `feab4d9a0f24bed8f9226ff169aabf5ef565a8d8` / `main`
TASK: UI1 complete Vulkan U++ UI experience
BRANCH: `supervisor/ui1-vulkan-ui-gallery`
SOURCE CHECKPOINT: `cb1a2ab6b7470c7839add8b73b09b72911b38e55`
LATEST PRE-DOC BRANCH CHECKPOINT: `86866b7eea2cc3d1dff1e3d0c2ab1c1ee5b50d86`
TOUCHED/INSPECTED: new `GpuUiGallery`, `GpuEmbeddedMotion`, `GpuUiCoverageTest`; `RenderCtrlBridge`; `GpuTopWindow`; `GpuCtrl`; upstream U++ `Draw`, `Drawing`, `DrawData`, Painter dispatch; examples/project plan
STATUS: UI1-A implementation/docs published on remote branch; complete upstream Draw dispatch audit narrowed the actual semantic gap list; Windows compile/runtime and first real control-semantic result pending
VALIDATION: previous Vulkan productization W2-R1 PASS remains accepted baseline; no UI1 validation claimed yet
NEXT: Windows build `GpuUiCoverageTest`, `GpuUiGallery`, `GpuEmbeddedMotion`; report exact first unsupported Draw semantic or PASS; then supervisor implements UI1-B immediately and proceeds to popup/dialog/multi-window UI1-C.
