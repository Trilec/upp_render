# Active Work

Remote `main` is authoritative. This is a compact recovery checkpoint, not project history.

## Recovery

- Repository: `Trilec/upp_render`; active branch: `main` only.
- Current source checkpoint: `cbe008a9b324df86201de7df9f66b3ca94127c86`.
- Current `upp_Ui` popup fix: `1780907a1aa50b3a64a4baf70d573718ebe45161`.
- Active milestone: UI1-C — transient/multi-window GPU completion.
- Next milestone: UI1-D — shared immutable GPU resources.
- Windows/U++ validation uses `CLANGx64_Vulkan.bm`.

## Accepted Foundation

UI1-A / UI1-B / UI1-R1 / UI1-R2 are accepted:

- backend-neutral display-list/software + RHI/provider architecture;
- Vulkan production provider and GPU2D geometry/images/text/vector/SVG;
- embedded `GpuCtrl` and `GpuTopWindow` root composition;
- modal second `GpuTopWindow`;
- DrawArc, rotated text, destination-invert caret and transformed child clipping;
- atomic software fallback + explicit `RetryGpuInit()`;
- repeated root/animation/resize/modal acceptance;
- final Vulkan ownership returns to ZERO.

Do not reopen accepted areas without a new reproducible regression.

## UI1-C Current State

- Generic owned transient popup lifecycle: PASS.
- Stock U++ `DropList` popup compatibility: Debug/Release PASS; `2 / 2 / 1` surface/swapchain/device; final ownership ZERO.
- Real `upp_Ui::UiDropdown`: focused Debug/Release PASS; pointer `20/20` PASS; keyboard `8/8` PASS.
- `upp_Ui` keyboard-reopen root cause fixed at `1780907a...` with `UiDropdownInteractionTest` regression coverage.
- UiDropdown acceptance is CLOSED.
- `GpuUiGallery` now includes real `UiDropdown` and real `UiMenu` menu-bar/submenu content.
- `GpuUiMenuPopupPresentationTest` is published and awaits Windows validation; it targets root menu `2 / 2 / 1`, submenu `3 / 3 / 1`, leaf close back to `1 / 1 / 1`, repeated cycles, and final ownership ZERO.

## Remaining UI1-C

1. Build/run `GpuUiMenuPopupPresentationTest` Debug + Release, then exercise `UiMenu` through normal gallery input including submenu selection.
2. Validate application tooltip transient presentation. No `UiTooltip` currently exists in `upp_Ui`; use the real U++ tooltip path attached to an `upp_Ui` control unless repository state changes.
3. Re-run consolidated root smoke: ordinary controls, `UiDropdown`, menu, tooltip, animation, resize/minimize/restore, caret, modal dialog, normal root close and final Vulkan ownership ZERO.
4. Mark UI1-C accepted, then move immediately to UI1-D.

## Repository Hygiene

- New work is directly on `main`; do not create recovery/chatgpt/agent branches unless Curt explicitly requests isolation.
- `upp_Ui` remote currently has only `main`.
- `upp_render` has 69 historical non-main remote branches and no open PRs.
- Delete a non-main branch only after `git merge-base --is-ancestor origin/<branch> origin/main` succeeds; do not discard unique commits.

## Guardrails

- Product-facing acceptance uses real `upp_Ui` controls where equivalents exist.
- U++ remains authority for layout, input, focus, state and theme.
- Public UI/recorder APIs remain backend-neutral; no Vulkan leakage.
- Do not create one native GPU child per ordinary control or a second layout/theme/state system.
- Diagnose before fixing; smallest coherent change; review complete touched files/package dependencies; run `git diff --check` before publication.

## Recovery Log

BASE: current remote `main`
TASK: finish UI1-C with `UiMenu`, tooltip and final root smoke
STATUS: foundation PASS; transient popup PASS; stock DropList compatibility PASS; real `UiDropdown` fully PASS; `UiMenu` acceptance source published, validation pending
NEXT: clean historical branches after ancestry verification; validate `UiMenu`, tooltip and final UI1-C root smoke
