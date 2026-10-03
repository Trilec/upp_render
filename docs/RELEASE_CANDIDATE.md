# Version 1 release candidate preparation

Updated 2026-10-03. Status: **preparing; not RC-ready**.

## Proposed first release scope

Windows/Vulkan accelerated U++ drawing through one public GpuRender package:

- embedded GpuCtrl in an ordinary Ui application;
- custom whole-window GpuWindow;
- root-composited GpuTopWindow with representative ordinary Ui controls and transient UI;
- existing neutral 2D image/text/vector/SVG drawing, with explicit unsupported/fallback behavior;
- compatible device sharing, independent surface lifetime and safe final cleanup.

Vulkan is the executable backend. Metal, WebGPU, browser hosting, compute/effect breadth and complete platform/control coverage remain roadmap items. This bounds the candidate without claiming every future stage is complete.

Application-authored shader rendering is an open scope decision: our RHI has shader modules, while GpuCtrl's public painter records 2D drawing. A small shader mode is recommended after the custom-pass/ownership contract exists. If deferred from version 1, release notes must state that limitation; if included, it must pass the same two-surface/lifecycle/error gates. Do not advertise a generic programmable Vulkan viewport until that API is usable.

## Candidate gates and present evidence

| Gate | Current evidence | Remaining work |
| --- | --- | --- |
| Easy embedded/full-window APIs | Historical accepted foundation/H1; current embedded demo compiled | Build all public tutorials on the exact candidate and verify failure/fallback behavior |
| Product demo | GpuSurfaceDemo Debug/Release 16 checks / 0 failures; non-BLITZ build; emitted minimal usage compiled | Complete current-source visual/input review, including latest antialias mode; build/review gallery and showcase |
| Ui/transient regression | Recorded UI1-C historical acceptance | Clean Debug/Release focused matrix on pinned renderer + Ui sources; zero final ownership |
| Resource reuse and memory budget | Shared device/cache infrastructure; content caches remain per-presenter | UI1-D immutable identity/lifetime; compatible sharing, bounded eviction and byte accounting |
| Responsiveness under load | Current GUI-thread synchronous path; blocking fence/acquire waits | Timed baseline, bounded admission/scheduling, fair surfaces and measured UI latency |
| Documentation/demo cleanup | Three product demos indexed; old motion moved to diagnostics; stale demo acceptance wording corrected | Final link/build/package audit and candidate release notes |
| Reproducible build/shader assets | Repository build method exists; internal SPIR-V bytecode present | Document prerequisites, pin dependencies/toolchain and record shader source/compiler provenance |
| Candidate packaging | Not produced | License inventory, source/runtime artifact manifest, hashes and clean-machine install/run check |

These are gate states, not a whole-project percentage. Current changes are local and uncommitted; historical acceptance does not certify the final candidate. No release/tag/package has been published.

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

Report p50/p95/p99/max, peak/steady retained bytes and post-close counts. Proposed initial interactive target on the declared reference hardware: p99 UI event delay at most 50 ms, maximum at most 100 ms in the agreed sustained workload, with resource caches inside their configured byte budgets. These are proposed release targets pending an actual baseline; do not claim a pass from FPS alone or weaken a target silently.

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
