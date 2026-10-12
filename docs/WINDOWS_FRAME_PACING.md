# Windows frame-pacing findings — 2026-10-13

Core and Animation remain read-only. This records diagnosis and the next
integration boundary; no dependency source replacement or linker override is used.

## Observed rates

GpuUiGallery's instrumented 512-particle/grid run at target 120 reported
Animation callbacks, distinct progress updates, scene recording and presented
frames all at 64.00 Hz. Root recording last/max was 0.62/2.56 ms; enqueue
last/max was 0.01/0.17 ms. CPU acquire/replay/present median/p99 was 7.23/15.54 ms.
An independent GpuCtrl compute content caller sustained about 120 presented FPS,
including a 3840 x 2160 compute output. These rates establish two distinct
observations: the existing particle scene receives only 64 updates per second,
and some heavy replay frames exceed the 8.33 ms target budget. They do not
establish a sole bottleneck or GPU execution time.

Exact executable hashes/jobs and reports:
VULKAN_COMPUTE.md and build/vulkan-compute-qualification-2026-10-13.json.
Subsequent descriptor reuse qualification is recorded separately.

## Read-only source trace

- GpuTopWindow::WindowProc handles its precise native wake by calling
  TimerProc(msecs()) on the Ui thread.
- CtrlCore/CtrlTimer.cpp selects callbacks against the supplied time.
- Animation/Animation.cpp arms a one-shot TimeCallback with integer
  1000 / target_fps (8 ms at 120). Its TickTimer calls RunFrame(msecs()) and
  rearms after callback work completes. Manual Tick also obtains msecs().
- This installed Core/Util.cpp defines msecs(int from) using GetTickCount().
  A steady_clock version is commented out with a previous llvm-mingw concern.

The observed 64 Hz cadence is consistent with the coarse timing seam. A precise
wake alone cannot provide precise animation progress while both scheduling and
state progression consume that clock. Rearming after work can add another limit.
Neither source reading nor the frame-rate correlation proves the complete cause.

## Required next validation

Keep clock policy explicit and preserve monotonicity, wrap handling, pause/resume,
looping and shutdown. A reusable Animation integration should accept an explicit
monotonic time source or externally supplied tick time, rather than overriding a
Core symbol globally. This is a proposed interface boundary for the Animation
owner; no change is made here.

Within the renderer, continue measuring frame recording, enqueue, acquisition,
submission and presentation separately. Pool reuse removes one allocation cost;
GPU timing queries and a bounded completion/fence path are still needed before
claiming asynchronous throughput or diagnosing a GPU limit. Do not increase
presented FPS by replaying unchanged animation states and call that smoother
animation.
