# Windows / Vulkan whole-Ui qualification

Updated2026-10-23. **8 of10 current acceptance gates pass (80%)**.
The implemented drawing and current automated checks pass. Gates7 and10 remain
open for physical keyboard/mouse, actual OS IME candidates, monitor DPI and final
human visual/input/AA acceptance. Current Caro runtime acceptance is separate.

## Current qualified source and executable

Renderer main base993fedfb3a20102797b19dd15a058446af803a11 plus the exact source
hashes in [latency investigation](WINDOWS_VULKAN_LATENCY_INVESTIGATION.json).
Ui:f88aab5150ca7f375b8c4a61a01da57832c82e03, from a verified committed-main export.
Ui main independently advanced to dc789c1 with theme/title-card/gallery updates;
the candidate remains pinned to the tested export. External Core and Animation
were read-only.

Current build/GpuUiGalleryWakeFixed.exe:8,904,192B.
SHA-256:2d45c0120f073d6d26d71d1bcff5910b570a5fccdfd951ef56cf8d1467a9f813.
[Historical and current qualification index](WINDOWS_VULKAN_FINAL_QUALIFICATION.json).

## Latency cause and correction

The renderer's coalesced PostMessage frame pulse could be inspected by
PeekMessage, become old input and then be ignored by MsgWaitForMultipleObjects
while it waited for new input. The pending flag suppressed another notification.
A separate native queue without a U++ utility timer reproduced a202ms timeout.

SendNotifyMessage now delivers a nonqueued asynchronous scalar-generation
notification to the validated owning thread. It never waits for the callback.
One pending notification and one replaceable pending frame remain bounded.
Separate frame/timer guards prevent recursive callback invocation while allowing
ordinary nested popup pumping. Stop joins the clock before HWND teardown.

The exact same regression wakes in2.2ms after the fix, in both Release and Debug.
Nested pumps, callback destruction, required failure/retry and teardown pass.
No external Core or Animation change was needed.

Microsoft documents [old input in message waits](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjects)
and [nonqueued delivery by PeekMessage](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-peekmessagew).
Original361/404ms events had no clock trace; this proves a defect and its fix,
not that every historical outlier had that cause. The pre-fix900s trace retained
an82.48ms UI notification wait with a0.09ms producer wake and0ms sampled UI CPU.

## Implemented and checked

- Client root, modal workspaces and owned popups present through Vulkan. Native
  frames and OS input services remain Windows services.
- Semantic Painter/SVG/vector paths, gradients, masks, shadows, clipping and
  bounded blur use GPU coverage. Shared Ui helpers preserve semantic drawing.
- All56 drawable catalogue controls pass224 enabled/disabled/two-size probes at
  standard scale and224 at doubled application units in the preserved text baseline.
- Current whole-Ui automation passes three workspaces, three compute dialogs,
  file-browser scan/preview/selection/accept/cancel/filter/nested colour popups,
  full/micro pickers,16 picker states, RGB/hex, themes, resize and text composition.
- Workspaces share one device with two surfaces/swapchains; final ownership ZERO.
- Shared DirectWrite/ICU shaping/editing covers Arabic, Indic, mixed bidi, combining
  and supplementary text. Rich labels and UiDoc share native paragraph geometry;
  native IME composition uses a Vulkan overlay.
- Preserved Release/Debug text checks:64 GPU pixels,37 editor,36 styled,
  27 document,38 IME,80 font layers. COLRv0 and PNG/JPEG/TIFF/BGRA assets supported;
  SVG glyphs/COLRv1 excluded. Shared immutable bitmap pixels count once/snapshot.
- Ordinary Ui without a renderer nest still builds in the preserved baseline.
  Existing Animation::State is stepped by a coalesced renderer-owned clock.

## Current measured results

RTX4070Ti, grid enabled, four-second warmup. CPU is elapsed acquire/replay/present
work; GPU timestamps are unavailable. Presented FPS and distinct animation updates
are separate measures.

| Workload | Validation | Distinct Hz | Presented FPS | CPU median/p99 ms | Peak private bytes | Max timer delay ms |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
|96 particles,20s|on|120.00|119.75|5.04/11.60|221,282,304|18.23|
|512 + real menu/tooltip,20s|on|119.55|118.85|6.09/14.25|226,275,328|52.59|
|512 particles,300s|on|120.00|118.85|6.14/13.74|224,542,720|19.20|
|512 particles,300s|off|120.00|123.85|3.75/8.36|208,199,680|18.95|

Both five-minute tests pass unchanged timer p99<=50ms/max<=100ms and memory
growth<=33,554,432B gates, with zero measured latency events over50ms. Validation
early/late private memory:223,115,264/220,366,848B. Production:
193,324,032/203,960,320B, growth10,636,288B. Absolute footprint is not a claimed
reduction over earlier candidates; allocator/driver/process memory is measured,
and payload cache bounds are not VRAM or total-memory measurements.

Warm CPU vector/glyph rasters, glyph/cache/upload misses, helper rasters, software
layers, fallback and legacy GDI font requests are zero. One sampled descriptor
pool is reused; final tracked native ownership ZERO. Maxima that include warmup
still record native tooltip animation around170ms, with nested pumping; this
does not appear as a measured steady-state timer stall.

4K compute20s retest:119.99FPS, CPU2.96/4.96ms, max callback25.45ms, peak236,535,808B;
resource reuse/memory/cleanup pass. This short retest does not prove every older
4K callback outlier resolved and does not qualify OCIO/asynchronous video.

Fresh root/dialog light/dark Vulkan readback passes, with smoothed geometry/text
visually inspected. Readback is not physical display/input certification.
Earlier361/404ms failed soaks and user-closed INCOMPLETE run remain historical.

## Bounds, runtime and remaining acceptance

Per renderer:images64MiB/4096 entries, vector pixels32MiB/4096, glyph pages16MiB/
8192, path geometry8MiB/256, fast coverage1MiB, active vertices32MiB. Clip/filter:
separate64MiB,32 entries,4 nested scopes,64 operations/frame,4096 pixels/axis.
Existing synchronous GPU completion owns retirement; async streaming is later work.

Current runtime:build/upp-render-ui-2026-10-23-wakeup.zip,3,875,443B,
SHA798ce03a6109a709b91a0058eb8f5d8acaa782d2d7fab3d7536501f38340e096.
It contains GpuUiGallery.exe,README.txt,SHA256SUMS.txt,AGENT_TASK.txt.
Fresh share folder:F:/networkshare/upp-render-ui-2026-10-23-wakeup.
Caro has neither U++ nor Vulkan SDK; neither is needed for runtime tests.
Its Robocopy saved identity still needs owner review; current runtime has not run there.

Remaining:physical keyboard/mouse/AA, actual OS IME candidate select/commit/cancel,
100/150/200% monitor changes and current Caro runtime review. Doubled application
units and generated native messages do not replace physical checks.

Unsupported semantic operations fail explicitly:BeginMask,text-on-path and
image-pattern strokes. Repetition is bounded to64 cells/axis and256 total.
WebGPU/Metal/Linux, OCIO/LUTs and asynchronous video remain separate work.
