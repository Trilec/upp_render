# Active Work

Remote `main` is authoritative. This file is a recovery checkpoint only, not project history.

## Recovery

- Repository: `Trilec/upp_render`; work directly on `main`.
- Fetch current remote HEAD before work; do not rely on remembered SHAs.
- Active milestone: UI1-C — transient/multi-window GPU completion.
- Next milestone: UI1-D — shared immutable GPU resources.
- Windows/U++ validation: `CLANGx64_Vulkan.bm`.

## Accepted Boundary

UI1-A / UI1-B / UI1-R1 / UI1-R2 are accepted:

- backend-neutral `UiCanvas` / immutable display list / software reference / RHI stack;
- Vulkan GPU2D images, text, vector/SVG and root composition;
- embedded `GpuCtrl`, `GpuTopWindow`, modal second GPU window and shared compatible device domain;
- common Draw semantics, transformed child clipping, software fallback and ownership-zero cleanup.

Do not reopen accepted areas without a new reproducible regression.

## UI1-C Current State

- Generic owned transient popup lifecycle: PASS.
- Stock U++ `DropList` compatibility: Debug/Release PASS; compatibility coverage only.
- Real `upp_Ui::UiDropdown`: Debug/Release PASS; pointer `20/20`; keyboard `8/8`.
- UiDropdown keyboard-reopen defect fixed in `upp_Ui` with regression coverage. UiDropdown acceptance is CLOSED.
- `GpuUiGallery` includes real `UiDropdown` and `UiMenu` menu/submenu content.
- `GpuUiMenuPopupPresentationTest` is published; Windows validation pending.

## Remaining UI1-C

1. Validate `GpuUiMenuPopupPresentationTest` Debug + Release and normal gallery menu/submenu input.
2. Validate the real current U++ tooltip path attached to an `upp_Ui` control.
3. Run consolidated root smoke: controls, dropdown, menu, tooltip, animation, resize/minimize/restore, caret, modal dialog, root close, final ownership ZERO.
4. If all pass, mark UI1-C accepted and move immediately to UI1-D.

## Next Architecture

- Durable convergence guidance: `docs/UPP_UI_RENDER_CONVERGENCE.md`.
- UI1-D must establish explicit immutable resource identity/lifetime before sharing presenter resources.
- Later focused `upp_Ui` integration must preserve independent software operation and renderer-side composition.

## Repository Hygiene

- Do not create routine recovery/chatgpt/agent branches; `main` is the working line.
- `upp_render` historical non-main branch cleanup remains pending; delete only branches proven contained in `origin/main`.

## Guardrails

- U++ / `upp_Ui` remain authority for hierarchy, layout, input, focus, state, theme/model and invalidation.
- No native GPU child per ordinary control; public recording/UI APIs remain backend-neutral.
- Diagnose first; smallest coherent change; review touched dependencies/tests; run `git diff --check`.

## Recovery Log

TASK: finish UI1-C without broadening scope
STATUS: foundation PASS; transient base PASS; real UiDropdown fully PASS; UiMenu validation pending
NEXT: clean merged historical branches; validate UiMenu, tooltip and final root smoke; then UI1-D
