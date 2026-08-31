# Active Work

Remote `main` is authoritative. This file is a recovery checkpoint only, not project history.

## Recovery

- Repository: `Trilec/upp_render`; work directly on `main`.
- Fetch current remote HEAD before work; do not rely on remembered SHAs.
- Windows/U++ validation: `CLANGx64_Vulkan.bm` (local machine config; uses the local Vulkan SDK; not committed).
- After any `upp_Ui` change, rebuild renderer tests with a clean `-ab`/`-abr` build; incremental builds after cross-repo changes can fail GPU init spuriously.

## Accepted Foundation

UI1-A / UI1-B / UI1-R1 / UI1-R2 are accepted:

- backend-neutral `UiCanvas` / immutable display list / software reference / RHI stack;
- Vulkan GPU2D images, text, vector/SVG and root composition;
- embedded `GpuCtrl`, `GpuTopWindow`, modal second GPU window and shared compatible device domain;
- common Draw semantics, transformed child clipping, software fallback and ownership-zero cleanup.

Do not reopen accepted areas without a new reproducible regression.

## UI1-C: ACCEPTED

- Generic owned transient popup lifecycle: PASS.
- Stock U++ `DropList` compatibility: PASS (compatibility coverage only).
- Real `upp_Ui::UiDropdown`: Debug/Release PASS; pointer `20/20`; keyboard `8/8`; keyboard-reopen defect fixed in `upp_Ui` with regression coverage.
- Real `upp_Ui::UiMenu`: Debug/Release PASS. Ownership `1/1/1` root -> `2/2/1` menu -> `3/3/1` submenu -> `1/1/1`, shared logical device, four cycles, final ownership ZERO. Leaf-activation use-after-free fixed in `upp_Ui` (`PopupLevel::LeftDown`) with `UiMenuInteractionTest`.
- Gallery real-input menu coverage: `20/20` pointer cycles (File, Tools, Tools->More submenu, leaf selections), keyboard `2/2`, UiDropdown smoke `2/2`; menus fully painted, actions fire, no stuck popups.
- Real tooltip: current U++ `ToolTip` (CtrlLib singleton) shown via `Ctrl::PopUp` as an owned top-level popup; attached to a real `upp_Ui` control with `Ctrl::Tip()`. `GpuUiTooltipPresentationTest` Debug/Release PASS: tooltip = `2/2/1`, hide = `1/1/1`, four cycles, final ZERO. Show path requires a foreground owner; the test asserts it.
- Consolidated `GpuUiGallery` smoke: `17/17` (edit/caret, option, slider/progress, apply, ArrayCtrl, particles, tooltip, menu, submenu, dropdown, resize, minimize/restore, post-restore menu, modal dialog 5x, GPU label stable, normal close).
- Regression matrix: menu/dropdown/tooltip focused tests Debug+Release PASS; DropList, transient, top-window, ctrl-presentation, UiCoverage, CtrlBridge, Gpu2D, Vulkan Debug PASS.
- Final Vulkan ownership ZERO on every focused test and the smoke.

## Next Milestone

UI1-D — shared immutable GPU resources:

- context-owned immutable image/glyph/vector resource identities;
- share only resources with explicit identity/lifetime contracts;
- prove closing one presenter/window cannot invalidate another.

Durable guidance: `docs/UPP_UI_RENDER_CONVERGENCE.md`.

## Repository Hygiene

- `upp_render` historical branch cleanup done: 45 merged remote branches and 2 merged local branches deleted after `merge-base --is-ancestor` verification.
- Preserved (unique commits, not discarded): 24 remote branches (`agent/*` 2, `chatgpt/*` 15, `recovery/*` 7) plus local `task/001-render-foundation` (1 commit). Tips and unique-commit counts recorded in the UI1-C final report.

## Guardrails

- U++ / `upp_Ui` remain authority for hierarchy, layout, input, focus, state, theme/model and invalidation.
- No native GPU child per ordinary control; public recording/UI APIs remain backend-neutral.
- Product-facing acceptance uses real `upp_Ui` controls; do not substitute stock controls.
- Diagnose first; smallest coherent change; review touched dependencies/tests; run `git diff --check`.

## Recovery Log

TASK: finish and close UI1-C
STATUS: UI1-C ACCEPTED — all gates PASS, final ownership ZERO, branch cleanup done
NEXT: UI1-D shared immutable GPU resources
