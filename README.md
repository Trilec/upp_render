# upp_render

`upp_render` is intended to become the rendering layer for Ultimate++.
The foundation phase is complete enough to have an actual RHI and a Vulkan
preflight probe, which is a little unsettling but useful.

The first planned GPU backend is Vulkan, but this stage is backend-neutral.
It establishes value types, display-list recording, deterministic inspection,
software replay, tests, and demos before any GPU API is introduced.
`GpuCtrl` now provides the embedded surface/session boundary; general UI rendering
still comes later.

## Current packages

- `render/RenderCore`
- `render/RenderCanvas`
- `render/RenderSoftware`
- `render/RenderRhi`
- `render/RenderNull`
- `render/RenderPlatformWin32`
- `render/RenderVulkan`
- `render/GpuCtrl`
- `examples/DisplayListDemo`
- `examples/GpuCtrlBasicDemo`
- `examples/GpuCtrlLifecycleDemo`
- `examples/GpuCtrlMultiViewDemo`
- `examples/VulkanClearFrameDemo`
- `tests/RenderCanvasTest`
- `tests/RenderRhiTest`
- `tests/RenderVulkanTest`
- `tests/RenderVulkanFrameTest`
- `tests/RenderVulkanClearFrameTest`
- `tests/GpuCtrlPresentationTest`
- `tests/RenderPlatformWin32Test`
- `tools/VulkanProbe`
- `tools/VulkanSurfaceProbe`

## Docs

- `docs/GPU_CTRL_USAGE.md`
- `docs/DEMO_ROADMAP.md`
- `docs/UI_GPU_RENDERING_ARCHITECTURE.md`

## Build

Build the packages in the Windows `CLANGx64` configuration using TheIDE.
`GitHubOut.var.example` uses `<path-to-uppsrc>` as a placeholder, because the
real U++ source tree is local to each machine and pretending otherwise is how
machines start developing personality.
For Vulkan work, use a local build method with `INCLUDE` extended by
`%VULKAN_SDK%\Include`.
The expected outputs are:

- the corresponding executables under `build/`

## Run

Run the test and demo executables after building.

## Current limitations

- Vulkan loader, instance, physical-device selection, logical-device creation,
  and graphics-queue bootstrap are implemented; TASK-007 surface bring-up also
  passes the ten-cycle validation gate
- `GpuCtrl` now owns a private Vulkan surface/swapchain/presentation lifecycle
  with a deterministic retry policy and no test-only public hooks
- explicitly grouped `VulkanSurfaceSession` instances share runtime and
  instance state while retaining per-session surfaces and logical devices;
  default sessions and `GpuCtrl` instances remain isolated
- no general Vulkan 2D rendering backend yet
- private swapchain ownership, explicit frame acquisition/presentation, and the
  first visible clear-colour frame are available through `VulkanSurfaceSession`
- S14 uses Vulkan 1.3 dynamic rendering with a color-attachment clear and no
  shaders, graphics pipeline, render pass, or renderer abstraction
- no other GPU backend yet
- `GpuCtrl` now automatically creates/recreates its private swapchain and presents
  the S14 clear frame from ordinary invalidation/paint events
- unsupported or failed presentation falls back to normal host painting; later
  invalidation can recover without a busy repaint loop
- the public `GpuCtrl` API remains backend-neutral
- no text, image, gradient, shadow, or shader pipeline yet
- no compute API or execution path yet
- no speculative backend packages are present
