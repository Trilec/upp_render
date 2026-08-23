# upp_render architecture

## Product boundaries

There are three U++ presentation cases and they intentionally share one renderer stack.

### Embedded accelerated content — `GpuCtrl`

A `GpuCtrl` is a bounded native accelerated surface inside an ordinary U++ layout. Standard U++ controls can remain around it. This is appropriate for previews, graph/canvas views, video/image tools and other explicitly accelerated regions.

### Custom whole GPU window — `GpuWindow`

A `GpuWindow` binds the same presentation stack directly to the top-level client area and lets the application draw through `GpuPainter`.

### GPU-composited U++ interface — `GpuTopWindow`

`GpuTopWindow` keeps U++ as authority for the control tree, layout, input, focus, state and theme. Resolved control painting is recorded through `RenderCtrlBridge` into the neutral display list and replayed through one root GPU surface. Ordinary controls are not converted into native GPU child windows.

## Public drawing pipeline

```text
GpuCtrl / GpuWindow
        |
    GpuPainter
        |
UiDisplayListBuilder
        |
 immutable UiDisplayList
        |
    UiRenderer2D
        |
      GpuRhi
        |
Vulkan / Metal / WebGPU
```

For `GpuTopWindow` the producer changes, not the renderer:

```text
U++ control tree + theme/state
        |
 RecordCtrlDisplayList
        |
 immutable UiDisplayList
        |
    same replay stack
```

Software replay consumes the same display list and remains the semantic/correctness reference.

## `GpuContext` and many surfaces

The application-level context is separate from the presentation surface.

Target ownership:

```text
GpuContext
  backend runtime / instance
  compatible physical/logical device
  queues
  pipelines and shaders
  image/glyph/resource caches
       |
       +-- surface + swapchain: GpuCtrl A
       +-- surface + swapchain: GpuCtrl B
       +-- surface + swapchain: GpuWindow
       +-- surface + swapchain: GpuTopWindow/dialog
```

This takes the useful lesson from U++ `GLCtrl` (share expensive backend state across controls) without copying OpenGL's global mutable rendering context.

Current Vulkan status: `GpuContext::Default()` shares the accepted grouped runtime/instance state. Compatibility-keyed logical-device/resource pooling is active productization work.

## Backend neutrality

The following public layers must contain no Vulkan/Metal/WebGPU objects:

- `GpuPainter`
- `GpuCtrl`
- `GpuWindow`
- `GpuTopWindow`
- `GpuContext`
- `RenderCanvas`
- generic `RenderRhi` contracts

Native-window adaptation is a platform/backend concern. The current Win32 path builds a neutral native-window descriptor below the public controls.

## Text, vector and image authority

U++ remains semantic authority where it already has mature behavior:

- U++ font/text metrics and glyph rasterization feed the glyph-atlas path;
- U++ Painter is the vector/gradient/SVG semantic raster authority for the v1 GPU vector path;
- the sampled-image renderer owns GPU upload, texture reuse, affine UV clipping and batching.

A future native GPU vector tessellator is an optimization, not a replacement semantic system.

## Whole-UI constraints

- one root presentation surface per GPU-composited top-level window;
- no native child per ordinary button/label/edit control;
- no second layout/theme/control-state authority;
- unsupported U++ Draw semantics fail explicitly and fall back rather than silently disappearing;
- GPU presentation failure must leave ordinary U++ software painting available as fallback.

## Future platforms

Metal and WebGPU should implement the same surface/device/replay contracts rather than creating new application-facing painters. Browser/WebAssembly support will additionally require a U++ browser platform host for event loop, input, text/clipboard/IME, timers and top-level surface integration; the renderer architecture is being kept compatible with that direction.
