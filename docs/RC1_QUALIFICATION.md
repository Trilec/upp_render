# Windows/Vulkan RC1 qualification
Updated 2026-10-04. Qualification acceptance is complete. RC1 publication is tracked separately.

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
- Final separate Windows runtime checks: embedded 16/16, normal, load and five-minute soak PASS.
- Owner manual acceptance on 2026-10-04: AA works, app runs, no observed problems; PASS.

## Second-machine check
The final refreshed binaries were verified against the qualification manifest:
Gallery SHA-256 828a28039a724ea01ae2f9d0646b3ae5f58df379c3cf19335e4477c4fbbaba7c.
Surface SHA-256 0d2ca7c6686ebaca9615d4f8fc49afd5c5d81939ed339287a3fa793ac86e074c.
Showcase SHA-256 41e507e343e5ecc859b9fff5357692d6d8cb39e220f36e3f4573098742c4c30f.

The second machine runs Windows 11 Pro build 26200, Intel i7-13700H and Iris Xe,
driver 31.0.101.3688 (2022-10-06), Vulkan GPU API 1.3.226. Neither U++ nor Vulkan
SDK is installed; no toolchain, SDK or driver installation was needed.

| Final SDK-free run | UI p99 / max | Private peak bytes | Result |
| --- | --- | --- | --- |
| Embedded self-test | 16/16 checks | — | PASS |
| Gallery normal | 29.60 / 31.31 ms | 241,844,224 | PASS |
| Gallery load | 24.73 / 31.97 ms | 561,094,656 | PASS |
| Gallery 300 s soak | 29.74 / 38.36 ms | 667,029,504 | PASS |

The soak has 28 memory samples: first/last four means 568,559,616/561,989,632
bytes, growth -6,569,984 bytes, and final native ownership ZERO. Heavy scene
presentation is approximately 10 frames/s on Iris Xe; no frame-rate gate was
defined. UI responsiveness acceptance does not establish smooth heavy-scene animation.

The first hidden-launch soak presented zero frames and failed; the visible rerun
of the same binary passed with exit zero. Both attempts remain in the evidence.
The remote agent could not initialize Computer Use, so its native checklist was
not performed. The owner then manually ran the app, confirmed aliasing/antialiasing
works and no observed problems, and explicitly accepted PASS on 2026-10-04.
This records the owner's acceptance, without asserting independent completion of
each menu, keyboard, resize or Showcase checklist item. Earlier development-machine
native review covers representative controls, colours, AA and lifecycle.

No compiler or Vulkan SDK is needed to run the product executables. An installed
Vulkan 1.3-capable graphics driver/runtime is required. Validation layers are
optional; omit --validation on a machine without them. Confirm Windows/GPU/driver,
compiler/SDK absence, native input/AA/resize/lifecycle and normal shutdown.

## Publication
Sustained and second-machine acceptance have passed. The RC1 distribution uses the
same qualified executable hashes; final documentation and packaging changes require
no renderer rebuild. The tag/release is a separate publication step. Keep source/runtime packages and third-party notices, checksum records
and known limitations together. Do not publish the branch-history bundle as a
product release asset. Only main remains; no extra development branch is required.
