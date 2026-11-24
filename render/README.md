# render package map

If you are writing an application, start with **`GpuRender`** and normally stop there.

```text
Application / U++ control
        |
     GpuRender
  +-----+-----------------------------+
  | GpuCtrl      embedded GPU surface |
  | GpuWindow    custom GPU window    |
  | GpuTopWindow U++ root compositor  |
  | GpuContext   shared app GPU state |
  +-----------------------------------+
        |
   GpuPainter / RenderCanvas
        |
      RenderGpu2D
        |
       RenderRhi
  neutral device + provider registry
        |
   +----+-----------+
   | RenderVulkan   |   future: Metal / WebGPU
   +----------------+
```

Supporting layers:

- `RenderCore` — small neutral value types.
- `RenderCanvas` — drawing intent, immutable display lists and `GpuPainter`.
- `RenderGpu2D` — GPU geometry/image/text/vector replay.
- `RenderRhi` — neutral resource/command/surface contract plus the internal backend-provider registry.
- `RenderVulkan` — Vulkan `GpuDevice` implementation and registered provider; current platform acceptance backend.
- `RenderSoftware` — software correctness reference.
- `RenderVector` — U++ Painter vector/SVG raster authority.
- `RenderNull` — headless RHI validation.
- `RenderPlatformWin32` — embedded HWND, clipped native child, and frame clock.
- `RenderPlatformCocoa` — embedded AppKit view, Metal drawable, backing-pixel sizing and overlay clipping. UMK selects the native package for the target OS.

These separate packages are not competing user APIs. They preserve dependency direction and make backend work testable without pulling U++ UI integration into the renderer core.

`GpuRender.upp` currently lists `RenderVulkan` so adding the public package on the supported Windows/Vulkan build also links the default provider registration. Generic `GpuRender` source owns no Vulkan implementation objects and talks only to neutral `RenderRhi` provider/device contracts.

For compatible Vulkan presenters the provider context shares runtime/instance/logical-device ownership, queue handles and one device-level pipeline cache. Presenter `UiRenderer2D` state, native surfaces, swapchains, frames and image/glyph RHI handles remain independent unless a future explicit resource-sharing contract says otherwise.

## 3D viewport

`RenderScene`, `Ui3DView` and optional `Ui3DViewUi` provide immutable scene values,
the embedded Vulkan view and native viewport controls. Start with the
[Ui3DView guide](../docs/UI3DVIEW.md) for ownership, API and integration.
