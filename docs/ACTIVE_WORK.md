# Active Work Status

This file is the recovery checkpoint for active `upp_render` implementation work.
Update it whenever a coherent checkpoint is published so work can resume from repository state rather than chat history.

## Accepted Baseline

- Branch: `main`
- Stage 3 Vulkan backend/bootstrap: **PASS / 100% accepted**
- Stage-3 Windows acceptance HEAD: `6ab33a42a3421643359cabfdae7afed7628ad349`
- Stage 4 GPU 2D renderer: **PASS / 100% accepted**
- Stage-4 Windows acceptance HEAD: `f8e7b24d510b4b5889370823dc1c0a5ef43a7f54`
- Stage-5 image foundation: **PASS / accepted**
- Stage-5 image implementation: `a11862d138e6b2f06d92067b4b804d8418b69d32`
- Stage-5 image Windows acceptance HEAD: `f2cd2bdf2ff7c05f7b883ef32405653ab198a98b`
- Stage-5 text/glyph-atlas implementation: `f98cce413b1992cfaef55669d4672824fe703b5f`
- Stage-5 text Windows acceptance HEAD: `91f1fe3cad91b5afe00de4afd6398b773e8f4715`
- TASK-010B-W1: **PASS** — text/software/Null Debug+Release PASS; Vulkan text Debug 4/4 + Release 2/2; GPU2D/image/GpuCtrl regressions PASS; validation 0/0; adapter resources 0; Vulkan ownership `0/0/0/0/0/0`
- TASK-010B-W1 mechanical source corrections: `6f322ad4ecc4b2364d020a00bb3676695cbc9cab`

## Current Objective

Stage 5 is **IMPLEMENTATION COMPLETE — FINAL WINDOWS/VULKAN ACCEPTANCE PENDING**.
Stage 6 U++ integration is active in parallel so validation latency does not stall implementation.

Active Stage-5 validation: `TASK-010-W1` — final vector/gradient/AA/SVG plus image/text regression acceptance.
Active Stage-6 validation: `TASK-011A-W1` — shared presenter + root `GpuTopWindow` Windows/Vulkan acceptance.
Active Stage-6 implementation: `TASK-011B` — CtrlCore semantic `DrawCtrl` recording bridge is published; next slice wires that bridge into the root `GpuTopWindow` compositor.

## Stage 5 - Text, Images and Vector Rendering

### Images — accepted

Production scope includes immutable `DrawImage`, deterministic software replay, sampled-texture RHI/Null authority, sRGB uploads, affine UV clipping, ordered image/solid batching, renderer image caching and real Vulkan offscreen/swapchain presentation.

### Text — accepted

Production scope includes immutable `DrawText`, fractional U++ software replay, U++-authoritative glyph metrics/fallback/composition, persistent sampled glyph atlas pages, padded partial uploads, affine clipping, text tint/alpha, ordered text/image/solid batching and cross-frame atlas/buffer reuse.

Boundary honesty: current U++ does not expose a HarfBuzz-grade complex-script shaper in the inspected text path, so Stage 5 does not claim one. U++ remains font/text authority.

### TASK-010C-A — neutral vector + U++ Painter authority

Published: `e6367d8e72eea4803a3585680674c79784f52bef`

Scope:
- copy-safe backend-neutral `UiPath` Move/Line/Quadratic/Cubic/Close;
- NonZero/EvenOdd fill rules;
- solid, multi-stop linear/radial paints with Pad/Repeat/Reflect spreads;
- stroke width, caps, joins, miter, dash pattern/offset;
- immutable FillPath/StrokePath/DrawSvg recording and deterministic dumps;
- shared `RenderVector` U++ Painter semantic/raster authority;
- U++ curves, gradients, stroke style and SVG replay;
- antialiased raster helper with conservative bounds, malformed-stream rejection and 4096-pixel safety cap;
- `RenderSoftware` uses the same vector authority;
- focused `RenderVectorTest`.

### TASK-010C-B — production GPU vector/SVG path

Published: `0d37b2472c4d49e6908f6acbf5f85cc523193006`
Status: **IMPLEMENTATION COMPLETE — PLATFORM VALIDATION PENDING**

Production design deliberately avoids a second GPU vector/resource authority:
- vector/SVG ops are rasterized by U++ Painter into cached antialiased `Image` values;
- raster scale follows the largest singular value of the active affine transform, clamped 1..8;
- vector cache identity is exact `UiDisplayOp` value equality + raster scale;
- materialized vector operations become ordinary `DrawImage` operations;
- the accepted image path owns GPU upload, sRGB conversion, texture cache, descriptors, affine UV clipping, batching and destruction;
- vector sidecar owns CPU Images only and owns zero Vulkan/GpuTexture resources;
- mixed vector + text scenes preserve original order and reuse both image and glyph caches;
- no-vector display lists retain the accepted prior renderer path unchanged.

Coverage:
- `RenderGpuVectorTest`: first-frame vector raster/image uploads, second-frame zero raster/upload work, mixed vector/text ordering, glyph reuse, RenderNull cleanup;
- `RenderVulkanVectorTest`: real Vulkan offscreen vector/text rendering, cached repeat, acquired swapchain presentation, zero adapter resources and final ownership diagnostics;
- `RenderVectorTest`: curves, fill rules, Pad/Reflect/Repeat gradients, stroke styles, SVG and explicit partial-alpha antialias evidence.

Native GPU path tessellation remains a Stage-8 optimization candidate. For v1, U++ Painter is the semantic authority and the accepted sampled-image pipeline is the GPU transport.

## Stage 6 - U++ Integration

Architecture constraints:
- one root GPU/native presentation boundary for a GPU-composited interface; do not create one native child per ordinary control;
- U++ retains control tree, layout, input, focus, state and theme authority;
- controls emit resolved neutral drawing intent/display lists;
- software replay remains the correctness/fallback path;
- `GpuCtrl` remains appropriate for explicitly embedded accelerated content, not as the implementation mechanism for every ordinary control.

### TASK-011A — first root-composition boundary

Published on `main`: `a4979f17becfb4af6390314cc316eb1ea31e3c92`
PR: `#22`
Status: **IMPLEMENTATION COMPLETE — PLATFORM VALIDATION PENDING**

Implemented scope:
- backend-neutral `RenderPresentation` package owns selected backend session, `UiRenderer2D`, logical surface/swapchain and frame-present lifecycle;
- Vulkan-specific ownership remains private to `RenderPresentation.cpp`; public presentation API contains no Vulkan types;
- `GpuCtrl` uses `GpuDisplayPresenter`, removing its duplicate private Vulkan/session/swapchain/render lifecycle while retaining DHCtrl hosting for explicitly embedded GPU content;
- `GpuTopWindow` binds `GpuDisplayPresenter` directly to the real top-level U++ HWND and creates no native child host;
- `GpuTopWindow::BuildGpuFrame()` is the first root neutral-display-list composition hook; U++ remains window/input/layout authority;
- root WM_PAINT presents one neutral frame through one Vulkan surface/swapchain; ordinary TopWindow handling remains fallback while GPU is unavailable;
- `GpuTopWindowPresentationTest` covers one live surface/device/swapchain, idle stability, refresh, resize, hide/show and zero final Vulkan ownership.

Still required before TASK-011A acceptance:
- Windows Debug/Release compile/run of `GpuTopWindowPresentationTest`;
- root Vulkan lifecycle evidence with validation 0/0 and final ownership zero;
- `GpuCtrlPresentationTest` regression after shared-presenter migration.

### TASK-011B — embedded neutral frame source

Published on `main`: `3ac69f1971b5770b08ab9d06b7be72654dabe521`
Status: **IMPLEMENTATION COMPLETE — PLATFORM VALIDATION PENDING**

- `GpuCtrl::WhenBuildFrame` optionally accepts a caller-produced immutable neutral `UiDisplayList` plus background;
- callback owns drawing intent only; native host, presentation session, surface/swapchain and renderer ownership stay in `GpuCtrl` / `GpuDisplayPresenter`;
- unset callback preserves the existing default reference scene exactly;
- invalid callback output is rejected before presentation.

### Renderer Showcase / consolidated acceptance

Published on `main`: `094e8807c70fd591bf7e921a5a98ae7069a8b97f`
Source head: `a8d873c987418a183515885751ea0c742edef3aa`
PR: `#25`
Status: **IMPLEMENTATION COMPLETE — PLATFORM VALIDATION PENDING**

Reference UI revision inspected while designing the showcase:
- `Trilec/upp_Ui` `main`: `b3375564dff21c124374472aabfd79d62ef0d51e`
- visual/layout reference: `examples/UiLabelDemo`
- PropertyEditor rules: `Utilities/PropertyEditor/DESIGN.md` and `README.md`

Implemented design:
- `examples/RendererShowcaseScene` is the single scene authority and depends only on Core/Draw/RenderCanvas;
- the shared scene records fills, rectangle strokes, rounded geometry, Save/Restore, clipping, affine transform, source alpha, sampled image, text, reflected multi-stop gradient vector fill, dashed round vector stroke and SVG;
- `examples/RendererShowcase` is a deliberately light developer-facing window inspired by the UiLabel demo: `UiTitleCard` header/title line, status + GPU/Software/Reset/Exit buttons, large preview area, right PropertyEditor rail;
- PropertyEditor remains the single authored interactive state with `Renderer`, `Content`, `Appearance` and `Geometry` groups;
- live controls are intentionally bounded: renderer mode, text/font size, image/SVG visibility, accent colour, opacity, corner radius, scale, rotation and clipping;
- GPU preview uses `GpuCtrl::WhenBuildFrame`; software preview uses `SoftwareUiRenderer`; both call the exact same `BuildRendererShowcaseScene()` function;
- `tests/RendererShowcaseTest` consumes that same shared scene, checks every broad capability op, deterministic dump, software visible output/immutability, and an alternate interactive property projection;
- focused tests remain in place for failure diagnosis; this showcase/test is the broad capability and developer-facing acceptance surface.

Dependency boundary:
- no `Ui` or PropertyEditor dependency was added to RenderCore/RenderCanvas/RenderGpu2D/RenderRhi/RenderVulkan;
- only the interactive example depends on the external `upp_Ui` repository;
- console showcase test and shared scene remain renderer-repo-only.

Windows build assembly notes:
- `RendererShowcaseTest`: include `tests,render,examples,E:\upp-18468\uppsrc`;
- interactive `RendererShowcase`: include `examples,render,E:\apps\github\upp_Ui,E:\upp-18468\uppsrc` so `Ui` and `Utilities/PropertyEditor` resolve from the live UI repo.

### TASK-011B — CtrlCore semantic recording bridge

Published on `main`: `c4210d80a815950df53df5db9dea45a38edbbfdd`
Source head: `30ef093f4035eff923594bc6408ae17292a8512f`
PR: `#27`
Status: **IMPLEMENTATION COMPLETE — PLATFORM VALIDATION PENDING**

Implemented scope:
- new `render/RenderCtrlBridge` production package depends on `CtrlCore` + `RenderCanvas`, not CtrlLib, Ui, Vulkan or platform APIs;
- public `RecordCtrlDisplayList()` supplies a recording Win32 `SystemDraw` to public `Ctrl::DrawCtrl()`, so U++'s own private `CtrlPaint` path remains the recursive control/frame/layout/theme authority;
- no duplicate Ctrl-tree traversal, private `CtrlPaint` access, HDC-backed target, native-child-per-control mechanism or second theme/layout model is introduced;
- U++ 2026.1 normally routes Win32 painting through `BackDraw`; the bridge uses a harmless `FULLBACKPAINT` probe to observe the otherwise write-only `GlobalBackBuffer` state, enables direct drawing only when needed, and restores the exact inherited state under the GUI lock;
- neutral `SystemDraw` translation maps U++ Begin/End, Offset, Clip/Clipoff/intersect, rectangles, images including U++ tint semantics, text, lines/polylines, disjunct polygons with EvenOdd fill, and ellipses into existing immutable `UiDisplayList` operations;
- Offset/Clip/Clipoff open neutral Save state and are paired with U++ End semantics; polygon grouping/counts are validated before recording;
- unsupported exclusion clips, native SystemDraw/GDI access, invert/XOR/pattern drawing, arcs and rotated text fail explicitly instead of being silently dropped;
- `tests/RenderCtrlBridgeTest` uses a real CtrlFrame plus real CtrlLib Label/Button and a custom painted child, checks recursive clip/offset state, text/image/vector intent, disjunct polygon grouping, deterministic repeat and software replay;
- focused Win32 evidence checks both inherited `GlobalBackBuffer(false)` and `GlobalBackBuffer(true)` states are preserved, and checks explicit failure for native drawing, native child-window exclusion and DrawArc.

Boundary before root wiring:
- this checkpoint records ordinary resolved Win32 U++ semantic control painting only;
- exclusion clips/native child surfaces, raw native drawing, arcs and rotated DrawText remain explicit unsupported boundaries;
- `GpuTopWindow` has not yet been changed to consume this bridge in this checkpoint.

## Recovery Log

BASE: `c4210d80a815950df53df5db9dea45a38edbbfdd` / `main`
TASK: `TASK-011B` root compositor wiring from published CtrlCore semantic recording bridge
TOUCHED: published recorder — `render/RenderCtrlBridge/*`, `tests/RenderCtrlBridgeTest/*`; recovery status — `docs/ACTIVE_WORK.md`
STATUS: Stage 3 PASS; Stage 4 PASS; Stage-5 images/text PASS; Stage-5 vector IMPLEMENTATION COMPLETE / PLATFORM VALIDATION PENDING; TASK-011A IMPLEMENTATION COMPLETE / PLATFORM VALIDATION PENDING; Renderer Showcase IMPLEMENTATION COMPLETE / PLATFORM VALIDATION PENDING; CTRL RECORDING IMPLEMENTATION COMPLETE / PLATFORM VALIDATION PENDING; ROOT WIRING NEXT
PUBLISHED: Ctrl recording `c4210d80a815950df53df5db9dea45a38edbbfdd` via PR `#27`; earlier checkpoints unchanged
VALIDATION: full aggregate PR/dependency/API/source review complete against U++ 2026.1 paint/Draw semantics; remote main and squash commit independently verified; Windows compile/runtime pending

## Next Action

Validate `RenderCtrlBridgeTest` on Curt's Windows U++ 2026.1 environment. In parallel, inspect the published `GpuTopWindow` root frame hook and implement the smallest coherent wiring that feeds its real resolved U++ control tree through `RecordCtrlDisplayList()` into the existing presenter, preserving fallback behavior and the single-root-surface architecture. Keep `RendererShowcase` as the broad visual acceptance surface and focused tests for diagnosis.
