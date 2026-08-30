# Active Work

Remote `main` is authoritative. This is a compact recovery checkpoint, not project history.

## Recovery

- Repository: `Trilec/upp_render`; active branch: `main` only.
- Current renderer source checkpoint before this status update: `0795c74101a834b34510136d5ec97b6c68a37ba4`.
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
- Product-facing `GpuUiGallery` uses real `upp_Ui::UiDropdown`.
- `GpuUiDropdownPopupPresentationTest`: Debug PASS and Release PASS.
- Real `UiDropdown` pointer acceptance: `20/20` PASS.
- Real `UiDropdown` keyboard acceptance: `8/8` PASS.
- `upp_Ui` keyboard-reopen root cause fixed at `1780907a...`: owner-initiated popup close no longer incorrectly suppresses the next owner click.
- `upp_Ui` includes `UiDropdownInteractionTest` regression coverage for that bug.

UiDropdown acceptance is CLOSED. Do not return to stock `DropList` as the product gate.

## Remaining UI1-C

1. Validate real `upp_Ui::UiMenu` root/context popup and submenu GPU presentation, including shared-device ownership and repeated open/close.
2. Validate application tooltip transient presentation. No `UiTooltip` control currently exists in `upp_Ui`; use the real U++ tooltip path attached to an `upp_Ui` control unless repository state changes.
3. Re-run consolidated root smoke: ordinary controls, `UiDropdown`, menu, tooltip, animation, resize/minimize/restore, caret, modal dialog, normal root close and final Vulkan ownership ZERO.
4. Mark UI1-C accepted, then move immediately to UI1-D.

## Repository Hygiene

- New work is directly on `main`; do not create recovery/chatgpt/agent branches unless Curt explicitly requests isolation.
- `upp_Ui` remote currently has only `main`.
- `upp_render` still contains historical `chatgpt/*`, `recovery/*` and `agent/*` branches; no open PR uses them.
- Delete non-main remote branches only after verifying each tip is contained in `origin/main`; do not discard unique commits.

## Guardrails

- Product-facing acceptance uses real `upp_Ui` controls where equivalents exist.
- U++ remains authority for layout, input, focus, state and theme.
- Public UI/recorder APIs remain backend-neutral; no Vulkan leakage.
- Do not create one native GPU child per ordinary control or a second layout/theme/state system.
- Diagnose before fixing; smallest coherent change; review complete touched files/package dependencies; run `git diff --check` before publication.

## Recovery Log

BASE: current remote `main`
TASK: finish UI1-C with `UiMenu`, tooltip and final root smoke
STATUS: foundation PASS; transient popup PASS; stock DropList compatibility PASS; real `UiDropdown` Debug/Release + pointer `20/20` + keyboard `8/8` PASS
NEXT: `UiMenu`/submenu acceptance, tooltip acceptance, final UI1-C root smoke; clean historical remote branches after ancestry verification
