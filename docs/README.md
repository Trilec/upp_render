# Renderer documentation

## Current Windows/Vulkan

- [Run and use the demo](../examples/GpuUiGallery/README.md)
- [Build on Windows](BUILD_WINDOWS.md)
- [Release checklist](RELEASE_CANDIDATE.md)
- [Exact executable and qualification](qualification/README.md)
- [Physical AA, OS IME and DPI checks](WINDOWS_VULKAN_MANUAL_ACCEPTANCE.md)
- [Current work](ACTIVE_WORK.md)
- [Qualified Ui dependency correction](dependencies/README.md)

## Application and drawing guides

[Usage](GPU_CTRL_USAGE.md) · [Integration](GPU_CTRL_INTEGRATION.md) ·
[Architecture](ARCHITECTURE.md) · [Public-API snippets](snippets/README.md) ·
[Package map](../render/README.md)

[Images](GPU_IMAGE_PIPELINE.md) · [Vector coverage](GPU_VECTOR_COVERAGE.md) ·
[Windows fonts](WINDOWS_FONT_PROVIDER.md) · [Frame pacing](WINDOWS_FRAME_PACING.md) ·
[Compute](VULKAN_COMPUTE.md) · [BGFX lessons](BGFX_REVIEW.md)

## Design and later work

[Backend roadmap](BACKEND_ROADMAP.md) · [UI rendering](UI_GPU_RENDERING_ARCHITECTURE.md) ·
[Ui/renderer convergence](UPP_UI_RENDER_CONVERGENCE.md) ·
[WebGPU architecture](WGPU_ARCHITECTURE.md), [dependency](WGPU_DEPENDENCY.md) and
[implementation plan](WGPU_IMPLEMENTATION_PLAN.md) ·
[CineView handoff](CINEVIEW_RENDERER_HANDOFF.md)

The active 3D design/handoff documents belong to concurrent work and are retained.
WebGPU/Metal, video/OCIO and 3D are not qualified by the Windows UI demo.

## History

Older restart notes, plans, audits, machine reports and qualification records are
compressed in [the history archive](qualification/README.md#history).
They retain their original paths and hashes; they do not select the current executable.
