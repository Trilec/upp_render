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
- Real `upp_Ui::UiDropdown`: Debug/Release PASS; pointer `20/20`; keyboard `8/8`; reopen regression covered.
- Real `upp_Ui::UiMenu`: Debug/Release PASS; root/menu/submenu ownership `1/1/1 -> 2/2/1 -> 3/3/1 -> 1/1/1`, shared device, final ZERO.
- Real U++ tooltip attached to an `upp_Ui` control: Debug/Release PASS; tooltip `2/2/1`, hide `1/1/1`, final ZERO.
- Consolidated gallery smoke and renderer regression matrix: PASS; final Vulkan ownership ZERO.

## Product Example

- `GpuUiGallery` is evolving post-acceptance into a useful `upp_Ui` GPU Scene Inspector rather than a static control gallery.
- Real `UiDropdown`, `UiSlider`, `UiMenu`, `UiButton` and `PropertyEditor` drive one live animated custom-Draw scene.
- Dropdown/menu switch Orbit/Flow/Pulse/Swirl; slider changes speed live; PropertyEditor changes particle geometry, grid and colours; modal dialog uses `upp_Ui` controls.
- Source is published; Windows build/runtime validation is the current bounded task. This does not reopen accepted UI1-C.

## Next Milestone

UI1-D — shared immutable GPU resources:

- context-owned immutable image/glyph/vector resource identities;
- share only resources with explicit identity/lifetime contracts;
- prove closing one presenter/window cannot invalidate another.

Durable guidance: `docs/UPP_UI_RENDER_CONVERGENCE.md`.

## Repository Hygiene

- Historical cleanup deleted only branches proven merged into `main`.
- Unique historical branches remain preserved; do not force-delete them.

## Guardrails

- U++ / `upp_Ui` remain authority for hierarchy, layout, input, focus, state, theme/model and invalidation.
- No native GPU child per ordinary control; public recording/UI APIs remain backend-neutral.
- Product-facing examples use real `upp_Ui` controls where equivalents exist; stock controls remain compatibility coverage only.
- Diagnose first; smallest coherent change; review touched dependencies/tests; run `git diff --check`.

## Recovery Log

TASK: validate the published `GpuUiGallery` GPU Scene Inspector
STATUS: UI1-C ACCEPTED; Scene Inspector source published, Windows validation pending
NEXT: validate useful live `upp_Ui` -> scene interactions, then begin UI1-D
