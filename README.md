# upp_render

`upp_render` is a backend-neutral GPU rendering layer for Ultimate++.

The ordinary application entry point is **`GpuRender`**. Add that package and include:

```cpp
#include <GpuRender/GpuRender.h>
```

Application code does not need HWNDs, Vulkan handles, queue families, swapchains or command buffers.

## Three ways to use it

### 1. Embedded GPU surface

Use `GpuCtrl` inside an ordinary U++ window:

```cpp
GpuCtrl view;
view.SetGpuPaint([](GpuPainter& w) {
    Size sz = w.GetSize();
    w.Clear(Color(24, 31, 45));
    w.FillRect(Rectf(20, 20, sz.cx - 20, sz.cy - 20), Color(62, 112, 214));
    w.DrawText(Pointf(40, 42), "GPU content", SansSerif(24).Bold(), White());
});

TopWindow win;
win.Add(view.SizePos());
win.Run();
```

`GpuCtrl` owns the native embedded presentation surface. The rest of the window can remain ordinary U++/GDI controls.

### 2. Whole custom GPU window

Subclass `GpuWindow` when the full client area is application-owned GPU content:

```cpp
class View : public GpuWindow {
    void GpuPaint(GpuPainter& w) override {
        w.Clear(Black());
        w.DrawText(Pointf(30, 30), "Whole GPU window", SansSerif(28), White());
    }
};
```

### 3. GPU-composited U++ interface

Use `GpuTopWindow` when U++ should keep control/layout/input/theme authority while the resolved control tree is recorded and replayed through one root GPU surface:

```cpp
class MainWindow : public GpuTopWindow {
public:
    MainWindow() {
        Add(button.LeftPos(20, 120).TopPos(20, 32));
        button.SetLabel("Ordinary U++ button");
    }
private:
    Button button;
};
```

## Current rendering capability

The accepted/implemented stack includes:

- immutable backend-neutral display lists;
- software reference replay;
- Vulkan 1.3 bootstrap, surfaces, swapchains and frame presentation;
- filled/stroked/rounded geometry;
- clipping, affine transforms, alpha/source-over and batching;
- sampled images;
- text through U++ font/glyph authority and persistent glyph atlases;
- vector paths, fill rules, multi-stop gradients, stroke styles and SVG through U++ Painter raster authority;
- embedded `GpuCtrl` presentation;
- full-window custom `GpuWindow` presentation;
- U++ control-tree recording and `GpuTopWindow` root composition.

Stage-5 vector/SVG implementation is complete; its final consolidated Windows/Vulkan acceptance is tracked together with the productization validation in `docs/ACTIVE_WORK.md`.

## Shared application GPU context

Ordinary presenters use `GpuContext::Default()`. Compatible Vulkan presenters now share the expensive application/device domain: runtime, instance, physical/logical-device ownership, selected queue handles, and one device-level pipeline cache. Each presenter still owns its own native surface, swapchain and frame lifecycle.

`UiRenderer2D` state and image/glyph RHI handles remain presenter/renderer-owned. They have not been promoted into process-wide caches without an explicit resource identity and lifetime contract.

The generic provider registry lives below the public façade in `RenderRhi`; the Vulkan provider registers itself from `RenderVulkan`. `GpuRender.upp` currently depends on `RenderVulkan` so a normal Windows/Vulkan application still gets the default provider simply by adding the one public package. That is build composition, not a Vulkan dependency in the public drawing or presentation API.

## Packages

Ordinary application developers should normally care about only:

- **`GpuRender`** — public U++ integration façade.

The remaining packages are deliberate renderer/backend layers:

- `RenderCanvas` — neutral display list and `GpuPainter` recording;
- `RenderCore` — backend-neutral value types;
- `RenderGpu2D` — GPU 2D replay engine;
- `RenderRhi` — backend contract and internal provider registry;
- `RenderVulkan` — current Vulkan implementation/provider;
- `RenderSoftware` — correctness/reference renderer;
- `RenderVector` — vector/Painter authority;
- `RenderNull` — headless validation backend;
- `RenderPlatformWin32` — current native-window adapter.

See `render/README.md` for the dependency map.

## Examples

Start with:

- `examples/GpuRenderEmbedded`
- `examples/GpuRenderWindow`
- `examples/GpuRenderUiWindow`
- `examples/RendererShowcase`

Historical bring-up/lifecycle probes are under `examples/diagnostics`; they remain useful for validation and resource-lifetime accounting rather than as competing user entry points.

## Backend roadmap

Vulkan is the current production/validation backend. Metal and WebGPU are first-class planned backends and the public API is intentionally kept free of Vulkan types.

- Metal: macOS first, while keeping the design viable for iOS/iPadOS presentation models.
- WebGPU: native/browser backend, with a longer-term goal of allowing U++ rendering/control intent to run in a WebAssembly/browser host. This requires browser platform/event/input work in addition to the renderer backend itself.

See `docs/BACKEND_ROADMAP.md`.

## Build status

Windows/Vulkan is the currently validated platform. Use the repository `CLANGx64_Vulkan.bm` environment/build method and a Vulkan SDK available to the local U++ toolchain. Platform-specific paths belong in local configuration, not application code.

The productization/H1 implementation is complete in code and awaiting the final consolidated Windows/Vulkan acceptance matrix. For exact accepted SHAs and the active validation boundary, see `docs/ACTIVE_WORK.md`.
