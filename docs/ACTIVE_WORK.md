# Active Work

Remote `main` is authoritative. This file is only a compact recovery checkpoint, not project history.

## Recovery

- Repository: `Trilec/upp_render`
- Branch: `main`
- Validated source checkpoint: `ab7b632e83b7d51de89879d75a1b42879d3031fe`
- Active milestone: UI1-C — transient/multi-window GPU completion
- Next milestone: UI1-D — shared immutable GPU resources
- Windows/U++ validation is performed with `CLANGx64_Vulkan.bm`.

## Accepted Foundation

UI1-A / UI1-B / UI1-R1 / UI1-R2 are accepted:

- backend-neutral display-list/software foundation;
- RHI/provider architecture with Vulkan as the current production backend;
- GPU2D geometry, images, text, vector/SVG;
- embedded `GpuCtrl`;
- `GpuTopWindow` root composition of ordinary U++ controls;
- modal second `GpuTopWindow`;
- DrawArc, rotated text and destination-invert caret semantics;
- transformed child clipping;
- atomic root software fallback + explicit `RetryGpuInit()`;
- repeated root/animation/resize/modal acceptance;
- final Vulkan ownership returns to ZERO.

Do not reopen accepted areas without a new reproducible regression.

## UI1-C Current State

Implemented architecture:

- owned Win32 `WS_POPUP` U++ top-levels are discovered through U++ `StateHook`;
- each transient top-level receives its own `GpuDisplayPresenter` surface/swapchain;
- compatible root/transient presenters share the application GPU context/logical device;
- ordinary child controls do not receive native GPU HWNDs;
- transient presentation failure falls back to ordinary U++ painting;
- non-modal `GpuTopWindow::Close()` releases its GPU session synchronously while modal-loop semantics remain intact.

Focused generic popup lifecycle: PASS.

Focused real `DropList` probe at `ab7b632...`: Gary reported Debug PASS and Release PASS:

- `WhenDrop` fired;
- real owned popup found, open and visible;
- popup rect `260 x 47`;
- live surface/swapchain/device = `2 / 2 / 1`;
- popup shared the root logical device;
- final Vulkan ownership = ZERO;
- no crashes, assertions or `FAIL:` lines.

Important conclusion: the real U++ `PopUpList` creation + GPU transient attachment path is currently proven. The earlier `GpuUiGallery` click run that showed no popup is not evidence of a renderer defect unless it reproduces with verified real input.

## Remaining UI1-C Acceptance

1. Run `GpuUiGallery` and exercise the real DropList through normal user input for 20 open/select/close cycles.
2. If it passes, close the old DropList blocker without changing renderer code.
3. If normal input still fails while programmatic `Drop()` passes, diagnose only root input/MultiButton hit-testing/focus/activation before touching transient presentation.
4. Add/validate real menu or context-menu GPU transient presentation.
5. Add/validate tooltip GPU transient presentation.
6. Re-run full root smoke: controls, particle animation, resize/minimize/restore, caret, modal dialog and final Vulkan ownership ZERO.
7. Mark UI1-C accepted only when the above real-control matrix passes.

## After UI1-C

UI1-D:

- define context-owned immutable image/glyph/vector resource identities;
- share only resources with explicit identity/lifetime contracts;
- prove closing one presenter/window cannot invalidate another.

After UI1-D, move to the next backend milestone. Prefer executable WebGPU work before Metal if Windows/browser tooling remains the fastest validation path.

## Guardrails

- U++ remains the authority for layout, input, focus, state and theme.
- Public UI/recorder APIs remain backend-neutral; no Vulkan leakage.
- Do not create one native GPU child per ordinary control.
- Do not add a second layout/theme/state system.
- Do not weaken fallback, validation or ownership-zero assertions.
- Unsupported Draw semantics remain explicit failures until deliberately implemented.
- Diagnose before fixing; make the smallest coherent change.
- Review complete touched files/package dependencies and run `git diff --check` before publication.

## Recovery Log

BASE: current remote `main`
TASK: finish UI1-C real transient-control acceptance
STATUS: foundation PASS; generic popup PASS; real programmatic DropList PASS Debug/Release; gallery normal-input DropList + menu/context-menu + tooltip acceptance remain
NEXT: real `GpuUiGallery` DropList 20-cycle normal-input acceptance, then menu/context-menu and tooltip
