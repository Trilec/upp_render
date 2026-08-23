# render package map

If you are writing an application, start with **`GpuRender`** and normally stop there.

```text
Application / U++ control
        |
     GpuRender
  +-----+-----------------------------+
  | GpuCtrl     embedded GPU surface  |
  | GpuWindow   custom GPU window     |
  | GpuTopWindow U++ root compositor  |
  | GpuContext  shared app GPU state  |
  +-----------------------------------+
        |
   GpuPainter / RenderCanvas
        |
      RenderGpu2D
        |
       RenderRhi
        |
   +----+-----------+
   | RenderVulkan   |   future: Metal / WebGPU
   +----------------+
```

Supporting layers:

- `RenderCore` — small neutral value types.
- `RenderCanvas` — drawing intent, immutable display lists and `GpuPainter`.
- `RenderGpu2D` — GPU geometry/image/text/vector replay.
- `RenderRhi` — neutral resource/command/surface contract.
- `RenderVulkan` — Vulkan implementation and current platform acceptance backend.
- `RenderSoftware` — software correctness reference.
- `RenderVector` — U++ Painter vector/SVG raster authority.
- `RenderNull` — headless RHI validation.
- `RenderPlatformWin32` — current Win32 native-surface description adapter.

These separate packages are not competing user APIs. They preserve dependency direction and make backend work testable without pulling U++ UI integration into the renderer core.
