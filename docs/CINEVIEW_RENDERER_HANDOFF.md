# Cineview renderer integration handoff

Inspected 2026-10-06 against the current CineView/CineViewCore sources. This is
an implementation plan; Cineview has not yet been built or qualified with GpuRender.

## First working integration

CineCanvas already paints its `CineFrame::display` with scaled `DrawImage`.
Host the existing canvas and Ui controls in `GpuTopWindow`, selecting
`SetRequireGpu()` and `SetAsyncPresentation()` before opening. Preserve CinePlayer's
worker, bounded CPU cache, cancellation generations, seek and prefetch policy.
Use `SetFrameClock()` for animated Windows roots if precise host wakes are needed;
this still dispatches the existing U++ scheduler on the UI thread.

The Draw bridge retains image identity and source crops, and the renderer owns
device/surface lifetime and bounded immutable GPU image caches. This supplies an
initial composited viewer, including its controls, without backend-specific public
Cineview control APIs. Windows hosting and CPU font/path rasterization remain.

## Changes needed before sustained video acceptance

1. Presentation acknowledgement: Poll currently calls `player.Presented` after
   selecting a CPU frame. The asynchronous root can replace pending recordings.
   Add renderer frame serial/completion information, then acknowledge the specific
   Cine frame after real presentation. Aggregate presented-frame counts alone
   cannot guarantee EveryFrame playback or measure display latency.
2. Streaming uploads: new U++ image identities use immutable texture allocations.
   Cache budgets bound residency but do not establish upload efficiency. Design
   a backend-neutral streaming-image resource with a small reusable texture/staging
   ring and completion fences. Current upload paths include synchronous queue waits;
   measure those before promising concurrent decode/upload or zero-copy import.
3. Colour: decoding currently retains OIIO source samples and produces RGBA8 display
   images with CPU exposure/gamma/colour conversion. Define precision and colour
   semantics before moving this to shaders. HDR/OCIO display processing, arbitrary
   shader interfaces, GPU decoding and external texture imports are not accepted
   renderer features today.

## Acceptance measurements

Measure 1080p/4K at 24/30/60 fps, forward/reverse playback, repeated seeks,
resize, pause/resume and colour revisions. Record actual presentation FPS,
frame drops and seek-to-display latency, separately from decode throughput.
Exercise real-time and EveryFrame policies. Count CPU decode/cache bytes,
GPU textures, reusable staging and frames in flight; check memory plateau and
zero final native ownership. Compare colour output and source/display probe
samples against CPU references. Run the viewer on a driver-only machine.

A shared GPU domain currently serializes queue work; multiple surfaces do not
imply separate GPU queues or unconstrained parallel rendering. Keep control,
decode, playback timing and rendering ownership explicit while adding the
smallest reusable contracts needed for the measured bottlenecks.
