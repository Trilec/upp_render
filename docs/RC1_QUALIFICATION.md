# Windows/Vulkan RC1 qualification
Updated 2026-10-04. Qualification is in progress; RC1 is not tagged or published.

## Sustained memory and responsiveness
The earlier 20-second load result was insufficient to establish a memory plateau.
A 120-second 512-particle run without validation exposed private-memory growth:
556,199,936 to 1,131,626,496 bytes in successive samples, peak 1,152,966,656.
UI p99 was 22.12 ms, but maximum delay reached 225.13 ms. Both sustained gates failed.
U++ heap samples stayed around 24–27 MB. Bounded pixel payload alone did not bound
the number of small image objects and their backend allocation/driver overhead.

UiRenderer2D now bounds images to 4096 entries as well as 64 MiB of pixel payload.
Both constraints use the existing unused-entry eviction and current-frame pinning.
A frame exceeding either constraint fails explicitly. A reduced limit trims between frames.

| Final-source run | UI p99 / max | Private peak | Early / late mean private bytes | Result |
| --- | --- | --- | --- | --- |
| 120 s, validation off | 25.43 / 76.08 ms | 481,861,632 | 480,887,808 / 481,667,072 | PASS |
| 300 s, validation on | 20.28 / 50.66 ms | 538,877,952 | 534,057,984 / 538,667,008 | PASS |

The unchanged responsiveness gates are p99 <=50 ms and maximum <=100 ms.
Memory samples start at 30 s and repeat every 10 s. Compare the first four
and last four samples; permitted mean growth is 32 MiB. The 300-second run
grew by 4,609,024 bytes, kept at most one pending frame, and ended with native
ownership ZERO. The reference GPU/toolchain/dependency pins are unchanged
from [Windows/Vulkan evidence](WINDOWS_VULKAN_V1.md).

An initial soak window closed before completing its report; exit zero and cleanup
alone were insufficient evidence. Benchmark modes now initialize a RUNNING report
and reject early closure without complete responsiveness and memory PASS summaries.
The interrupted attempt is not counted as a passed soak.

## Reproduce
```text
GpuUiGallery.exe --benchmark-soak --validation
GpuUiGallery.exe --benchmark-soak --benchmark-seconds=120
GpuResourceLoad.exe --soak --soak-cycles=5
GpuResourceLoad.exe --soak --soak-cycles=5
GpuSiblingLoad.exe
```
Gallery measures 300 seconds after 4 seconds warmup by default. The optional
seconds argument accepts 20–300 for diagnostics. Plateau acceptance requires at
least eight memory samples. The report is GpuUiGallery-soak.txt beside the executable.

GpuResourceLoad --soak performs ten lifecycle cycles of ten real Vulkan surfaces.
Each cycle churns 4096 frames with a four-image entry limit and generous 64 MiB
pixel limit, then tests shared-image survivors and final native cleanup. Use two
five-cycle jobs when a ten-minute execution cap cannot accommodate all ten cycles.
The first monolithic attempt was cancelled after five completed cycles; it is
retained as interrupted evidence and is not counted as a passed full test.

GpuSiblingLoad compares a lightweight one-rectangle embedded surface with a
4096-rectangle sibling, repeatedly destroying/recreating the sibling. Its
60-second report measures UI timer overshoot and lightweight frame gaps.
This is a declared bounded workload, not independently scheduled GPU queues.

## Verification status
- Five-minute validation-enabled root soak: PASS.
- Cache count/byte/active-frame exhaustion and zero-limit regressions: Debug/Release PASS.
- Extended native churn/lifecycle: two five-cycle jobs PASS. In total, 100 surface
  lifetimes and 409,600 image replays; shared cold images upload once per cycle.
  Image payload peak 163,840 bytes and native owned allocation peak 368,640 bytes
  per ten-surface cycle; final ownership ZERO in both jobs. Replay CPU maxima are
  separate from UI responsiveness; this offscreen test has no UI timer acceptance.
- Heavy/lightweight embedded sibling: PASS. Loaded UI p99 25.87 ms/max 97.91 ms;
  lightweight frame gaps p99 45.81 ms/max 90.95 ms, seven recreations, final ZERO.
  The maximum is close to the unchanged 100 ms limit; this bounds the demonstrated
  workload and does not establish arbitrary-load isolation.
- Three product demos, three public tutorials and Debug Gallery rebuilds: PASS.
- Clean non-BLITZ Release Gallery: PASS. Refreshed embedded smoke: 16/16 PASS.
- Refreshed normal/load runs: p99/max 20.70/26.45 ms and 25.00/29.49 ms; final ZERO.
- Packaging requires the complete passing reports and verifies ZIP CRC/entry hashes.
- Separate Windows runtime test: original candidate passes Caro embedded 16/16,
  normal and load modes without SDK/toolchain. Final refreshed candidate remains pending.

## Second-machine check
KlickCaro Render is project-ED940563C935, primary root C:/GitHub/upp_render.
Machine selection relies on the user's chosen connection configuration.
The initial candidate at renderer 5d7ed27 was copied as a runtime ZIP only.
Its SHA-256 is 1c799f5e2df27141a266d47bc27f51546ec87889bc2da17c9d0d0d3ce8a4fff1.
All three extracted executable hashes match its candidate manifest.
The user confirms neither U++ nor Vulkan SDK is installed on Caro. The extracted
original candidate passes embedded 16/16, normal p99 26.71 ms/max 30.76 ms and
load p99 29.54 ms/max 31.09 ms. Both Gallery reports end with ownership ZERO.
This establishes SDK-free execution of the original candidate; final-candidate
sustained and native acceptance is still pending.
The post-soak fix requires a refreshed candidate before final RC1 qualification.

No compiler or Vulkan SDK is needed to run the product executables. An installed
Vulkan 1.3-capable graphics driver/runtime is required. Validation layers are
optional; omit --validation on a machine without them. Confirm Windows/GPU/driver,
compiler/SDK absence, native input/AA/resize/lifecycle and normal shutdown.

## Publication
RC1 remains withheld until the sustained checks and second-machine runtime/native
review pass. Keep source/runtime packages and third-party notices, checksum records
and known limitations together. Do not publish the branch-history bundle as a
product release asset. Only main remains; no extra development branch is required.
