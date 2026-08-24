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

On the current Vulkan provider, compatible presenters share one runtime/instance/logical-device domain, queue handles and device-level pipeline cache. Their native surfaces, swapchains, acquired frames, `UiRenderer2D` state and image/glyph RHI handles remain independent.

Closing or resizing one surface therefore does not transfer another surface's presentation ownership. Per-surface submitted/presented work is drained through that surface's graphics/present queues before swapchain destruction; the final compatible presenter release performs the final shared-device cleanup.

## Backend selection

Vulkan is currently the implemented production backend and remains the default package composition for Windows. `SetBackend()` exists for backend selection/testing, but Metal and WebGPU are not yet implemented. Application drawing code should not depend on backend-specific types.
