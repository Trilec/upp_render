# Examples

Start here:

- `GpuRenderEmbedded` — put a `GpuCtrl` inside a normal U++ layout and draw with `GpuPainter`.
- `GpuRenderWindow` — use `GpuWindow` when the whole client area is custom GPU content.
- `GpuRenderUiWindow` — use `GpuTopWindow` when ordinary U++ controls should be recorded and composited through one root GPU surface.
- `RendererShowcase` — broader renderer capability showcase and software/GPU comparison.

`RendererShowcaseScene` is shared showcase/test scene data, not a separate user-facing API.

Older bring-up, lifecycle and low-level demonstrations live under `diagnostics/`. They are useful for renderer/backend development but are not competing public APIs.
