# upp_render architecture

## Product boundaries

There are three U++ presentation cases and they intentionally share one renderer stack.

### Embedded accelerated content — `GpuCtrl`

A `GpuCtrl` is a bounded native accelerated surface inside an ordinary U++ layout. Standard U++ controls can remain around it. This is appropriate for previews, graph/canvas views, video/image tools and other explicitly accelerated regions.

### Custom whole GPU window — `GpuWindow`

A `GpuWindow` binds the same presentation stack directly to the top-level client area and lets the application draw through `GpuPainter`.

### GPU-composited U++ interface — `GpuTopWindow`

`GpuTopWindow` keeps U++ as authority for the control tree, layout, input, focus, state and theme. Resolved control painting is recorded through `RenderCtrlBridge` into the neutral display list and replayed through one root GPU surface. Ordinary controls are not converted into native GPU child windows.

## Public drawing pipeline

The `RenderCanvas` package exposes the backend-neutral `UiCanvas` recording contract. `GpuPainter` provides the ordinary immediate-looking application API above the same immutable display-list model.

```text
GpuCtrl / GpuWindow
        |
    GpuPainter
        |
UiDisplayListBuilder : UiCanvas
        |
 immutable UiDisplayList
        |
    UiRenderer2D
        |
      GpuRhi
        |
registered backend provider
 Vulkan / future Metal / WebGPU
```

For `GpuTopWindow` the producer changes, not the renderer:

```text
U++ control tree + resolved theme/state
        |
 RecordCtrlDisplayList
        |
 immutable UiDisplayList
        |
    same replay stack
```

Software replay consumes the same display list and remains the semantic/correctness reference.

## upp_Ui convergence direction

`upp_Ui` remains independently usable and must not acquire a hard dependency on `upp_render` or a backend package. A future opt-in whole-UI path should adapt resolved U++ / `upp_Ui` presentation on the render/integration side into `UiCanvas` / `UiDisplayList` rather than creating a competing renderer abstraction inside `upp_Ui`.

Style, layout, model, focus, input and control state stay above the renderer. The renderer receives resolved presentation intent and damage information.

The Graph/UiNodeGraph optimization work reinforces one general renderer rule: repeated immutable presentation work should not be regenerated per item or per frame. GPU implementations may satisfy that through shared immutable geometry, instancing, shared textures, glyph atlases, reusable vector/path resources and batching; software and GPU paths share semantics, not necessarily optimization algorithms.

Keep display-list operations semantic enough for replay/resource layers to optimize. Do not prematurely flatten public drawing intent into backend-specific triangles or command-buffer concepts.

Detailed convergence constraints and parity requirements are recorded in `docs/UPP_UI_RENDER_CONVERGENCE.md`.

## Damage and prepared-state boundary

GPU composition must remain compatible with local U++ invalidation/damage. A caret blink, slider change or one-control state change must not require the integration contract to forget the bounded dirty region, even if a backend later chooses broader replay because it is cheap.

Heavy controls continue to own and prepare geometry, routes, LOD, hit regions and spatial/model state. `RenderGpu2D` and backend providers consume prepared drawing intent; they do not become a second layout/model system.

## `GpuContext` and many surfaces

The application-level context is separate from each presentation surface.

Current compatible Vulkan ownership is:

```text
GpuContext
  provider context
    shared Vulkan runtime / instance
    shared physical + logical device domain
    shared queue handles
    shared VkPipelineCache
       |
       +-- presenter A: UiRenderer2D + surface + swapchain + frame state
       +-- presenter B: UiRenderer2D + surface + swapchain + frame state
       +-- presenter C: UiRenderer2D + surface + swapchain + frame state
```

This takes the useful lesson from U++ `GLCtrl`—reuse expensive backend state across controls—without copying OpenGL's global mutable rendering context.

Image/glyph RHI handles and `UiRenderer2D` caches remain presenter-owned today. UI1-D will only promote resources into cross-presenter sharing after defining explicit immutable identity, compatibility, ownership, invalidation, synchronization, memory-budget/eviction and device/context-lifetime rules. Device compatibility alone is not enough, and accidental pointer identity is not a resource contract.

On Vulkan, per-surface teardown waits only the queues used by that surface before destroying submitted/presented swapchain state. The final release of the shared logical device still uses a device-wide idle boundary before destroying device-owned resources. A never-used swapchain that fails during creation can be destroyed directly because no GPU work references it.

## Backend neutrality and registration

The following public layers contain no Vulkan/Metal/WebGPU objects:

- `GpuPainter`
- `GpuCtrl`
- `GpuWindow`
- `GpuTopWindow`
- `GpuContext`
- `UiCanvas` / `RenderCanvas`
- generic `RenderRhi` contracts

The internal provider registry is defined in `RenderRhi`. `RenderVulkan` implements and registers the Vulkan provider. Generic presentation owns `UiRenderer2D` and logical surface/swapchain orchestration through `GpuDevice`; it does not inspect Vulkan reports or construct Vulkan implementation objects.

`GpuRender.upp` currently pulls `RenderVulkan` so the ordinary Windows/Vulkan build gets a default provider from the one public package. This is package composition rather than public/API coupling; future platform compositions can register Metal or WebGPU providers through the same neutral seam.

Native-window adaptation is a platform/backend concern. The current Win32 path builds a neutral native-window descriptor below the public controls.

## Text, vector and image authority

U++ remains semantic authority where it already has mature behavior:

- U++ font/text metrics and glyph rasterization feed the glyph-atlas path;
- U++ Painter is the vector/gradient/SVG semantic raster authority for the v1 GPU vector path;
- the sampled-image renderer owns GPU upload, texture reuse, affine UV clipping and batching.

A future native GPU vector tessellator is an optimization, not a replacement semantic system.

The `RenderGpu2DBase.inc` and `RenderVulkanRhiBase.inc` implementation layers are intentionally retained for now. Their wrapper and base code share private implementation state within one translation unit; splitting them before final acceptance would require publishing another internal API without reducing runtime ownership complexity.

## Whole-UI constraints

- one root presentation surface per GPU-composited top-level window;
- no native child per ordinary button/label/edit control;
- transient native windows such as popup/menu/tooltip may have their own presentation surface while sharing compatible context/device state;
- no second layout/theme/control-state authority;
- local invalidation/damage remains available to the integration/presentation policy;
- unsupported U++ Draw semantics fail explicitly and fall back rather than silently disappearing;
- GPU presentation failure must leave ordinary U++ software painting available as fallback.

## Future platforms

Metal and WebGPU should implement the same provider/device/surface/replay contracts rather than creating new application-facing painters. Browser/WebAssembly support will additionally require a U++ browser platform host for event loop, input, text/clipboard/IME, timers and top-level surface integration; the renderer architecture is being kept compatible with that direction.
