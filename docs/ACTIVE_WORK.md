# Active Work Status

Remote `main` is authoritative for all active and accepted `upp_render` work. Development proceeds directly on `main`; do not create supervisor feature/review branches unless Curt explicitly requests an isolated experiment.

## Current Accepted Baseline

UI1-R2 accepted source baseline:

`2b64964b89324cdd8ce2e1627c942205f783d896`

The Vulkan productization/UI baseline is PASS / accepted:

- backend-neutral display-list/software foundation: PASS;
- RHI + Null backend: PASS;
- Vulkan bootstrap/resources/presentation: PASS;
- GPU2D geometry, images, text, vector/SVG: PASS;
- backend registry/provider decoupling: PASS;
- compatible surfaces share runtime/instance/logical-device/queue domain and pipeline cache while retaining independent surface/swapchain/frame state: PASS;
- embedded `GpuCtrl`: PASS;
- `GpuTopWindow` root compositor: PASS;
- broad ordinary U++ control-tree recording: PASS;
- second modal `GpuTopWindow`: PASS;
- final live Vulkan ownership: ZERO.

## UI1-A / UI1-B / UI1-R1 / UI1-R2 — PASS

Validated Windows/Vulkan behavior now includes:

- representative U++ controls through one Vulkan root surface;
- ordinary animated custom `Ctrl` through the root recorder;
- `DrawArc`: PASS;
- rotated `DrawText`: PASS;
- destination-invert rectangle / U++ edit caret: PASS;
- atomic root fallback after post-record GPU failure: PASS;
- explicit `RetryGpuInit`: PASS;
- transformed child clipping fixed at GPU2D replay level;
- translated-clip base path and text-aware path: PASS;
- software clip cross-check: PASS;
- non-axis-aligned rectangular-clip boundary remains explicit;
- previous GPU2D switch warning removed;
- 20 repeated cold opens: PASS;
- full `ArrayCtrl`: PASS;
- full particle panel: PASS;
- 60-second animation: PASS;
- resize/minimize/restore: PASS;
- modal dialog open/close 5/5: PASS;
- half-painted/partial root: NOT REPRODUCED after UI1-R2;
- GPU/software flashing: NOT OBSERVED;
- no Vulkan validation errors, crashes or assertions;
- final Vulkan ownership: ZERO.

The former half-painted gallery failure was caused by GPU2D storing `ClipRect` in local coordinates after child transforms. Clips are now resolved through the current axis-aligned transform when established, matching U++ Painter semantics.

## UI1-C — Transient / Multi-window GPU Completion — IMPLEMENTED FIRST SLICE / WINDOWS VALIDATION PENDING

Current source/test checkpoint:

`771d9d42fb092d21910d2cf07bba767eb3115cf8`

Architecture:

- U++ Win32 `Ctrl::PopUp` creates an owned native `WS_POPUP` HWND;
- U++ exposes a process-wide `StateHook`, and `StateH(OPEN)` occurs after the HWND exists;
- `GpuRender` now installs one state hook when the first `GpuTopWindow` is constructed;
- owned `WS_POPUP` top-level controls whose owner chain reaches a ready `GpuTopWindow` receive a `GpuDisplayPresenter` automatically;
- each transient native top-level gets its own surface/swapchain, while compatible presenters share the application `GpuContext` / Vulkan logical device;
- ordinary child controls still do NOT receive native GPU HWNDs;
- transient GPU failure falls back to stable ordinary U++ painting for that popup;
- `GpuTopWindow` exposes read-only backend/validation policy so transient surfaces use the same selected backend policy instead of hard-coding Vulkan.

Focused package added:

`tests/GpuTransientPopupPresentationTest`

It is intended to prove repeatedly:

- one root GPU surface + one popup GPU surface;
- two independent swapchains;
- one shared logical device;
- popup refresh re-records U++ painting;
- popup close releases only its presentation objects;
- repeated popup open/close leaves root GPU presentation alive;
- final close returns Vulkan ownership to zero.

Windows validation of this checkpoint is required before transient popup presentation is accepted.

## UI1-C Continuation After First Gate

Once the generic popup host passes:

1. validate real `DropList` popup presentation in `GpuUiGallery`;
2. validate context/menu popup presentation through the same mechanism;
3. validate U++ tool-tip popup presentation;
4. fix only real semantic gaps discovered by those controls;
5. broaden the Vulkan UI gallery with representative `upp_Ui` controls.

Do not introduce a second layout/theme/state authority and do not create one native GPU child per ordinary control.

## UI1-D — Shared Immutable Resources

After UI1-C is stable:

- define context-owned immutable image/glyph/vector resource identities;
- share suitable resources across compatible windows;
- prove closing one window cannot invalidate another;
- do not globally alias mutable presenter caches without an explicit lifetime contract.

## Backend Order After UI1

Prefer WebGPU before Metal if Windows/browser tooling gives faster executable evidence. WebGPU renderer/provider work remains separate from the browser/WebAssembly U++ platform host.

## Recovery Log

BASE: current remote `main`
WORKING BRANCH: `main`
ACCEPTED BASELINE: `2b64964b89324cdd8ce2e1627c942205f783d896`
ACTIVE TASK: UI1-C — transient/pop-up native top-level GPU presentation
SOURCE CHECKPOINT: `771d9d42fb092d21910d2cf07bba767eb3115cf8`
STATUS: UI1-A/B/R1/R2 PASS / ACCEPTED; UI1-C generic owned-popup presenter implemented, Windows validation pending
NEXT: Windows validate `GpuTransientPopupPresentationTest`, then real DropList/menu/tool-tip acceptance using the same presenter seam.
