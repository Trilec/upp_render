# Version 1 release candidate preparation

Updated 2026-10-04. Status: **sustained Windows/Vulkan qualification in progress; clean-machine check and publication remain**.

See [Windows/Vulkan v1 measured results](WINDOWS_VULKAN_V1.md) for the initial baseline. [RC1 qualification](RC1_QUALIFICATION.md) records the
small-image allocation fix, sustained measurements and remaining acceptance.

## Proposed first release scope

Windows/Vulkan accelerated U++ drawing through one public GpuRender package:

- embedded GpuCtrl in an ordinary Ui application;
- custom whole-window GpuWindow;
- root-composited GpuTopWindow with representative ordinary Ui controls and transient UI;
- existing neutral 2D image/text/vector/SVG drawing, with explicit unsupported/fallback behavior;
- compatible device sharing, independent surface lifetime and safe final cleanup.

Vulkan is the executable backend. Metal, WebGPU, browser hosting, compute/effect breadth and complete platform/control coverage remain roadmap items. This bounds the candidate without claiming every future stage is complete.

Generic application shader passes and compute are deferred from v1. The public painter
records 2D drawing; checked-in internal SPIR-V is used without a runtime compiler.
Original bytecode generation provenance is unavailable. See the measured report for limits.

## Candidate gates and present evidence

| Gate | Baseline and current qualification evidence |
| --- | --- |
| Public API examples | Three product demos and three minimal public tutorials rebuilt |
| Whole-Ui application | Gallery native colours, controls, menu/tooltip/modal and maximized layout reviewed |
| Embedded surfaces | Debug/Release 16/16 smoke; native AA toggle, independent pause, hide/show and Randomize reviewed |
| Regression | 34-target Release baseline plus 22 affected final reruns; 12 focused Debug tests |
| Build configurations | Clean Debug, clean Release and clean non-BLITZ Gallery builds |
| Memory and ownership | Byte/entry-bounded caches; five-minute private mean growth 4.6 MB; 100 surface lifetimes and 409,600 image replays; final native ownership ZERO |
| Responsiveness | Five-minute validation load p99 20.28 ms/max 50.66 ms; embedded sibling p99 25.87 ms/max 97.91 ms |
| Distribution | Source/runtime ZIPs, source and binary SHA-256 manifest, dependency pins and third-party notices |
| Branch hygiene | Only main remains; unique historical commits preserved in verified Git bundle |

The initial baseline is in v1-validation.json. Current qualification jobs, including
failed/interrupted attempts, are in rc1-qualification-final.json and the soak/sibling reports.
A tooltip hover attempt failed once and its isolated same-artifact retry passed;
both records are retained. The original candidate passed SDK-free Caro embedded and
normal/load checks; the refreshed final candidate and native review remain pending.
No public tag or release has been published. Local checks do not certify every driver,
Ui control or future backend.

## Memory and load acceptance protocol

Use one reproducible diagnostic harness rather than extra product apps. Fix the random seed, item count, surface sizes, DPI, update cadence and workload duration. Record renderer/Ui revisions, GPU/driver, build mode, validation state, vsync/present policy and hardware.

| Case | Required observation |
| --- | --- |
| One, two and ten surfaces; identical immutable content | Separate per-surface swapchain/frame cost from shared content storage; compatible content uploads once per shared resource/device domain |
| Same surfaces; unique content | Content memory scales with distinct live resources, with configured limits and explicit exhaustion behavior |
| Cold then warm replay | Warm immutable images/glyphs/vectors do not re-upload/rasterize unchanged data |
| Repeated Randomize and content churn | Retained bytes plateau within the configured policy, rather than growing for the renderer's lifetime |
| Resize, DPI changes and hide/show | Bounded retained allocation high-water marks; hidden/paused surfaces avoid needless work |
| Destroy/recreate B while A renders | A stays correct; retired resources reclaim after GPU completion; final live ownership returns to zero |
| One deliberately heavy sibling surface | UI and lightweight surface delay are measured; no unbounded backlog or UI-thread waits |
| Repeated runs | Comparable fixed work and warm-up; no auto-adjusting load that hides regressions |

Report CPU recording/replay time, fence/acquire/present time, UI event/timer delay, per-surface frame age, draw/batch counts, upload bytes/counts, cache entries/hits/misses, live versus cached GPU allocation bytes, retained CPU image/atlas/vector bytes and process private bytes. Process working-set size alone is not GPU memory accounting. Missing GPU timing/byte information must be labelled unavailable, not zero.

Report p50/p95/p99/max, peak/steady retained bytes and post-close counts. Proposed initial interactive target on the declared reference hardware: p99 UI event delay at most 50 ms, maximum at most 100 ms in the agreed sustained workload, with resource caches inside their configured byte budgets. The five-minute Gallery workload passed these targets after adding an image-entry
cap; extended lifecycle and embedded-sibling checks remain required. FPS alone is
not acceptance evidence.

Implement and compare improvements against that baseline: shared immutable content and shader/pipeline identities, bounded cache eviction, reusable staging/buffers, then immutable-frame handoff with finite queue depth and fair scheduling. Keep Ui recording/control access on its owning thread. Retirement must respect GPU completion, not just CPU reference counts.

## Demo/source distribution

The product demo set is GpuSurfaceDemo, GpuUiGallery and RendererShowcase. Retain three minimal public-API source tutorials. RendererShowcaseScene is a dependency of the showcase and its test and must travel with them.

Development diagnostics and tools remain in source but need not be built or bundled as release demo executables. Existing lifecycle/resource probes and deterministic tests are valuable release evidence, not obsolete code to delete.

## Documentation and build audit before a tag

- README: supported platform, ordinary entry points, examples and real limitations.
- GPU_CTRL_USAGE / integration / architecture: names, ownership, fallback and threading agree with current code.
- ACTIVE_WORK: exact current validation boundaries and source combination.
- PROJECT_PLAN / backend and demo roadmaps: accepted work separated from future capabilities.
- Candidate evidence: complete clean Debug/Release build and test results, native input/pixel review, load/memory metrics, shutdown counts and known issues.
- Build instructions: actual dependency nests, Vulkan SDK/compiler prerequisites, local machine path configuration and no dependence on an ignored build/cache folder.
- Artifact/license manifest: public packages and dependencies, product demos, shader sources/generated artifacts and third-party notices.
- Final Git diff and links: no stale package paths, obsolete competing examples, generated logs, local secrets or recovery journals in the release artifact.

Use [BGFX_REVIEW.md](BGFX_REVIEW.md) for the inspected reference patterns. This checklist prepares a concrete candidate; completion and tagging require the evidence above.
