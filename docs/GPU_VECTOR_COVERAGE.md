# GPU vector and glyph coverage

Updated 2026-10-22. The Windows/Vulkan renderer generates general path and glyph
pixels on the GPU. CPU work prepares bounded edge geometry. The optional engine
uses the existing RHI compute/copy API and embedded SPIR-V; deployed applications
need the graphics driver, without a shader compiler or Vulkan SDK.

## Drawing contract

The convex/straight-stroke fast path is unchanged. General FillPath/StrokePath
supports quadratic/cubic curves, compound nonzero/even-odd fills, open/closed
strokes, all existing caps/joins, dash offset/pattern, linear gradients and focal
radial gradients with Pad/Repeat/Reflect, plus angular gradients. Rectangular
clips and painter order are preserved. Bounded GPU composition supports path
clips and 3x3 box blur without CPU masks.

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

RenderVulkanGeneralVectorTest passes in Release and Debug: 152 declared cases
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
RenderVulkanVectorTest verifies semantic SVG replay with zero CPU vector rasters
and the required sRGB gradient variant (seven ordered batches). Existing direct-path regressions remain applicable.
Native validation and complete ownership cleanup are required by these tests.

Current application: build/GpuUiGalleryFinish.exe. Exact artifact/job identities,
source hashes, dependency state and raw reports are preserved in
qualification/history-2026-11-01.zip (entry docs/VULKAN_UI_FINISH_QUALIFICATION.json). Earlier evidence remains in the2026-10-15 manifest.
See VULKAN_UI_FINISHING.md for the application measurements.

## Semantic Painter and composition

SVG and retained Painting now record existing Painter path intent instead of
allocating CPU vector images on the Vulkan branch. Ui helper migration covers
faces, focus, rings, ranges, dropdown content, tags, graph grids and shadows.
56 catalogue controls pass224 drawing cases; eight shadow cases independently
check coloured coverage, cutouts and repeat resource reuse. GPU scalar glyph
coverage remains unchanged and passes the current DirectWrite Unicode tests.

Clip/filter composition has4nested-scope,64operations/frame,32retained-entry,
64MiB pixel-payload and4096axis bounds. Path clips crop targets conservatively;
idle variants reuse textures. Depth/budget failures recover on the next valid
frame. Mask/colour targets preserve the destination colour space. Simple angular,
blur and repeated-image pixel oracles pass on all four formats.

Painter BeginMask, text-on-path and image-pattern strokes fail explicitly.
Image fills bound repetition to64cells/axis and256total. Layer-mask AA is not
a promise of exact legacy per-primitive composition. Software BlurLayer reports
GPU required; native software Ui drawing retains its own fallback routes.

## Remaining work

General GPU drawing and shared-helper gates4/6 pass. The full Windows checklist
is7/10 (70%). The user authorized full shaped text and cluster-correct editing
now; implement that shared layout authority before closing gate5. Current
physical input/AA/monitor-DPI acceptance remains open.

Steady-clock stepping of the existing Animation::State removed the observed64Hz
cadence in this Gallery. Current heavy runs deliver about120distinct updates/s;
the full300second soak passed. See WINDOWS_FRAME_PACING.md and the qualification
manifest. CPU elapsed timings are not GPU timestamps.

WebGPU/Metal and OCIO/asynchronous video remain separate work.
