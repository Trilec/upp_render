# Demo Roadmap

The demos should arrive in the same order as the rendering features they prove.

| Demo | Purpose | Proves | Prerequisite | Interactive | Compare to `RenderSoftware` | Why it matters |
| --- | --- | --- | --- | --- | --- | --- |
| `examples/GpuCtrlLifecycleDemo` | Lifecycle probe | open, retry policy, resize, hide/show, clean close | `GpuCtrl` host/session foundation | Automated | No | Validates control ownership and teardown |
| `examples/GpuCtrlBasicDemo` | Ordinary usage | one embedded control, status/error reporting | `GpuCtrl` ready state + retry hook | Interactive | No | Shows the small default developer path |
| `examples/GpuCtrlMultiViewDemo` | Multiple controls | independent sessions, hide/show, resize, no singleton state | multiple stable embedded controls | Interactive | No | Proves `GpuCtrl` scales beyond one host |
| Clear colour demo | First rendered output | initial frame path and present path | swapchain + presentation | Interactive | No | Confirms the pipeline can show a frame |
| Animated triangle | Basic GPU draw | pipeline, vertex path, frame cadence | clear colour demo | Interactive | No | Sanity-check for dynamic drawing |
| Resize stress | Resize/present robustness | swapchain resize and surface recovery | visible rendering path | Interactive | No | Finds windowing bugs early |
| Texture viewer | Image upload and display | texture handling and sampling | visible rendering path | Interactive | Maybe | Useful for asset preview work |
| Vector shapes | GPU 2D primitives | fills, strokes, transforms, clipping | 2D renderer | Interactive | Yes | Bridges toward ordinary control rendering |
| Mandelbrot | Specialized shader example | shader, uniform, and update cadence | shader pipeline | Interactive | No | Classic demo, useful for capability checks |
| Offscreen rendering | Render-to-texture | offscreen targets and reuse | render targets | Interactive | Maybe | Important for compositing and UI effects |
| Compute buffer example | Compute plumbing | buffer dispatch and results | compute execution | Interactive | No | Keeps compute honest without UI noise |
| UI theme/control gallery | U++/upp_Ui integration | theme-driven control rendering | GPU control renderer | Interactive | Yes | Shows real application value |

## Suggested Sequence

1. Embedded surface and basic status demos.
2. Multi-control and resize stress demos.
3. Clear colour and animated triangle.
4. Texture and vector demos.
5. Offscreen and compute examples.
6. Full UI gallery once the neutral control-rendering path exists.
