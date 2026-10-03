# Active Work

Remote `main` is authoritative. This file is a recovery checkpoint only, not project history.

## Recovery

- Repository: `Trilec/upp_render`; work directly on `main`.
- Fetch current remote HEAD before work; do not rely on remembered SHAs.
- Windows/U++ validation: `CLANGx64_Vulkan.bm` is tracked as the current build-method baseline; its machine-specific paths need local configuration and a compatible installed Vulkan SDK.
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

- `GpuUiGallery` is a GPU Scene Inspector: real `UiDropdown`, `UiSlider`, `UiMenu`, `UiButton` and `PropertyEditor` drive one live animated custom-Draw scene.
- Dropdown/menu switch Orbit/Flow/Pulse/Swirl; slider changes speed live; PropertyEditor changes particle geometry, grid and colours; modal dialog uses `upp_Ui` controls.
- Debug + Release builds PASS. Menu/submenu GPU ownership re-validated after the `upp_Ui` hardening: `1/1/1 -> 2/2/1 -> 3/3/1 -> 1/1/1`, final ZERO.
- `upp_Ui` hardening during validation: menubar-mode reopen after row activation crashed (stale `PopupLevel` references across event pumps); levels are now closed immediately but destroyed at the next safe teardown point. Regression: `UiMenuInteractionTest` (popup + menubar LeftDown reopen cycles, 26 checks).
- Real-input gallery interactions individually verified (dropdown mode switches, slider drag, PropertyEditor rows/colours, tooltip, modal, resize/restore); consolidated scripted desktop run is environment-sensitive on a shared desktop, so deterministic focused tests are the authority.

## Embedded Surface Demo — 2026-10-03

- TASK: demonstrate the primary product goal: a small Vulkan control inside a normal Ui app.
- TOUCHED: `examples/GpuSurfaceDemo` and `examples/README.md`; existing examples retained.
- STATUS: implementation complete locally; initial light/dark/input review is partial, full native visual acceptance pending.
- VALIDATION: clean Debug BLITZ + Release + non-BLITZ baselines built; latest antialias changes build in all three configurations. Debug/Release native smoke: 16 checks / 0 failures each; exact generated minimal usage compiled unchanged.
- DEPENDENCY FIX: local `upp_Ui/Ui/UiRangeSegmentsPaintParts.cpp` helper renamed to avoid a BLITZ `PaletteInk` collision with UiMediaCard.
- CONTRACT: two per-instance scenes/timers and independent presenters; current painting/presentation is GUI-thread synchronous, not one worker thread per surface.
- ANTIALIAS: Inspector switch defaults on; finite cached Painter coverage sprites replay through Vulkan. Off uses direct single-sample geometry. This is demo-level coverage, not renderer MSAA; no per-frame rasterization or fresh image identities on Randomize at fixed DPI.
- MEMORY: compatible presenters share runtime/instance/device/queues/device pipeline cache; image, glyph and vector caches are per-presenter. General cache budget/eviction and cross-presenter immutable sharing remain UI1-D work; no measured memory/performance acceptance yet.
- PREVIEW: the capture session ended at the Execute preview's 180-second timeout (exit 124), not a recorded crash. Launch the executable directly for an untimed session.
- NEXT: finish visual/interaction review; implement UI1-D identities/lifetimes plus budget/eviction and measured reuse/resident-memory tests; keep async scheduling a separate milestone.
- PUBLISHED: not committed or pushed. Prior accepted renderer milestones below remain historical acceptance.

## Version 1 Preparation — 2026-10-03

- BGFX reference reviewed at `Trilec/bgfx` master `abf165d8a78f962ad05da05f10adf0380bce286d`; see `docs/BGFX_REVIEW.md`. Shader packaging, shared program/geometry across swapchains, deferred resource retirement, budgeted allocation recycling and stress metrics inform the next contracts. BGFX was not built or adopted as a dependency.
- Product demo set: `GpuSurfaceDemo`, `GpuUiGallery`, `RendererShowcase`; three minimal API tutorials retained. Older `GpuEmbeddedMotion` moved to `examples/diagnostics` without source changes; relocated Debug build PASS.
- Documentation: README/examples/demo roadmap reconciled; diagnostics index and `docs/RELEASE_CANDIDATE.md` added. Candidate not ready: measured memory budgets/resource reuse, bounded responsiveness, exact final-source matrix/visual acceptance and reproducible packaging remain open.
- Shader mode: recommended small animated gradient/ripple using the existing shell; public custom-pass/program/uniform ownership is not yet implemented. Current painter demo is not a programmable shader showcase.
- CHECKS: relocated motion Debug build PASS; Release `RenderGpu2DTest` PASS (Null backend warm-image/vertex reuse, not a Vulkan load benchmark); 20 local links across 15 Markdown files valid; `git diff --check` PASS.
- Cleanup is local, uncommitted and unpublished. Recovery journals preserved.

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
STATUS: UI1-C ACCEPTED; Scene Inspector Debug/Release validated; UiMenu menubar reopen crash fixed in `upp_Ui` with regression coverage
NEXT: UI1-D shared immutable GPU resources
