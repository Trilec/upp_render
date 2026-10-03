# Active Work

Use `main`; candidate source identity is recorded in the artifact manifest. This file is a recovery checkpoint, not project history.

## Recovery

- Repository: `Trilec/upp_render`; work on `main`.
- Current source: `main`; qualification baseline `5d7ed27`; exact candidate commit is in its manifest.
- Ui dependency: `b86e59849adfc6992d08ad3eaa09f90b2839c63d`; clean working tree.
- Build baseline: U++ 18468, clang 21.1.1, Vulkan SDK 1.4.350.0, Windows x64.
- After a Ui change, clean rebuild renderer callers with `-ab`/`-abr`.
- Fetch remote main before new work; preserve uncommitted work and recovery journals.

## Accepted Foundation

- UI1-A/B/R1/R2: neutral canvas/list/software reference/RHI, images/text/vector/SVG,
  embedded/root presentation, compatible device domain and ownership-zero cleanup.
- UI1-C: real UiDropdown, UiMenu/submenu and tooltip lifecycle/input; historical
  Debug/Release acceptance. Representative Ui control coverage, not the entire library.
- GpuSurfaceDemo: two adjustable embedded surfaces, Randomize, pause/lifecycle,
  optional cached antialias coverage. Public usage emitted and compiled.
- GpuUiGallery: real Ui controls and PropertyEditor drive a Vulkan-composited window.

## Windows/Vulkan v1 Finishing — 2026-10-04

- Image/vector caches now have pixel budgets and eviction of unused old entries.
- Current-frame resources stay pinned; oversized active content fails explicitly.
- Glyph atlas bytes/entries are bounded; exhausted atlases reset between frames.
- Integer vector placement is separate from shape/gradient identity. Fractional
  coverage phase remains part of the key; moving a shape reuses its raster/texture.
- Immutable images share native allocations across compatible Vulkan adapters;
  logical texture handles remain independent. Mutable glyph atlases stay local.
- GpuTopWindow has opt-in worker replay with one replaceable pending immutable frame.
  Ui recording/control access stays on its owning thread; close joins before HWND teardown.
- Public presentation transactions serialize shared Vulkan queue/pipeline-cache use.
  Embedded/custom/transient paths remain synchronous; no independent queue guarantee.
- Gallery enables the worker. Normal/load timed modes measure UI delay, replay CPU
  time, private bytes, cache payloads and final native ownership.
- Real Vulkan 1/2/10-surface test: identical image uploads once; 4096 native bytes;
  bounded unique-image churn; surviving surface after first close; final ZERO.
- Before shape reuse: 96-particle p99 UI delay 274.29 ms, private peak 1.56 GB.
  After reuse: p99 27.63 ms, private peak 272 MB (pre-final-sharing run).
- Final Release 512-particle run, validation ON: p99 UI delay 18.72 ms, max 29.84 ms;
  178 presented, 937 stale frames replaced, at most one pending; final ZERO.
- GPU timestamps are unavailable. Acquire/replay/present timings are CPU elapsed time.
- Cache churn/warm/exhaustion/recovery, vector translation/gradient phase and async
  resize/close/reopen regressions passed. 34-target Release baseline and 22 affected final reruns passed.
- Clean Debug, clean Release and clean non-BLITZ gallery builds passed.
- Twelve focused Debug tests and Debug/Release embedded smoke (16/16 each) passed.
- All three product demos and three public tutorials rebuilt successfully.
- Native gallery and embedded demo reviews passed after app approval: controls,
  scene colours, antialias switching, independent pause and secondary hide/show.
- Product demos remain SurfaceDemo, Gallery, Showcase; old motion under diagnostics.
- Scope/build/measurement notes: `docs/WINDOWS_VULKAN_V1.md`.

## RC1 Qualification — 2026-10-04

- Qualification started at `5d7ed27`; exact refreshed source identity is in its manifest.
- 120 s heavy run exposed private-memory growth (556 MB to 1.13 GB) and max UI
  delay 225 ms despite bounded pixel payloads; RC1 publication is on hold.
- Added a 4096-image entry cap alongside the 64 MiB payload cap. Retest: 482 MB
  peak, early/late means 480.9/481.7 MB, p99 25.43 ms/max 76.08 ms; final ZERO.
- Five-minute validation soak passed: p99 20.28 ms/max 50.66 ms; private peak
  539 MB, early/late mean growth 4.6 MB. Churn (100 lifetimes) and sibling tests PASS.
- Benchmark now rejects early close without a complete PASS summary.
- Caro Render: `project-ED940563C935`, `C:/GitHub/upp_render`; original runtime
  ZIP verified; SDK-free embedded 16/16 and normal/load PASS. Refreshed package pending.
- Caro needs only a compatible Vulkan graphics driver; no compiler/SDK required.
- Only main remains. Preserve the existing candidate and branch archive.

## Branch Hygiene

- User explicitly requested only main on 2026-10-03.
- All 24 historical remote branches and one local branch contained unique commits.
- Complete verified archive: `build/branches-before-cleanup-2026-10-03.bundle`.
- SHA-256: `c00c2ae7060ce55f7d09ca76f5a5d7c8828aca25c93b4bb1918021c351934cec`.
- Atomic exact-head remote deletion and local removal completed; only main remains.
- Preserve the archive separately when cleaning build outputs.

## Remaining Acceptance

- Initial local acceptance and sustained qualification passed; refreshed Caro acceptance pending.
- Refreshed candidate packages include sustained evidence, hashes and notices; RC1 unpublished.
- Original candidate passes SDK-free Caro checks; refreshed-candidate/native acceptance pending.
- WebGPU/Metal, generic shader/compute APIs and full Ui coverage remain later milestones.

## Guardrails

- Ui owns hierarchy/layout/input/focus/state/theme/invalidation.
- Public application drawing APIs stay backend-neutral.
- Representative GPU control rendering still uses Windows hosting/font APIs.
- Tests and native review are separate evidence. Never infer latency from FPS.
