# Demo Roadmap

## Version 1 demo set

Keep three product demonstrations:

1. `GpuSurfaceDemo`: bounded Vulkan control inside the normal Ui shell.
2. `GpuUiGallery`: ordinary Ui controls and transient UI composed through Vulkan.
3. `RendererShowcase`: supported drawing features and software/GPU comparison.

Keep the three minimal source tutorials for embedded, custom whole-window and root-composited use. The [examples index](../examples/README.md) is the current entry point.

`GpuEmbeddedMotion` has moved to `examples/diagnostics/GpuEmbeddedMotion`. Its earlier motion role is covered by the new surface demo; its stock-U++/GDI compatibility coverage is still useful. Existing lifecycle/ownership probes stay in diagnostics. Do not remove tests or shared `RendererShowcaseScene` support merely because they are not product demos.

## Shader showcase

Candidate: an animated gradient/ripple field in a small surface, with time, speed, scale and colour uniforms. Reuse the existing shell, size controls and two-surface lifetime controls. A fragment shader over a quad is sufficient; Mandelbrot, compute and 3D are not prerequisites.

This is planned, not implemented. First define neutral program/uniform/resource ownership and a custom-pass contract for the owned surface, including resize, failure and teardown. The internal RHI has shader modules and pipelines; the current public `GpuCtrl` painter callback is still a 2D display-list API. Keep shader sources and reproducible compiler recipes alongside generated backend artifacts. Show an explicit unavailable/error state instead of disguising a CPU raster as a programmable shader.

[BGFX review](BGFX_REVIEW.md) records shader packaging, multi-window reuse and scheduling patterns from the user's fork.

## Load diagnostic

The small product demo shows behavior, not performance acceptance. Use fixed-seed, fixed-duration runs with identical and unique images/text/vector content, cold and warm caches, and one/two/ten surfaces. Vary item count and surface size separately.

Collect CPU recording/replay time, fence/acquire/present time, UI event/timer delay, upload bytes/counts and retained memory/resource counts. Report p50/p95/p99/max and final cleanup. Exercise resize, hide/show and close/recreate under load, with a deliberately heavy sibling surface. A render thread still needs backpressure and fair per-surface scheduling.

The existing renderer statistics cover draw/batch counts, cache misses, uploads and vertex capacity in part; they do not yet establish complete resident GPU bytes or UI responsiveness. See [release gates](RELEASE_CANDIDATE.md) for acceptance requirements.

## Current sequence

Productization/H1, Stage 5 and UI1-C have recorded acceptance; they are not unstarted work. Establish fresh evidence on the final renderer/Ui source combination, finish UI1-D shared immutable resources and memory policy, then validate bounded scheduling under load. Complete visual review and documentation/build reproducibility before naming a release candidate.

Offscreen effects, compute demos, Metal/WebGPU and browser hosting remain future scopes.
