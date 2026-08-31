# Active Work

Remote `main` is authoritative. This is a compact recovery checkpoint, not project history.

## Recovery

- Repository: `Trilec/upp_render`; work directly on `main`.
- Always fetch current remote HEAD before work; do not rely on a recorded SHA here.
- Current `upp_Ui/main` observed during this update: `1e6907c4fe6e15adeb9daed06d5a12fd884e3f39`; accepted UiDropdown reopen fix `1780907a...` is its ancestor.
- Active milestone: UI1-C — transient/multi-window GPU completion.
- Next: UI1-D — shared immutable GPU resources; then UI2 — focused `upp_Ui`/renderer convergence.
- Windows/U++ validation uses `CLANGx64_Vulkan.bm`.

## Accepted Foundation

UI1-A / UI1-B / UI1-R1 / UI1-R2 are accepted:

- backend-neutral `UiCanvas` / immutable display-list + software reference and RHI/provider architecture;
- Vulkan production provider and GPU2D geometry/images/text/vector/SVG;
- embedded `GpuCtrl` and one-surface `GpuTopWindow` root composition;
- modal second `GpuTopWindow`, shared compatible device domain and ownership-zero cleanup;
- DrawArc, rotated text, destination-invert caret, transformed child clipping and atomic software fallback.

Do not reopen accepted areas without a new reproducible regression.

## UI1-C Current State

- Generic owned transient popup lifecycle: PASS.
- Stock U++ `DropList` compatibility: Debug/Release PASS; final ownership ZERO.
- Real `upp_Ui::UiDropdown`: focused Debug/Release PASS; pointer `20/20`; keyboard `8/8`.
- UiDropdown keyboard-reopen root cause fixed in `upp_Ui` with regression coverage. UiDropdown acceptance is CLOSED.
- `GpuUiGallery` includes real `UiDropdown` and real `UiMenu` menu-bar/submenu content.
- `GpuUiMenuPopupPresentationTest` is published and awaits Windows validation; target ownership is root `1/1/1`, menu `2/2/1`, submenu `3/3/1`, close back to `1/1/1`, final ZERO.

## Remaining UI1-C

1. Validate `GpuUiMenuPopupPresentationTest` Debug + Release and normal gallery `UiMenu`/submenu input.
2. Validate real application tooltip transient presentation; use the actual current U++ tooltip path attached to an `upp_Ui` control unless a first-class Ui tooltip exists by then.
3. Run consolidated root smoke: controls, UiDropdown, UiMenu, tooltip, animation, resize/minimize/restore, caret, modal dialog, root close and final ownership ZERO.
4. Mark UI1-C accepted and move on; do not redesign the renderer inside UI1-C.

## Architecture Convergence Recorded

- `docs/UPP_UI_RENDER_CONVERGENCE.md` is the durable `upp_Ui`/renderer guidance from current Graph optimization work.
- `upp_Ui` remains independently usable; no hard `upp_Ui -> upp_render` or backend dependency.
- preserve semantic `UiCanvas`/`UiDisplayList`, local damage information and renderer-side integration ownership;
- repeated immutable presentation work should be reusable rather than recreated per item/frame;
- UI1-D must define explicit resource identity/ownership/invalidation/synchronization/budget/eviction/device-lifetime rules;
- UI2 will address focused normal-`upp_Ui` GPU integration, including source-rect image, tint, 9-slice, non-rect clipping, DPI/damage and prepared-state boundaries.

## Repository Hygiene

- Do not create routine recovery/chatgpt/agent branches; `main` is the working line.
- `upp_Ui` remote was already main-only when last checked.
- `upp_render` still has 69 historical non-main remote branches; cleanup remains pending.
- Delete a non-main branch only after proving its tip is contained in `origin/main`; preserve and report any unique commits.

## Guardrails

- U++ / `upp_Ui` remain authority for layout, input, focus, state, theme/model and invalidation semantics.
- No native GPU child per ordinary control; transient native windows are the exception when U++ creates a real top-level popup/menu/tooltip.
- Public recording/UI APIs remain backend-neutral; no Vulkan leakage or second UI framework.
- Heavy controls retain/precompute geometry, routes, LOD, hit regions and prepared presentation; renderer consumes resolved intent.
- Diagnose first; smallest coherent change; review touched dependencies/tests; `git diff --check` before publication.

## Recovery Log

TASK: finish UI1-C without broadening scope
STATUS: foundation PASS; transient base PASS; real UiDropdown fully PASS; UiMenu acceptance source published, validation pending
NEXT: clean merged historical branches; validate UiMenu, tooltip and final UI1-C root smoke; then start UI1-D
