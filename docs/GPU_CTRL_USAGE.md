# GpuRender usage

## Add one package

For ordinary application use add package `GpuRender` and include:

```cpp
#include <GpuRender/GpuRender.h>
```

Do not add `RenderVulkan`, `RenderRhi` or lower renderer packages directly unless you are working on the renderer/backend itself.

## Embedded `GpuCtrl`

```cpp
GpuCtrl gpu;
gpu.SetGpuPaint([](GpuPainter& w) {
    Size sz = w.GetSize();
    w.Clear(Color(25, 30, 40));
    w.FillRect(Rectf(20, 20, sz.cx - 20, sz.cy - 20), Color(70, 120, 220));
});

TopWindow win;
win.Add(gpu.SizePos());
```

Subclassing is also supported:

```cpp
class Preview : public GpuCtrl {
    void GpuPaint(GpuPainter& w) override {
        w.Clear(Black());
    }
};
```

`GpuPainter` records neutral intent. It is not a Vulkan command wrapper.

### Refresh

Call `RequestGpuRefresh()` when application state changes and the control should repaint. There is no implicit busy render loop.

### Diagnostics

- `IsGpuReady()` — presentation backend is ready.
- `GetGpuError()` — most recent configuration/presentation error.
- `RetryGpuInit()` — explicit retry after failed initialization.
- `SetValidation(true)` — request backend validation before opening.

`IsNativeHostReady()` is an advanced host-lifecycle diagnostic rather than normal drawing API.

## Advanced frame ownership

`WhenBuildFrame` remains available when a caller deliberately needs to produce an immutable `UiDisplayList` itself:

```cpp
gpu.WhenBuildFrame = [&](Size size, UiDisplayList& list,
                         Rgba8& background, String& error) {
    UiDisplayListBuilder b;
    background = Rgba8(20, 20, 20, 255);
    b.FillRect(Rectf(10, 10, size.cx - 10, size.cy - 10), Rgba8(80, 130, 220, 255));
    if(!b.Finish(list)) {
        error = b.GetError();
        return false;
    }
    return true;
};
```

When this advanced callback is set it intentionally takes precedence over the `GpuPainter` path.

## `GpuWindow`

Use `GpuWindow` for a top-level window whose client area is entirely custom GPU content. Override `GpuPaint()` or assign `WhenGpuPaint`.

## `GpuTopWindow`

Use `GpuTopWindow` when ordinary U++ controls should remain the logical UI and be composited through a root GPU surface. U++ continues to own layout/input/focus/state/theme.

## Multiple controls and windows

Multiple `GpuCtrl`/window presenters use `GpuContext::Default()` unless an advanced caller opens presentation against a separate context.

On the current Vulkan provider, compatible presenters share one runtime/instance/logical-device domain, queue handles and device-level pipeline cache. Their native surfaces, swapchains, acquired frames and logical image/glyph RHI handles remain independent. Immutable images now share one native allocation under stable image identity while each presenter owns its own handle; mutable glyph atlases remain presenter-owned.

Closing or resizing one surface therefore does not transfer another surface's presentation ownership. Per-surface submitted/presented work is drained through that surface's graphics/present queues before swapchain destruction; the final compatible presenter release performs the final shared-device cleanup.

## Backend selection

Vulkan is currently the implemented production backend and remains the default package composition for Windows. `SetBackend()` exists for backend selection/testing, but Metal and WebGPU are not yet implemented. Application drawing code should not depend on backend-specific types.

## Responsive whole-Ui presentation

Select the worker before opening:

```cpp
MainWindow win; // derives from GpuTopWindow
win.SetAsyncPresentation();
win.Run();
```

Ui still records the control tree on its GUI thread. The worker receives only immutable display lists, replays them, and presents. Each root retains at most one pending frame; newer work replaces stale pending work. Closing cancels pending work and joins the worker before destroying its HWND. Public presenter transactions serialize access to shared Vulkan queues and pipeline caches.

The option is currently on GpuTopWindow. Embedded GpuCtrl, custom GpuWindow and transient presenters keep synchronous semantics. A shared device does not provide independent GPU execution queues or guaranteed frame rates under unlimited load.

`GetGpuStats()` reports presented/replaced/pending frames, CPU acquire/replay/present time and renderer cache/upload counters. It returns a worker snapshot in asynchronous mode. GPU timestamp timing is not implemented.

## Retained memory policy

UiRenderer2D defaults to 64 MiB/4096 images of pixel payload, 32 MiB/4096 vector rasters and 16 MiB/8192 glyph entries. It evicts image/vector entries unused by the current frame. Exhausted glyph atlases reset between frames. Content that cannot fit in one active frame fails with a diagnostic; the normal root fallback policy applies.

These are pixel payload and entry limits, not total driver VRAM limits. Native allocation alignment, vertex buffers, swapchains, pipelines, transient upload staging and driver storage are separate. Backend diagnostics count explicit native allocations; process private bytes are a separate host metric. See [Windows/Vulkan v1 evidence](WINDOWS_VULKAN_V1.md).

## Colour contract

Authored Ui/Draw colours and Image RGB bytes are sRGB encoded. Replay uses matching BGRA texture formats for U++ pixel storage, converts solid/text RGB for an sRGB target, and leaves alpha linear. UNORM targets preserve encoded RGB values. Images retain separate warm sampling variants when target colour space changes. Low-level GpuClearColor is already a GPU-space value; public presentation converts its Rgba8 background for the acquired target.

Ordinary static-library builds explicitly retain the Vulkan presentation provider
through GpuRender; application code does not need provider-registration calls.

## Required GPU acceptance mode

Call GpuTopWindow::SetRequireGpu() before opening when software fallback must be
considered a failure. GetGpuError() retains startup/record/replay failures; explicit
RetryGpuInit() is needed to retry a failed root. Owned transient presenters inherit
the requirement and propagate failures to the root. Default applications retain
software fallback. GetSoftwareFallbackCount() counts root native paint dispatches
to TopWindow software painting, not CPU rasterization or total GDI use.

GpuUiGallery --require-gpu --benchmark exercises this mode and writes gpu_required
and software_fallback_count in its report. Required runs write the separate
GpuUiGallery-required-normal/load/soak.txt reports. This mode still uses Windows hosting
and font APIs. See [UI2 drawing audit](UI2_DRAWING_AUDIT.md) for remaining portability work.

## Image crops, colour masks and opacity

DrawImage(destination, image, source, tint, alpha_mask) records an integer source
rectangle and retains the original image identity. Source pixels must be inside
the image and nonempty; empty images are no-ops. tint alpha is opacity. With
alpha_mask=true, tint RGB replaces image RGB and sampled alpha supplies coverage;
otherwise tint RGB modulates sampled RGB in the target working colour space.
Linear filtering clamps to the selected crop's edge texel centres. Full images
keep a single quad; crops use at most nine quads that batch under one texture.

```cpp
w.DrawImage(Rectf(0, 0, 32, 32), icon, Rect(0, 0, 16, 16),
            Rgba8(80, 140, 220, 128), true);
```

The U++ Draw bridge records scaled destinations directly, avoiding inherited
CPU rescaling and per-colour image creation. Image/mask pipelines share one
original upload and vertex shader; mask colours batch together. Shader bytecode
ships with GpuRender; application users need no shader compiler.

Image filtering uses premultiplied colour to avoid dark transparent edges;
opacity multiplies colour and alpha together. sRGB uploads premultiply in linear
light before encoding. Glyphs and coloured masks use alpha coverage separately.
GPU readback acceptance covers RGBA/BGRA UNORM/sRGB targets; the synchronous
Vulkan ReadTexturePixels diagnostic is bounded to 64 MiB and is for checks,
not per-frame UI presentation.

## Animated Windows roots and FPS

Call `SetFrameClock()` before opening an animated `GpuTopWindow`. It opts into
high-resolution Windows timer wakes (Windows 10 1803 or later), coalesces them
to one queued message, and dispatches the existing U++ timer scheduler on the
UI thread. The waiting thread never paints or accesses controls. Close and GPU
failure stop the clock; retry starts a new generation and rejects stale wakes.
Clock creation failure is reported rather than silently using coarse timing.
Static roots leave this opt-in disabled. `IsFrameClockActive()` reports status.

GpuUiGallery uses upp_animation at 60 Hz and shows actual presented FPS,
sampled over approximately one second. Replay milliseconds are CPU timing;
neither value is a GPU timestamp. `--particles=512` starts the interactive load
scene; the inspector permits 6–512 particles. `--no-grid` disables the grid for
comparison. Benchmark reports record grid state and measured presentation FPS.
