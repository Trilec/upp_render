# Examples

## Product demonstrations

| Package | Purpose |
| --- | --- |
| `GpuSurfaceDemo` | Embedded Vulkan controls in the Ui shell: adjustable size, randomized shapes, optional antialiasing, pause and second-surface lifetime. See its [README](GpuSurfaceDemo/README.md). |
| `GpuUiGallery` | Whole-window GPU composition of ordinary Ui controls, animated Draw content, menus, dropdowns, tooltip and modal window. |
| `RendererShowcase` | Rendering capabilities and software/GPU comparison. |

These are the product demo set for version 1 preparation. They demonstrate different public use cases. The surface demo's current smoke is not a load benchmark; full current-source visual/regression acceptance remains in [ACTIVE_WORK](../docs/ACTIVE_WORK.md).

## Minimal source tutorials

| Package | Entry point |
| --- | --- |
| `GpuRenderEmbedded` | `GpuCtrl` inside an ordinary U++ layout |
| `GpuRenderWindow` | `GpuWindow` for custom whole-window drawing |
| `GpuRenderUiWindow` | `GpuTopWindow` for the root-composited control tree |

Tutorials remain small copyable source examples, rather than additional showcase apps.

## Development support

`RendererShowcaseScene` is shared scene data used by both the showcase and `tests/RendererShowcaseTest`; retain it wherever either package is built.

Older bring-up/lifecycle probes and the stock-control `GpuEmbeddedMotion` compatibility example are under [diagnostics](diagnostics/README.md). They are development tools, not product demos.

A future shader mode belongs in `GpuSurfaceDemo` after the custom-render contract exists. Load testing belongs in a diagnostic harness with reproducible metrics. See [demo roadmap](../docs/DEMO_ROADMAP.md) and [release gates](../docs/RELEASE_CANDIDATE.md).
