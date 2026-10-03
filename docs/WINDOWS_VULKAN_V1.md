# Windows/Vulkan v1 — local candidate evidence

Updated 2026-10-04. Local candidate implementation, build/regression checks and native reviews are complete.
A separate clean-machine installation and public release remain unperformed.

## Product contract

Use one package, `GpuRender`. GpuCtrl embeds a GPU surface in an ordinary application,
GpuWindow owns custom client content, and GpuTopWindow records the ordinary Ui tree
into a root Vulkan surface. Ui continues to own layout, input, focus, themes and state.

GpuUiGallery is the whole-Ui application: UiDropdown, UiMenu/submenu, UiSlider,
UiButton and PropertyEditor drive an animated, antialiased Painter-vector scene.
GpuSurfaceDemo provides two adjustable embedded surfaces and optional coverage antialiasing.
RendererShowcase retains rendering/reference comparisons.

Whole-Ui means GPU client-area composition. Windows hosting, font discovery and some
platform helpers still use Windows/GDI APIs. This release does not remove those platform
dependencies or claim complete coverage of every control in the Ui library.

## Finished changes

- Image pixel budget: 64 MiB per renderer; old unused entries evict under pressure.
- Vector raster budget: 32 MiB and 4096 entries per renderer.
- Glyph atlas budget: 16 MiB and 8192 entries; exhausted caches reset between frames.
- Current-frame textures stay pinned. An oversized active frame fails explicitly.
- Integer placement is separate from vector/gradient shape identity; fractional phase
  remains significant. Animated translated shapes reuse raster images and GPU textures.
- Immutable Image serial/size/format identities share one Vulkan image/memory allocation
  across compatible presenters. Handles remain independent, immutable writes are rejected,
  and allocations disappear after the last live lease and GPU completion.
- GpuTopWindow supports `SetAsyncPresentation()` before opening. The GUI records immutable
  frames; a worker replays/presents. One pending frame is replaceable, with drop counters.
  Close cancels pending work and joins before HWND destruction.
- Public presenter transactions serialize shared queues/pipeline-cache access.
- Colour-correct replay: U++ BGRA image bytes match the sampled texture format; authored
  RGB and text tint decode for sRGB targets. UNORM targets preserve encoded values.
  Image cache identity includes sampling colour space; warm variants reuse their textures.
- Embedded smoke waits use steady-clock duration because GuiSleep may return early.
- Native allocation diagnostics use VkMemoryRequirements-sized explicit allocations.
  Shared image allocations are counted once separately from exclusive adapter allocations.

Mutable glyph atlases and vector metadata remain presenter-owned. Generic shader/compute,
Metal/WebGPU, browser/mobile hosting, comprehensive Ui coverage and independently scheduled
GPU queues remain future scope. Embedded/custom/transient presentation remains synchronous.
An asynchronous root protects UI event processing; it does not guarantee animation FPS or
isolation from every synchronous sibling/transient operation.

## Measurement protocol and source combination

- Renderer base: `d2a96a0dcc8c3a1400eac87cb94cf61a18ea4e28`; finishing changes are included in the candidate source snapshot.
  The exact candidate commit and all source hashes are recorded in candidate-manifest.json.
- Ui: `b86e59849adfc6992d08ad3eaa09f90b2839c63d`, clean checkout.
- Animation: `4a01b6f4e2a9f122ea1a93457b62a4054d01f970`.
- U++ 18468, clang 21.1.1, C++17, Windows x64, Vulkan SDK 1.4.350.0.
- Reference GPU: NVIDIA GeForce RTX 4070 Ti, vendor 4318, driver raw 2480242688,
  Vulkan API raw 4211013.
- Gallery: 1220x760 initial client layout, deterministic Orbit scene; 4 s warmup,
  20 s measured phase, 16 ms UI-timer probe. Normal 96 / load 512 particles.
- UI delay is timer overshoot, not a physical-input hardware latency measurement.
  CPU acquire/replay/present elapsed time includes blocking GPU/driver waits;
  GPU timestamp timing is unavailable.
- Target: measured p99 UI delay <=50 ms and maximum <=100 ms.
- Churn: 64 frames of unique 32x32 RGBA images, 1/2/10 hidden Vulkan sessions,
  64x64 offscreen targets, validation enabled, fixed 16 KiB image budget per renderer.
  This test measures real allocations/replay/cleanup; it is not a swapchain FPS test.

## Measured results

| Gallery case | p99 UI delay | Maximum UI delay | Peak process private bytes | Result |
| --- | ---: | ---: | ---: | --- |
| Initial 96-particle baseline | 274.29 ms | 274.29 ms | 1,560,522,752 | Fail |
| Integer-shape reuse, 96 particles | 27.63 ms | 32.64 ms | 271,810,560 | Pass |
| Worker replay, 512 particles, validation off | 26.07 ms | 30.81 ms | 516,407,296 | Pass |
| Final clean Release, 96 particles | 18.88 ms | 29.99 ms | 271,323,136 | Pass |
| Final clean Release, 512 particles, validation on | 18.72 ms | 29.84 ms | 570,605,568 | Pass |

The final validation-enabled load presented 178 frames and replaced 937 stale frames
in 24 seconds, retaining at most one pending frame. CPU replay-cycle p99 was 187.91 ms;
the application remained responsive while replay was busy. Final native ownership was ZERO.
These runs are different declared workloads; compare normal baseline/reuse directly,
and compare the synchronous versus worker 512-particle case separately.

The synchronous 512-particle result before the worker was p99 75.00 ms, max 85.03 ms:
it missed the same p99 target. No target was relaxed to claim a pass.

| Churn case | Retain-all peak image payload | Bounded peak image payload | Bounded native allocation peak |
| --- | ---: | ---: | ---: |
| 1 surface | 266,240 bytes | 16,384 bytes | 36,864 bytes |
| 2 surfaces | 532,480 bytes | 32,768 bytes | 73,728 bytes |
| 10 surfaces | 2,662,400 bytes | 163,840 bytes | 368,640 bytes |

Identical cold content uploads exactly once for every surface count, occupying one 4096-byte
shared native image allocation. Closing the first presenter preserves warm drawing in survivors.
All six churn cases finish with zero native ownership and zero validation warnings/errors.

Payload budgets are not total VRAM budgets. Swapchain/driver allocations, pipelines,
command/descriptors and temporary staging are outside these explicit retained byte counters.
Process private bytes is a separate host-memory measurement.

## Reproduce the checks

Build with the tracked CLANGx64_Vulkan.bm after configuring its local paths.
Use nests for renderer render/tools/examples/tests/root, upp_Ui, upp_animation and uppsrc.

```text
umk <nests> GpuUiGallery CLANGx64_Vulkan.bm --out-dir <cache> -abr +GUI <output>/GpuUiGallery.exe
GpuUiGallery.exe --benchmark
GpuUiGallery.exe --benchmark-load --validation
umk <nests> GpuResourceLoad CLANGx64_Vulkan.bm --out-dir <cache> -br +CONSOLE <output>/GpuResourceLoad.exe
GpuResourceLoad.exe
```

Gallery benchmark modes close themselves and write GpuUiGallery-normal.txt /
GpuUiGallery-load.txt next to the executable. Untimed interactive use has no benchmark argument.

Internal SPIR-V arrays are checked in under RenderGpu2D. Builds consume those pinned arrays;
no shader compiler is required at runtime. The original bytecode generation/compiler provenance
is not reconstructed by this candidate. Slang/BGFX are references, not dependencies.

## Current verification record

- Clean final Release gallery build: PASS.
- Image churn/warm/exhaustion/recovery and vector translation/gradient/subpixel regressions: PASS.
- Real Vulkan shared-image/churn/close-first/final-ZERO test: PASS.
- Async root resize/close/reopen and fallback/retry: PASS.
- 34-target Release baseline: PASS; 22 targets affected by the final colour correction were rerun and PASS.
- Clean Debug gallery and 12 focused Debug tests: PASS; embedded smoke 16/16 PASS.
- Clean non-BLITZ Release gallery, all three product demos and three public tutorials: PASS.
- Final-source gallery native review: PASS for colours, PropertyEditor grid/count,
  keyboard adjustment, dropdown/menu/tooltip, modal close/state and maximized layout.
- Embedded demo native review: PASS after app approval; independent pause, AA off/on,
  secondary hide/show, Randomize and normal close observed.
- Candidate packaging records source/runtime hashes, dependency pins and third-party notices.
  ZIP CRC and every entry SHA-256 are checked on creation; see candidate-manifest.json.
- One final Release tooltip run missed hover activation; an isolated same-artifact retry
  passed all four show/hide cycles. Both results are retained in v1-validation.json.
- A separate clean-machine install/run check has not been performed.

## Branch archive

Only main remains locally and remotely. The 24 remote historical branches and one local
branch had unique commits; they were archived before explicit user-requested removal.

Archive: `build/branches-before-cleanup-2026-10-03.bundle`.
SHA-256: `c00c2ae7060ce55f7d09ca76f5a5d7c8828aca25c93b4bb1918021c351934cec`.

`git bundle verify <archive>` confirms complete history. Recover a chosen ref by fetching
that ref from the bundle into a temporary local branch. Preserve the archive when clearing builds.
