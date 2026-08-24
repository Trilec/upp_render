# Demo Roadmap

The ordinary developer path is now represented by the canonical examples:

- `examples/GpuRenderEmbedded` — embedded `GpuCtrl` drawing through `GpuPainter`;
- `examples/GpuRenderWindow` — whole custom GPU client area;
- `examples/GpuRenderUiWindow` — U++ control tree composited through one root GPU surface;
- `examples/RendererShowcase` — interactive rendering-capability/property showcase.

Historical bring-up probes remain under `examples/diagnostics`. They are validation tools rather than competing user APIs: several provide auto-close modes, validation output, lifecycle stress and zero-live-resource accounting that the canonical examples intentionally omit.

| Diagnostic / future demo | Purpose | Proves | Interactive | Compare to `RenderSoftware` |
| --- | --- | --- | --- | --- |
| `examples/diagnostics/GpuCtrlLifecycleDemo` | Lifecycle probe | open/retry/resize/hide-show/clean close | Automated | No |
| `examples/diagnostics/GpuCtrlBasicDemo` | Embedded host probe | ready/error reporting and automatic cleanup evidence | Interactive/auto-close | No |
| `examples/diagnostics/GpuCtrlMultiViewDemo` | Multi-surface probe | independent presentation targets over compatible shared device state | Interactive | No |
| Resize stress | Resize/present robustness | swapchain recreation and survivor isolation | Interactive | No |
| Texture viewer | Image upload/display | texture handling and sampling | Interactive | Maybe |
| Vector shapes | GPU 2D primitives | fills, strokes, transforms, clipping | Interactive | Yes |
| Mandelbrot | Specialized shader example | shader/uniform/update cadence | Interactive | No |
| Offscreen rendering | Render-to-texture | offscreen targets and reuse | Interactive | Maybe |
| Compute buffer example | Compute plumbing | buffer dispatch and results | Interactive | No |
| UI theme/control gallery | U++/upp_Ui integration | theme-driven control rendering | Interactive | Yes |

## Current sequence

1. Keep the four canonical examples small and representative of the public API.
2. Keep lifecycle/multi-surface/resource-accounting probes under `examples/diagnostics`.
3. Complete the consolidated productization/Stage-5 Windows acceptance.
4. Add new specialized demos only when their underlying product capability exists; do not create placeholder API commitments for future effects/compute/backends.
