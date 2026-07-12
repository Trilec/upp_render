# upp_render

`upp_render` is intended to become the rendering layer for Ultimate++.
The foundation phase is complete enough to have an actual RHI and a Vulkan
preflight probe, which is a little unsettling but useful.

The first planned GPU backend is Vulkan, but this stage is backend-neutral.
It establishes value types, display-list recording, deterministic inspection,
software replay, tests, and a visual demo before any GPU API is introduced.

## Current packages

- `render/RenderCore`
- `render/RenderCanvas`
- `render/RenderSoftware`
- `render/RenderRhi`
- `render/RenderNull`
- `render/RenderVulkan`
- `examples/DisplayListDemo`
- `tests/RenderCanvasTest`
- `tests/RenderRhiTest`
- `tests/RenderVulkanTest`
- `tools/VulkanProbe`

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

- no Vulkan rendering backend yet
- Vulkan loader/device bootstrap only; no surface or presentation path yet
- no other GPU backend yet
- no U++ control integration yet
- no text, image, gradient, shadow, or shader pipeline yet
- no speculative backend packages are present
