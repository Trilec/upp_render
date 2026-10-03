# Renderer diagnostics

These packages support renderer development and acceptance. They are excluded from the product demo set, but remain in the source repository.

| Package | Coverage |
| --- | --- |
| `GpuCtrlBasicDemo` | Embedded host readiness/errors, optional auto-close and ownership accounting |
| `GpuCtrlLifecycleDemo` | Retry, configuration rejection while open, resize, minimize/restore, hide/show and close |
| `GpuCtrlMultiViewDemo` | Multiple presenters, compatibility sharing, resize/hide/show and survivor lifecycle |
| `GpuEmbeddedMotion` | Historical motion scene surrounded by stock U++/GDI controls; moved here without source changes |
| `DisplayListDemo` | Low-level neutral display-list inspection and replay |
| `VulkanClearFrameDemo` | Low-level Vulkan frame bring-up and cleanup |

Add `examples/diagnostics` to the U++ assembly nests when building these packages. Moving a package here does not make it part of the ordinary application API.

Keep deterministic tests under `tests` as the acceptance authority. Developer tools under `tools` remain separate. Planned load diagnostics must measure memory and event-loop delay as well as drawing throughput; see [release gates](../../docs/RELEASE_CANDIDATE.md).
