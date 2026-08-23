# Active Work Status

This file is the recovery checkpoint for active `upp_render` implementation work.
Update it whenever a coherent checkpoint is published so work can resume from repository state rather than chat history.

## Accepted Baseline

- Branch: `main`
- Stage 3 Vulkan backend/bootstrap: **PASS / 100% accepted**
- Stage-3 Windows acceptance HEAD: `6ab33a42a3421643359cabfdae7afed7628ad349`
- Stage 4 GPU 2D renderer: **PASS / 100% accepted**
- Stage-4 Windows acceptance HEAD: `f8e7b24d510b4b5889370823dc1c0a5ef43a7f54`
- Vulkan framebuffer orientation correction: **PASS / accepted**
- Orientation implementation: `c13783aaad1ce10d4ade5ac8f020c56e876ae5f8`
- Orientation Windows revalidation HEAD: `42d74c7bf44bac5f9ce8c92a3e553946943b8738`
- Stage-5 image foundation: **PASS / accepted**
- Stage-5 image implementation: `a11862d138e6b2f06d92067b4b804d8418b69d32`
- Stage-5 image Windows acceptance HEAD: `f2cd2bdf2ff7c05f7b883ef32405653ab198a98b`
- Stage-5 text/glyph-atlas implementation: `f98cce413b1992cfaef55669d4672824fe703b5f`
- Stage-5 text Windows acceptance HEAD: `91f1fe3cad91b5afe00de4afd6398b773e8f4715`
- TASK-010B-W1: **PASS** — text/software/Null Debug+Release PASS; Vulkan text Debug 4/4 + Release 2/2; GPU2D/image/GpuCtrl regressions PASS; validation 0/0; adapter resources 0; Vulkan ownership `0/0/0/0/0/0`
- TASK-010B-W1 mechanical source corrections: `6f322ad4ecc4b2364d020a00bb3676695cbc9cab`
- TASK-011A shared presenter/root presentation boundary: **PASS / accepted**
- TASK-011A implementation: `a4979f17becfb4af6390314cc316eb1ea31e3c92`
- TASK-011A Windows acceptance HEAD: `cb01a20a283ac18e07121a94ccc90bc3d232d8cf`
- TASK-011B embedded neutral frame source: **PASS / accepted**
- TASK-011B frame-source implementation: `3ac69f1971b5770b08ab9d06b7be72654dabe521`
- TASK-011B focused acceptance package: `947a06038bddb6fd116b00aff1ac697a79eab55b`
- TASK-011B frame-source Windows acceptance HEAD: `e3ad497b0bb0d001287eddd2d90e0fae861e00c7`
- TASK-011B CtrlCore semantic recording bridge: **PASS / accepted**
- TASK-011B implementation: `c4210d80a815950df53df5db9dea45a38edbbfdd`
- TASK-011B Windows acceptance HEAD: `d386ba1aa954ea8d16a58a35170fa9f722be1e78`
- TASK-011C root compositor wiring: **PASS / accepted**
- TASK-011C implementation: `21ca529525455408356c38c4d5a2b8361cf950fd`
- TASK-011C Windows acceptance HEAD: `cb01a20a283ac18e07121a94ccc90bc3d232d8cf`

## Current Objective

Stage 5 is **IMPLEMENTATION COMPLETE — FINAL WINDOWS/VULKAN ACCEPTANCE PENDING**.
Stage 6 U++ integration has accepted the shared presenter, root presentation boundary, embedded neutral frame source, CtrlCore recorder and root compositor wiring.

Active Stage-5 validation: `TASK-010-W1` — final vector/gradient/AA/SVG plus image/text regression acceptance.
Remaining Stage-6 validation: Renderer Showcase manual interaction smoke only; its GPU visual/orientation boundary and automated renderer coverage are accepted.

The Renderer Showcase live GPU smoke exposed a real Vulkan framebuffer-orientation defect that earlier structural/resource tests did not detect. Production correction `c13783aaad1ce10d4ade5ac8f020c56e876ae5f8` normalizes Vulkan to the neutral/U++ top-left framebuffer orientation with a negative-height viewport. Windows revalidation at `42d74c7bf44bac5f9ce8c92a3e553946943b8738` passed the focused Vulkan graphics/image/text/vector regressions plus GPU2D and live GpuCtrl frame-source regression, and manager screenshot evidence confirms the authored scene orientation is restored. The orientation defect is therefore closed.

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
Status: **PASS / accepted**
Windows acceptance HEAD: `cb01a20a283ac18e07121a94ccc90bc3d232d8cf`

Implemented scope:
- backend-neutral `RenderPresentation` package owns selected backend session, `UiRenderer2D`, logical surface/swapchain and frame-present lifecycle;
- Vulkan-specific ownership remains private to `RenderPresentation.cpp`; public presentation API contains no Vulkan types;
- `GpuCtrl` uses `GpuDisplayPresenter`, removing its duplicate private Vulkan/session/swapchain/render lifecycle while retaining DHCtrl hosting for explicitly embedded GPU content;
- `GpuTopWindow` binds `GpuDisplayPresenter` directly to the real top-level U++ HWND and creates no native child host;
- `GpuTopWindow::BuildGpuFrame()` is the root neutral-display-list composition hook; U++ remains window/input/layout authority;
- root WM_PAINT presents one neutral frame through one Vulkan surface/swapchain;
- `GpuTopWindowPresentationTest` covers live surface/device/swapchain ownership, idle stability, refresh, resize, hide/show and zero final Vulkan ownership.

Windows acceptance evidence:
- current `GpuTopWindowPresentationTest` Debug compile PASS and runs PASS `4/4`;
- Release compile PASS and runs PASS `2/2`;
- `GpuCtrlPresentationTest` Debug PASS and Release PASS after the shared-presenter migration;
- validation was requested in the root and embedded Vulkan presentation tests and no validation messages occurred;
- no assertions, crashes or `FAIL` output occurred;
- root test final Vulkan ownership passed at zero;
- final worktree clean and diff check passed.

### TASK-011B — embedded neutral frame source

Published on `main`: `3ac69f1971b5770b08ab9d06b7be72654dabe521`
Focused acceptance package on `main`: `947a06038bddb6fd116b00aff1ac697a79eab55b`
PR: `#29`
Status: **PASS / accepted**
Windows acceptance HEAD: `e3ad497b0bb0d001287eddd2d90e0fae861e00c7`

- `GpuCtrl::WhenBuildFrame` optionally accepts a caller-produced immutable neutral `UiDisplayList` plus background;
- callback owns drawing intent only; native host, presentation session, surface/swapchain and renderer ownership stay in `GpuCtrl` / `GpuDisplayPresenter`;
- unset callback preserves the existing default reference scene exactly;
- invalid callback output is rejected before presentation.

Focused acceptance coverage in `tests/GpuCtrlFrameSourceTest`:
- live Vulkan `GpuCtrl` invokes `WhenBuildFrame` and passes the current native-host/control size;
- same-size refresh invokes the callback again without recreating the swapchain;
- resize propagates the new size and recreates the presentation swapchain while retaining one live surface/device/swapchain;
- callback failure preserves caller diagnostic text without tearing down the GPU session;
- invalid `UiDisplayList` output is rejected at the `GpuCtrl` boundary before presentation with deterministic builder error evidence;
- a later valid frame recovers cleanly after both callback failure and invalid-list rejection;
- close requires final Vulkan ownership to return to zero;
- existing `GpuCtrlReplayTest` remains the unset/default-scene authority and `GpuCtrlPresentationTest` remains the shared lifecycle authority.

Windows acceptance evidence:
- required focused-test ancestor `947a06038bddb6fd116b00aff1ac697a79eab55b` verified at acceptance HEAD;
- `GpuCtrlFrameSourceTest` Debug compile PASS, runs PASS `4/4`;
- Release compile PASS, runs PASS `2/2`;
- `GpuCtrlReplayTest` Debug/Release PASS;
- `GpuCtrlPresentationTest` Debug/Release PASS;
- no Vulkan validation messages, assertions, crashes or `FAIL` output;
- final Vulkan ownership returned to zero as asserted by the tests;
- final worktree clean and diff checks passed;
- no edits, commits or pushes were made during validation.

Accepted boundary: caller-owned neutral embedded frame sourcing, default-scene preservation, invalid-output rejection, failure recovery and shared presentation lifecycle are accepted.

### Renderer Showcase / consolidated acceptance

Published on `main`: `094e8807c70fd591bf7e921a5a98ae7069a8b97f`
Source head: `a8d873c987418a183515885751ea0c742edef3aa`
PR: `#25`
Status: **PARTIAL — GPU VISUAL/ORIENTATION BOUNDARY ACCEPTED; MANUAL INTERACTION SMOKE REMAINS**

Reference UI revision inspected while designing the showcase:
- `Trilec/upp_Ui` `main`: `b3375564dff21c124374472aabfd79d62ef0d51e`
- visual/layout reference: `examples/UiLabelDemo`
- PropertyEditor rules: `Utilities/PropertyEditor/DESIGN.md` and `README.md`

Current compatibility review:
- current `Trilec/upp_Ui` API compatibility was rechecked past `1c239c68c504919e60859955db4faf9ea537d181`;
- targeted orientation revalidation used `upp_Ui` HEAD `6ec3232ee295c16db17abb54a51dce0398064473`;
- `UiTitleCard` still exposes the title/subtitle/title-line/content-cell/inset API used by the showcase;
- `UiStack`, `UiBoxLayout`, `UiButton` and PropertyEditor model/event APIs used by the showcase remain source-compatible.

Implemented design:
- `examples/RendererShowcaseScene` is the single scene authority;
- the shared scene records fills, rectangle strokes, rounded geometry, Save/Restore, clipping, affine transform, source alpha, sampled image, text, reflected multi-stop gradient vector fill, dashed round vector stroke and SVG;
- `examples/RendererShowcase` is a deliberately light developer-facing window inspired by the UiLabel demo: `UiTitleCard` header/title line, status + GPU/Software/Reset/Exit buttons, large preview area, right PropertyEditor rail;
- PropertyEditor remains the single authored interactive state with `Renderer`, `Content`, `Appearance` and `Geometry` groups;
- GPU preview uses accepted `GpuCtrl::WhenBuildFrame`; software preview uses `SoftwareUiRenderer`; both call the exact same `BuildRendererShowcaseScene()` function;
- `tests/RendererShowcaseTest` consumes that same shared scene, checks every broad capability op, deterministic dump, software visible output/immutability, and an alternate interactive property projection.

Mechanical Windows fixes published during acceptance:
- `758e8126b073abb7b6659b5b153781346a24f170` adds the explicit `CtrlLib` include/package dependency required for concrete `ImageDraw` construction;
- `77bce046cef7c1e746b4e5ae63c5b49f82272d4a` replaces stale `upp_AnimationEasing` in `GitHubOut.var` with the actual `E:/apps/github/upp_animation` dependency used by current `upp_Ui`;
- both fixes are mechanical build/dependency corrections; no renderer/showcase behavior or test expectations changed.

Automated/binary acceptance:
- `RendererShowcaseTest` Debug build PASS and runs PASS `2/2`;
- `RendererShowcaseTest` Release build PASS and run PASS `1/1`;
- interactive `RendererShowcase` Debug build/link PASS;
- interactive `RendererShowcase` Release build/link PASS, launch PASS and clean close PASS;
- `GpuCtrlFrameSourceTest` Debug PASS;
- targeted post-orientation `RenderVulkanGraphicsTest` Debug/Release PASS;
- targeted post-orientation `RenderVulkanImageTest` Debug/Release PASS;
- targeted post-orientation `RenderVulkanTextTest` Debug/Release PASS;
- targeted post-orientation `RenderVulkanVectorTest` Debug/Release PASS;
- targeted post-orientation `RenderGpu2DTest` Debug PASS;
- no Vulkan validation errors or runtime failures were observed;
- both validation repositories were clean and diff checks passed.

Visual orientation acceptance:
- manager screenshot plus Windows validation screenshot confirms footer at bottom;
- gradient/vector blob is upper-right;
- SVG panel is lower-right and its triangle points upward;
- GPU text is upright/readable;
- sampled image is upright;
- transformed left card has the authored orientation;
- no blank GPU preview was observed;
- resize operations left the process stable.

Accepted orientation root cause/fix:
- the pre-fix whole framebuffer was vertically inverted; this was not a glyph/SVG-specific or sampled-UV-only defect;
- `UiRenderer2D` maps U++ y=0 to NDC +1 and bottom to NDC -1, while the Vulkan backend had used a positive-height viewport;
- `c13783aaad1ce10d4ade5ac8f020c56e876ae5f8` corrects the Vulkan backend with `y = height`, `height = -height`;
- the correction applies uniformly to solid, sampled image, glyph and vector/SVG-raster paths without changing neutral display-list, U++ text/vector authority or UV conventions.

Remaining manual interaction boundary:
- Windows UI Automation exposes only the top-level U++ window, so it could not reliably drive the custom GPU/Software buttons or PropertyEditor child controls;
- therefore live GPU -> Software comparison, representative property edits, Reset and repeated GPU/Software switching have not been directly runtime-verified;
- this is not evidence of a renderer failure: the shared scene's software replay and alternate property projection are already covered by `RendererShowcaseTest`, but GUI event dispatch itself remains a short human smoke item.

Compiler note:
- one existing warning remains in the preserved Stage-4 `RenderGpu2DBase.inc` switch because later `UiDisplayOpType` values are handled by Stage-5 wrapper paths; current source review found no missing production fallback caused by this warning;
- do not create a renderer validation cycle solely to silence it; clean it only as part of a coherent warning/hygiene pass.

Dependency boundary:
- no `Ui` or PropertyEditor dependency was added to RenderCore/RenderCanvas/RenderGpu2D/RenderRhi/RenderVulkan;
- only the interactive example depends on the external `upp_Ui` repository;
- console showcase test and shared scene remain renderer-repo-only.

### TASK-011B — CtrlCore semantic recording bridge

Published on `main`: `c4210d80a815950df53df5db9dea45a38edbbfdd`
Source head: `30ef093f4035eff923594bc6408ae17292a8512f`
PR: `#27`
Status: **PASS / accepted**

Implemented scope:
- new `render/RenderCtrlBridge` production package depends on `CtrlCore` + `RenderCanvas`, not CtrlLib, Ui, Vulkan or platform APIs;
- public `RecordCtrlDisplayList()` supplies a recording Win32 `SystemDraw` to public `Ctrl::DrawCtrl()`, so U++'s own private `CtrlPaint` path remains the recursive control/frame/layout/theme authority;
- no duplicate Ctrl-tree traversal, private `CtrlPaint` access, HDC-backed target, native-child-per-control mechanism or second theme/layout model is introduced;
- U++ 2026.1 normally routes Win32 painting through `BackDraw`; the bridge uses a harmless `FULLBACKPAINT` probe to observe the otherwise write-only `GlobalBackBuffer` state, enables direct drawing only when needed, and restores the exact inherited state under the GUI lock;
- neutral `SystemDraw` translation maps U++ Begin/End, Offset, Clip/Clipoff/intersect, rectangles, images including U++ tint semantics, text, lines/polylines, disjunct polygons with EvenOdd fill, and ellipses into existing immutable `UiDisplayList` operations;
- unsupported exclusion clips, native SystemDraw/GDI access, invert/XOR/pattern drawing, arcs and rotated text fail explicitly instead of being silently dropped;
- focused Windows validation passed Debug 4/4, Release 2/2 plus RenderCanvas/RenderVector/RenderText regressions.

Accepted boundary:
- ordinary resolved Win32 U++ semantic control painting is accepted;
- exclusion clips/native child surfaces, raw native drawing, arcs and rotated DrawText remain explicit unsupported boundaries.

### TASK-011C — root compositor wiring

Published on `main`: `21ca529525455408356c38c4d5a2b8361cf950fd`
Source head: `dbc7f27f2888ba637d949dadc25edcd9074a3537`
PR: `#28`
Status: **PASS / accepted**
Windows acceptance HEAD: `cb01a20a283ac18e07121a94ccc90bc3d232d8cf`

Implemented scope:
- `GpuTopWindow` now depends directly on `RenderCtrlBridge` and the base `BuildGpuFrame()` records `*this` through accepted `RecordCtrlDisplayList()`;
- no duplicate traversal, second style/layout state, per-control native surface or backend-specific UI API was introduced;
- the existing virtual `BuildGpuFrame()` contract is preserved, so custom neutral frame sources remain source-compatible and can still bypass default control-tree recording deliberately;
- successful root GPU presentation validates the Win32 paint region only after presentation succeeds;
- recorder or presenter failure now falls through to `TopWindow::WindowProc()` so ordinary U++ software painting remains the visible fallback instead of consuming WM_PAINT and blank-filling the client area;
- the production hot path adds only the accepted semantic recording call on actual root repaints; no new persistent cache or duplicate state is introduced;
- `GpuTopWindowPresentationTest` now exercises the base recorder with a real Label, Button and custom child, verifies root/child paint execution plus semantic text/geometry evidence, re-records on explicit refresh, preserves one surface/device/swapchain lifecycle, and checks an intentional frame-build failure falls through to U++ software painting while retaining diagnostic error evidence.

Windows acceptance evidence:
- implementation ancestor `21ca529525455408356c38c4d5a2b8361cf950fd` verified at acceptance HEAD;
- `GpuTopWindowPresentationTest` Debug compile PASS, runs PASS `4/4`;
- Release compile PASS, runs PASS `2/2`;
- `GpuCtrlPresentationTest` Debug/Release PASS;
- `RenderCtrlBridgeTest` Debug/Release PASS;
- `RenderCanvasTest`, `RenderVectorTest` and `RenderTextTest` Debug PASS;
- no assertions, validation messages, crashes or `FAIL` output;
- root test final Vulkan ownership passed at zero;
- final worktree clean and diff check passed;
- no edits, commits or pushes were made during validation.

## Recovery Log

BASE: `42d74c7bf44bac5f9ce8c92a3e553946943b8738` / `main`
TASK: Renderer Showcase final human interaction smoke, then Stage-5 final vector/Vulkan acceptance
TOUCHED: `render/RenderVulkan/RenderVulkanRhi.cpp`; showcase/build fixes; `docs/ACTIVE_WORK.md`
STATUS: Stage 3 PASS; Stage 4 PASS with orientation correction accepted; Stage-5 images/text PASS; Stage-5 vector IMPLEMENTATION COMPLETE / PLATFORM VALIDATION PENDING; TASK-011A/B/C accepted; Renderer Showcase GPU visual/orientation + automated coverage PASS; manual GUI interaction smoke remains
PUBLISHED: showcase `094e8807c70fd591bf7e921a5a98ae7069a8b97f`; `ImageDraw`/CtrlLib fix `758e8126b073abb7b6659b5b153781346a24f170`; animation dependency-path fix `77bce046cef7c1e746b4e5ae63c5b49f82272d4a`; Vulkan framebuffer orientation fix `c13783aaad1ce10d4ade5ac8f020c56e876ae5f8`; evidence checkpoint `42d74c7bf44bac5f9ce8c92a3e553946943b8738`; earlier accepted checkpoints unchanged
VALIDATION: orientation-fix ancestor verified; Debug showcase build/live GPU PASS; authored orientation visually PASS; resize stable; Release build/launch/close PASS; RenderVulkanGraphics/Image/Text/Vector Debug+Release PASS; RenderGpu2D Debug PASS; GpuCtrlFrameSourceTest Debug PASS; no Vulkan validation/runtime failures; `upp_Ui` tested at `6ec3232ee295c16db17abb54a51dce0398064473`; trees clean

## Next Action

Perform one short human Renderer Showcase interaction smoke: switch GPU -> Software, change representative Text/Accent/Rotation or Scale values, toggle Image/SVG/Clip, Reset, switch Software -> GPU, and confirm both previews remain visible and responsive. If that passes, mark Renderer Showcase and Stage 6 **PASS / accepted**. Then execute the separately tracked `TASK-010-W1` final Stage-5 vector/gradient/AA/SVG plus image/text Vulkan acceptance.