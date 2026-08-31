# upp_Ui / upp_render convergence constraints

This document records architectural constraints for eventual normal `upp_Ui` rendering through `upp_render`.

It is **not** a replacement for the active UI1-C transient/multi-window milestone. Finish current acceptance first and make only small changes now when they clearly prevent structural debt.

## End goal

The long-term target is a fully non-GDI rendering path for normal `upp_Ui` controls while preserving software-only operation.

- `upp_Ui` must not acquire a hard dependency on `upp_render`.
- GPU composition remains opt-in initially.
- ordinary software `upp_Ui` remains independently usable;
- U++ / `upp_Ui` remain authoritative for control hierarchy, layout, input, focus, state, style/theme and invalidation;
- `upp_render` owns recording, replay, GPU resource/presentation policy and backend composition;
- ordinary child controls remain part of one root-composited scene, not one GPU-native surface per control.

Desired dependency direction:

```text
upp_Ui
  -> normal U++ Draw / UiDraw software path

opt-in GPU composition:

U++ / upp_Ui control tree + resolved presentation
  -> GpuRender integration / RenderCtrlBridge
  -> UiCanvas
  -> immutable UiDisplayList
  -> RenderGpu2D
  -> RenderRhi
  -> Vulkan / Metal / WebGPU
```

Do not introduce `upp_Ui -> RenderVulkan`, or an unnecessary permanent `upp_Ui -> GpuRender` dependency. The adapter/compositor belongs on the renderer/integration side where practical.

## Lesson from current UiNodeGraph software benchmarks

Recent equivalent 10 / 100 / 1000-item software measurements rejected a universal "BufferPainter everything" policy.

The measured software strategy is hybrid:

- cheap flat/simple primitives: direct U++ `Draw` is best;
- repeated antialiased/composed visuals: shared cached raster can be dramatically cheaper than regenerating AA work per item;
- one shared `BufferPainter` scene can help some dense unique-vector workloads, but is not the default;
- one dirty item should not cause construction of a much larger AA scene when a direct/cached local path is cheaper.

Representative Release measurements supplied from the Graph work:

- rounded surfaces: cached about 26–30 us/item versus fresh AA about 765–850 us/item;
- ring: cached about 29 us/item versus fresh AA about 806 us/item;
- composed 9-slice: cached about 14 us/item versus direct recomposition about 3700 us/item.

The renderer lesson is **not** "use CPU raster caches on GPU". It is:

> Immutable or repeated presentation work should not be recreated for every item or every frame.

GPU implementations should be free to satisfy the same presentation semantics using shared immutable geometry, instancing, shared textures, glyph atlases, reusable vector/path resources, batching and pipeline/resource reuse.

Software and GPU paths should share semantics, not identical optimization algorithms.

## Keep UiCanvas and UiDisplayList semantic

The current neutral interface lives in the `RenderCanvas` package as `UiCanvas`, with `UiDisplayListBuilder` recording immutable `UiDisplayList` operations.

Preserve semantic operations long enough for replay/resource layers to reason about them. Do not prematurely flatten all presentation into low-level triangles or backend command-buffer concepts.

Useful semantic operations include:

- image;
- text;
- rounded rectangle;
- path fill/stroke;
- gradient paint;
- SVG;
- transform and clip state.

That semantic level allows later batching, instancing, texture reuse, glyph atlas reuse, vector caching and backend-specific clipping choices without changing application/control presentation intent.

Do not leak Vulkan pipelines, descriptor sets, command buffers or other native backend objects into `UiCanvas`, display-list or public control APIs.

## Dirty regions are a first-class contract

GPU composition must remain compatible with local invalidation/damage information.

Examples:

- slider thumb movement damages a bounded slider region;
- caret blink damages the caret region;
- one button state change damages that control region;
- Graph pan can legitimately damage most or all of the Graph viewport.

Replay may choose to batch/replay more broadly when cheap, but the UI integration contract must not discard damage information prematurely.

A future `upp_Ui` GPU integration milestone should explicitly trace U++ invalidation into recording/presentation so partial-damage policy is testable rather than accidental.

## Keep layout/model/preparation above the renderer

GPU acceleration cannot compensate for unnecessary control-side work.

Heavy controls should continue to own/precompute presentation inputs such as:

- geometry;
- routes;
- hit regions;
- spatial indexes;
- LOD decisions;
- prepared item presentation.

Paint/record should consume prepared state. Do not move model, layout, route or hit-test authority into `RenderGpu2D` or a backend simply because the result is GPU-rendered.

## UiCanvas parity requirements to preserve

The current `UiCanvas` vocabulary already provides a strong neutral base: save/restore, rectangular clip, affine transforms, rectangles, rounded rectangles, image, text, arbitrary paths, quadratic/cubic curves, fill rules, solid/linear/radial paints, gradient stops/spread, stroke cap/join/dash and SVG.

The following are important convergence requirements. They do not all need immediate implementation.

### High priority

1. **Image source rectangle / crop**
   - support source + destination rectangles without CPU temporary-image cropping.

2. **Image opacity / tint / modulation**
   - allow state/icon tinting without generating one CPU image per state;
   - distinguish ordinary modulation from monochrome-mask style tint when semantics require it.

3. **9-slice helper**
   - does not need to become a fundamental RHI command;
   - once source-rect image drawing exists, a neutral helper can emit nine image draws and GPU replay can batch them.

4. **Non-rectangular clipping**
   - plan for rounded/path clipping or another sufficiently general semantic mechanism;
   - needed by cards, thumbnails, graph nodes, image surfaces and custom controls.

5. **Logical coordinate / DPI contract**
   - keep control/logical coordinates distinct from physical device pixels;
   - do not encode Vulkan-specific pixel assumptions into control presentation.

### Medium priority

6. **Ellipse / arc / ring convenience**
   - paths can express these;
   - convenience operations belong above the RHI unless a dedicated GPU primitive demonstrates a real advantage.

7. **Group opacity / layers**
   - useful for transitions, animations and compound-control fades;
   - require clear software/GPU composition and lifetime semantics before freezing an API.

8. **Shadows**
   - style semantics remain above the renderer;
   - renderer policy may resolve a shadow to cached texture/resource, geometry, effect/blur pass or deliberate fallback.

## Style remains above the renderer

`upp_Ui` already resolves style through concepts such as `StyledPalette`, `StyledMetrics`, `StyledSkin`, `UiTheme` and `StyledState`.

Do not duplicate those systems in `upp_render`.

Renderer input should receive resolved presentation intent such as rectangles, radii, fills, strokes, images, text and transforms. It should not understand product concepts such as "UiButton hover style" or "selected UiGraph node theme".

## UI1-D — shared immutable resource requirements

The Graph benchmark strengthens the need for a deliberate resource identity/lifetime model.

UI1-D should explicitly consider eventual repeated normal-Ui workloads, not merely cross-window deduplication.

Likely resource classes:

- immutable image/upload identity;
- glyph/font atlas identity;
- reusable vector/path identity;
- SVG/vector-source identity;
- immutable geometry;
- possibly reusable composed surfaces when semantics justify them.

Any shared resource system must define:

- stable identity/keying;
- ownership and reference lifetime;
- context/device compatibility;
- invalidation/versioning;
- synchronization/readiness;
- memory budgeting;
- eviction policy;
- behavior during presenter destruction;
- behavior during device/context loss/recovery.

Do not globalize presenter-owned mutable caches by pointer identity or incidental object addresses. Closing one presenter must not invalidate resources still used by another compatible presenter.

## UI2 — focused upp_Ui integration milestone

After UI1-C transient completion and UI1-D resource identity are stable, create a bounded integration milestone rather than accumulating ad-hoc dependencies.

UI2 should define and validate how normal resolved `upp_Ui` presentation reaches `UiCanvas` / `UiDisplayList` while keeping `upp_Ui` independently usable.

Expected concerns include:

- integration/adapter ownership on the render side;
- damage propagation;
- source-rect images and tint;
- 9-slice;
- non-rect clipping;
- text/glyph resource reuse;
- repeated styled-surface resource reuse;
- LOD/prepared-state boundaries for heavy controls;
- software-reference parity;
- opt-in whole-UI GPU composition without per-control native surfaces.

The exact integration mechanism is deliberately not frozen yet.

## Do not do now

Do not derail UI1-C to redesign the stack.

Do not:

- make `upp_Ui` depend on `RenderVulkan` or require `GpuRender` for normal operation;
- create a competing `UiRenderContext` abstraction in `upp_Ui`;
- force every `upp_Ui` control through BufferPainter;
- convert all drawing helpers immediately;
- create one GPU child/native surface per ordinary control;
- duplicate theme/layout/input/model systems in the renderer;
- prematurely freeze the final `UiCanvas` API while active renderer evidence is still changing it.

## Convergence target

```text
U++ / upp_Ui
control + layout + interaction + style authority
                 |
                 | resolved presentation intent + damage
                 v
              UiCanvas
                 |
        immutable UiDisplayList
                 |
        +--------+--------+
        |                 |
 software/reference     GPU replay
                          |
                      RenderRhi
                     /    |     \
                Vulkan  Metal  WebGPU
```

This document is architectural guidance. Current milestone/acceptance truth remains in `docs/ACTIVE_WORK.md`.
