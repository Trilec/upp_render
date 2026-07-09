# upp_render

`upp_render` is intended to become the rendering layer for Ultimate++.
This repository is currently in the Stage 1 foundation phase.

The first planned GPU backend is Vulkan, but this stage is backend-neutral.
It establishes value types, display-list recording, deterministic inspection,
software replay, tests, and a visual demo before any GPU API is introduced.

## Current packages

- `render/RenderCore`
- `render/RenderCanvas`
- `render/RenderSoftware`
- `render/RenderRhi`
- `render/RenderNull`
- `examples/DisplayListDemo`
- `tests/RenderCanvasTest`
- `tests/RenderRhiTest`

## Build

Build the packages in the Windows `CLANGx64` configuration using TheIDE.
`GitHubOut.var.example` uses `<path-to-uppsrc>` as a placeholder, because the
real U++ source tree is local to each machine and pretending otherwise is how
machines start developing personality.
The expected outputs are:

- `build/RenderCanvasTest.exe`
- `build/RenderRhiTest.exe`
- `build/DisplayListDemo.exe`

## Run

Run the test and demo executables after building.

## Current limitations

- no Vulkan backend yet
- no other GPU backend yet
- no U++ control integration yet
- no text, image, gradient, shadow, or shader pipeline yet
- no speculative backend packages are present
