# Active Work

Remote `main` is authoritative. This is a compact recovery checkpoint, not project history.

## Recovery

- Repository: `Trilec/upp_render`; branch: `main`.
- Last validated renderer checkpoint: `ab7b632e83b7d51de89879d75a1b42879d3031fe`.
- `upp_Ui` API checked against `main` at `0fd8df299bca20d27e19d755693f65eff4dcdca6`.
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

- Owned Win32 `WS_POPUP` U++ top-levels are discovered through U++ `StateHook`.
- Each transient top-level gets its own `GpuDisplayPresenter` surface/swapchain while compatible presenters share the application GPU context/logical device.
- Ordinary child controls do not get native GPU HWNDs; transient failure falls back to ordinary U++ painting.
- Non-modal `GpuTopWindow::Close()` releases its GPU session synchronously; modal-loop semantics remain intact.
- Generic transient popup lifecycle: PASS.
- Stock-U++ `GpuDropListPopupPresentationTest`: Debug/Release PASS at `ab7b632...`; real popup visible at `260 x 47`, live surface/swapchain/device `2 / 2 / 1`, final ownership ZERO.
- Stock `DropList` is compatibility coverage only, not the product-facing dropdown acceptance target.
- `GpuUiGallery` now uses the real `UiDropdown` from `upp_Ui` / package `Ui`.
- `GpuUiDropdownPopupPresentationTest` now provides deterministic product-control popup/device/lifetime coverage; Windows validation pending.

## Remaining UI1-C Acceptance

1. Build/run `GpuUiDropdownPopupPresentationTest` Debug + Release with the `upp_Ui` assembly path.
2. Require real `UiDropdown` popup open/visible, valid rect, `2 / 2 / 1` live surface/swapchain/device, repeated close/reopen, collapsed selection still functional, and final ownership ZERO.
3. Run `GpuUiGallery`; exercise `UiDropdown` through normal input for 20 open/select/close cycles across all three choices.
4. If `UiDropdown` fails, diagnose its actual popup/input path; do not substitute stock `DropList` as product acceptance.
5. Add/validate the real application menu/context-menu transient path, preferring `upp_Ui` controls where equivalents exist; then tooltip presentation.
6. Re-run full root smoke: controls, particle animation, resize/minimize/restore, caret, modal dialog and ownership ZERO.
7. Mark UI1-C accepted only when the real-control matrix passes.

## After UI1-C

UI1-D:

- define context-owned immutable image/glyph/vector resource identities;
- share only resources with explicit identity/lifetime contracts;
- prove closing one presenter/window cannot invalidate another.

Then move to the next backend milestone; prefer executable WebGPU work before Metal if it remains the fastest validation path.

## Guardrails

- Product-facing acceptance uses real `upp_Ui` controls where equivalents exist.
- U++ remains authority for layout, input, focus, state and theme.
- Public UI/recorder APIs remain backend-neutral; no Vulkan leakage.
- Do not create one native GPU child per ordinary control or a second layout/theme/state system.
- Do not weaken fallback, validation or ownership-zero assertions.
- Unsupported Draw semantics remain explicit failures until deliberately implemented.
- Diagnose before fixing; smallest coherent change; review complete touched files/package dependencies; run `git diff --check` before publication.

## Recovery Log

BASE: current remote `main`
TASK: finish UI1-C using real product controls
STATUS: foundation PASS; generic popup PASS; stock DropList compatibility PASS; real `UiDropdown` gallery + focused test source present, validation pending
NEXT: focused `UiDropdown` Debug/Release validation, then 20-cycle gallery input acceptance, then menu/context-menu + tooltip
