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

## UI1-C — Transient / Multi-window GPU Completion — GENERIC POPUP PASS / REAL DROPLIST BLOCKED

Current published source checkpoint:

`b8e611c4b0fbdd10e62e2a0b9760660493ed814a`

Architecture:

- U++ Win32 `Ctrl::PopUp` creates an owned native `WS_POPUP` HWND;
- U++ exposes a process-wide `StateHook`, and `StateH(OPEN)` occurs after the HWND exists;
- `GpuRender` installs one state hook when the first `GpuTopWindow` is constructed;
- owned `WS_POPUP` top-level controls whose native owner chain reaches a ready `GpuTopWindow` receive a `GpuDisplayPresenter` automatically;
- each transient native top-level gets its own surface/swapchain, while compatible presenters share the application `GpuContext` / Vulkan logical device;
- ordinary child controls still do NOT receive native GPU HWNDs;
- transient GPU failure falls back to stable ordinary U++ painting for that popup;
- `GpuTopWindow` exposes read-only backend/validation policy so transient surfaces use the same selected backend policy instead of hard-coding Vulkan.

Focused package:

`tests/GpuTransientPopupPresentationTest`

Windows validation at the original UI1-C1 checkpoint proved the generic owned-popup path in Debug. Release then exposed a real root-close lifetime bug: ordinary `GpuTopWindow::Close()` could leave the root presenter alive until object destruction because cleanup relied on the later native `PreDestroy()` path.

That lifetime bug is fixed at `b8e611c4b0fbdd10e62e2a0b9760660493ed814a`:

- real non-modal `GpuTopWindow::Close()` now closes its presenter synchronously before `TopWindow::Close()` tears down native state;
- modal `InLoop()` behavior remains owned by `TopWindow::Close()` so the GPU session remains alive until the later real close;
- `PreDestroy()` and the destructor remain safety-net cleanup paths;
- focused diagnostics now print all six live Vulkan ownership counters if final-zero ever fails again.

Gary's UI1-C1-R1 Windows validation on the exact three-file fix reported:

- focused Debug: PASS;
- focused Release: PASS;
- immediate `!win.IsOpen()`: PASS;
- immediate `!win.IsGpuReady()`: PASS;
- root: 1 surface / 1 swapchain / 1 device;
- root + popup: 2 surfaces / 2 swapchains / 1 shared device;
- popup close: 1 surface / 1 swapchain / 1 device;
- repeated popup lifecycle: PASS, four cycles;
- root survives repeated popup open/close: PASS;
- final runtime/instance/debug-messenger/surface/device/swapchain live ownership: all ZERO;
- `RenderCtrlBridgeTest`: PASS;
- `RenderGpu2DTest`: PASS;
- `GpuUiCoverageTest`: PASS;
- `GpuTopWindowPresentationTest`: PASS;
- `GpuCtrlPresentationTest`: PASS;
- `RenderVulkanTest`: PASS;
- no Vulkan validation errors, crashes or assertions reported.

The next real-control gate is currently blocked:

- `GpuUiGallery` real `DropList` acceptance: FAIL;
- after a verified foreground click, no DropList popup window appeared;
- required 20-cycle DropList acceptance therefore stopped at 0/20;
- full-root/particle/modal/resize/caret follow-up smoke was not run after this substantive failure;
- menu/context-menu/tool-tip GPU acceptance has NOT started and must not be claimed.

Important source distinction for the next diagnosis:

- the synthetic `Ctrl::PopUp()` host path is proven;
- U++ `DropList::Drop()` uses `PopUpList`, whose popup opens as an activated/save-bits owned `Ctrl` and may use slide animation before reaching its final size;
- therefore isolate whether the failure is DropList input/hit-testing, real `PopUpList` creation/activation, or transient-host behavior during that real popup lifecycle before changing architecture.

## UI1-C Continuation

Next bounded task:

1. reproduce the real DropList path independently of mouse hit-testing by invoking `DropList::Drop()` programmatically in a focused Windows probe;
2. record whether a second U++ top-level popup actually becomes open, its final rect/visibility, and whether Vulkan diagnostics become 2 surfaces / 2 swapchains / 1 device;
3. compare with an activated synthetic `Ctrl::PopUp()` using the same save-bits/drop-shadow style;
4. if programmatic DropList works, diagnose root-input/MultiButton hit testing rather than rewriting transient presentation;
5. if the real popup opens but never gets a second GPU surface, diagnose the state-hook/owner/attach seam;
6. if it opens and gets a GPU surface but remains invisible/1-pixel/blank, diagnose PopUpList activation/slide-resize presentation;
7. fix the smallest root cause and rerun real DropList 20-cycle acceptance before menu/context-menu/tool-tip work.

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
ACTIVE TASK: UI1-C — real DropList transient/pop-up GPU presentation
SOURCE CHECKPOINT: `b8e611c4b0fbdd10e62e2a0b9760660493ed814a`
STATUS: UI1-A/B/R1/R2 PASS / ACCEPTED; UI1-C generic owned-popup presenter + synchronous root-close lifetime PASS; real DropList gallery gate FAIL / blocker
NEXT: focused programmatic DropList probe to isolate input vs PopUpList creation/activation vs GPU transient attachment, then smallest root-cause fix and 20-cycle DropList acceptance.
