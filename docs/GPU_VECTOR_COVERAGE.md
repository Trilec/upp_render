# GPU vector and glyph coverage

Updated 2026-10-15. The Windows/Vulkan renderer generates general path and glyph
pixels on the GPU. CPU work prepares bounded edge geometry. The optional engine
uses the existing RHI compute/copy API and embedded SPIR-V; deployed applications
need the graphics driver, without a shader compiler or Vulkan SDK.

## Drawing contract

The convex/straight-stroke fast path is unchanged. General FillPath/StrokePath
supports quadratic/cubic curves, compound nonzero/even-odd fills, open/closed
strokes, all existing caps/joins, dash offset/pattern, linear gradients and focal
radial gradients with Pad/Repeat/Reflect. Rectangular clip and painter order are
preserved by the existing replay geometry.

Fill subdivision has a 0.175 device-pixel bound. Stroke subdivision, dash and cap
geometry use Painter's existing geometry consumers, without its rasterizer.
Row-indexed edge buffers feed GPU winding coverage. Boundary pixels integrate
horizontal intervals at eight vertical samples (32 for fonts); interiors evaluate
once. More than 32 crossings in one pixel uses a bounded point-grid fallback.
Gradients, alpha, premultiplication and sRGB encode/decode execute on the GPU.

GPU textures share the existing 32 MiB / 4096-entry vector cache with the counted
CPU reference assets. Entries used by the active frame remain pinned. Cache keys
include geometry, paint, linear transform, fractional translation and colour-space.
Integer translation reuses exact coverage; solid colour remains draw state.
No full image pixel upload occurs on the GPU coverage branch.

Glyphs keep U++ integer advances, fallback and decorations. Base/mark components
retain separate source-over drawing. Empty glyphs preserve cold and warm advances.
New atlas pages clear on the GPU; cold glyph writes use bounded packed storage
ranges and immutable per-dispatch descriptors. Each batch completes before replay
samples it. A failed frame discards queued writes and unpublished text entries.

## Explicit bounds

- Geometry: 8192 path commands, 4096 flattened points and 8192 expanded edges;
  glyph outline collection is limited to 4096 commands.
- General coverage: at most 4096 pixels per axis and 32 MiB output per operation.
- Row indices: at most 1,048,576 uint32 values; gradients: at most 32 stops.
- Conservative intersection work: at most 1 Gi units including the worst-case
  point-grid fallback. Dash and round-stroke expansion have independent preflights.
- Glyph batching: at most 128 dispatches, 2 MiB geometry payload including range
  alignment and 32 MiB packed output per batch. Large single payloads use the
  bounded immediate route. Device storage alignment/dispatch capacity are checked.
- Four reusable storage buffers grow to bounded power-of-two capacities.
  gpu_vector_scratch_bytes reports retained capacities separately from mask bytes.
  Temporary CPU geometry/staging and backend allocation alignment are separate.

Resources retire after the existing synchronous completion boundary. This does
not introduce asynchronous submission or independent compute queues. Completed
descriptor pools retain their existing bounded reuse policy.

## Qualification

RenderVulkanGeneralVectorTest passes in Release and Debug: 120 declared cases
over RGBA/BGRA UNORM/sRGB, plus transformed gradient numerics, clipped transparency,
exact integer-translation reuse and explicit oversized/dash/stop/cache failures.
Concave and compound fills and curved strokes compare independent software
pixels. Overlapping square dash caps also match an analytic union rectangle
exactly; their crossings can differ from Painter's legacy area accumulator.

RenderVulkanTextTest passes in Release and Debug. The existing small Unicode
test retains maximum 0.03 / mean 0.001 tolerances in all four formats. A larger
222-glyph fixture needs three compute submissions, checks cold/warm pixel identity,
and exercises interrupted-batch recovery. Six crossing pixels differ from the
legacy software accumulator; independent 128 x 128 polygon sampling validates
them within 0.005124. The legacy comparison has a separately declared maximum
0.25 and mean 0.001 bound for this extended fixture.

RenderGpuVectorTest preserves the compute-unsupported Null reference.
RenderVulkanVectorTest explicitly counts the remaining CPU SVG and the required
sRGB gradient variant. Existing direct-path regressions remain applicable.
Native validation and complete ownership cleanup are required by these tests.

Current application: build/GpuUiGalleryVector.exe. Exact artifact/job identities,
source hashes, dependency state and raw reports are preserved in
build/vulkan-vector-glyph-qualification-2026-10-15.json.
See VULKAN_UI_FINISHING.md for the application measurements.

## Remaining work

This closes gate 4's stated general-path/gradient/clip/bounds criterion.
The whole-UI checklist remains 6/10. SVG still uses the explicitly counted CPU
Painter path; remaining Ui shared shapes, shadows and layer helpers need semantic
GPU routes. Full shaped glyph runs/bidi and current visual/input/monitor-DPI
acceptance remain open. Core and Animation were read only.

The Gallery currently observes about 64 callback/record/present updates per
second at its 120 FPS Animation target. GPU coverage does not resolve that
scheduler seam. Process-cold first-frame timing includes driver pipeline/cache
effects and is CPU elapsed time, not a GPU timestamp.

Compute shader authoring and the embedded UINT16/dither demo are separately
qualified. OCIO LUT bindings, asynchronous source-image streaming and video
qualification remain future imaging work; this coverage engine does not certify
them. WebGPU and Metal follow the Windows milestone.
