# Examples

Start here:

- `GpuRenderEmbedded` — minimal `GpuCtrl` inside a normal U++ layout.
- `GpuEmbeddedMotion` — ordinary U++/GDI controls surrounding a lightweight animated `GpuCtrl`; demonstrates the bounded accelerated-surface use case.
- `GpuRenderWindow` — use `GpuWindow` when the whole client area is custom GPU content.
- `GpuRenderUiWindow` — minimal `GpuTopWindow` example for ordinary U++ controls on one root GPU surface.
- `GpuUiGallery` — broader full-UI acceptance/demo: standard U++ controls, `ArrayCtrl`, slider/progress, animated custom `Ctrl`, and a second modal `GpuTopWindow` surface.
- `RendererShowcase` — renderer capability showcase and software/GPU comparison.

The two motion/gallery examples deliberately stay lightweight. `GpuUiGallery` draws its animated panel through ordinary U++ `Draw`, proving the control-recording/root-compositor path. `GpuEmbeddedMotion` draws through `GpuPainter`, proving the separate native child-surface path.

`RendererShowcaseScene` is shared showcase/test scene data, not a separate user-facing API.

Older bring-up, lifecycle and low-level demonstrations live under `diagnostics/`. They are useful for renderer/backend development but are not competing public APIs.
