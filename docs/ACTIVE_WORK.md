# Active Work

Remote `main` is authoritative. This file is only a compact recovery checkpoint, not project history.

## Recovery

- Repository: `Trilec/upp_render`
- Branch: `main`
- Last validated renderer checkpoint: `ab7b632e83b7d51de89879d75a1b42879d3031fe`
- Current source: `main`; `GpuUiGallery` now uses `upp_Ui::UiDropdown` and awaits Windows validation.
- `upp_Ui` API checked against `main` at `0fd8df299bca20d27e19d755693f65eff4dcdca6`.
- Active milestone: UI1-C — transient/multi-window GPU completion
- Next milestone: UI1-D — shared immutable GPU resources
- Windows/U++ validation uses `CLANGx64_Vulkan.bm`.

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

Focused stock-U++ `DropList` compatibility probe at `ab7b632...`: Gary reported Debug PASS and Release PASS:

- real owned popup found, open and visible;
- popup rect `260 x 47`;
- live surface/swapchain/device = `2 / 2 / 1`;
- popup shared the root logical device;
- final Vulkan ownership = ZERO;
- no crashes, assertions or `FAIL:` lines.

This stock `DropList` test remains a low-level U++ compatibility regression. It is not the product-facing dropdown acceptance target.

`GpuUiGallery` now uses the real `upp_Ui::UiDropdown` from the `Ui` package. This deliberately exercises the control family used by product applications, including its own styled collapsed control and native popup window.

## Remaining UI1-C Acceptance

1. Build `GpuUiGallery` with the `upp_Ui` assembly path and verify `UiDropdown` records through the Vulkan root.
2. Exercise the real `UiDropdown` through normal user input for 20 open/select/close cycles across all three choices.
3. Verify its popup is GPU-presented, visually correct, responsive, and leaves no ownership leak/fallback.
4. If it fails, diagnose the actual `UiDropdown` popup/input path; do not substitute stock `DropList` as product acceptance.
5. Keep `GpuDropListPopupPresentationTest` passing as the generic stock-U++ compatibility regression.
6. Add/validate the relevant real menu/context-menu transient path, preferring `upp_Ui` controls used by applications.
7. Add/validate tooltip transient presentation used by applications.
8. Re-run full root smoke: controls, particle animation, resize/minimize/restore, caret, modal dialog and final Vulkan ownership ZERO.
9. Mark UI1-C accepted only when the real-control matrix passes.

## After UI1-C

UI1-D:

- define context-owned immutable image/glyph/vector resource identities;
- share only resources with explicit identity/lifetime contracts;
- prove closing one presenter/window cannot invalidate another.

After UI1-D, move to the next backend milestone. Prefer executable WebGPU work before Metal if Windows/browser tooling remains the fastest validation path.

## Guardrails

- Product-facing acceptance should use the real `upp_Ui` controls where equivalents exist.
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
TASK: finish UI1-C using real product controls
STATUS: foundation PASS; generic popup PASS; stock DropList compatibility PASS; `GpuUiGallery` switched to `UiDropdown`, validation pending
NEXT: build + 20-cycle real `UiDropdown` gallery acceptance, then menu/context-menu and tooltip
