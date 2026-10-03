# BGFX reference review

Reviewed 2026-10-03 against the user's [Trilec/bgfx fork](https://github.com/Trilec/bgfx), master at `abf165d8a78f962ad05da05f10adf0380bce286d`. This is source/design review, not a BGFX build or benchmark. No BGFX dependency or backend replacement has been introduced.

## Useful implementation patterns

| Pattern found in the fork | Relevance to upp_render |
| --- | --- |
| Two submit/render frame objects and semaphore handoff | Record immutable frames on the UI side; render elsewhere under a bounded handoff contract. |
| Resource commands processed before/after rendering, with handle lifetime tracking | Shared immutable resources must survive all referencing presenters and in-flight frames. |
| Per-view framebuffer/swapchain assignment in the windows example | Multiple surfaces can reuse one program and geometry; surface ownership does not require duplicate content or one device per window. |
| Shader binary validation, content-hash lookup and shader reference counts | Reuse compatible shader/program objects with explicit ownership; use collision-safe identity comparison in our implementation. |
| Vulkan allocation recycling with an LRU and byte cap | Bound retained allocations; distinguish live content bytes from reusable allocation-pool bytes. |
| Draw stress with thread/draw/texture controls and CPU/GPU timing | Load tests should distinguish submission cost, state/texture switching and GPU cost, with fixed workloads. |

Evidence: [frame/resource internals](https://github.com/Trilec/bgfx/blob/abf165d8a78f962ad05da05f10adf0380bce286d/docs/internals.rst), [shader creation and references](https://github.com/Trilec/bgfx/blob/abf165d8a78f962ad05da05f10adf0380bce286d/src/bgfx_p.h#L6684), [allocation recycle budget](https://github.com/Trilec/bgfx/blob/abf165d8a78f962ad05da05f10adf0380bce286d/src/renderer_vk.cpp#L5409), [multiple windows](https://github.com/Trilec/bgfx/blob/abf165d8a78f962ad05da05f10adf0380bce286d/examples/22-windows/windows.cpp), [draw stress](https://github.com/Trilec/bgfx/blob/abf165d8a78f962ad05da05f10adf0380bce286d/examples/17-drawstress/drawstress.cpp).

The windows example creates its vertex/index buffers and program once, then submits them to views with separate framebuffer targets. On destruction it explicitly advances frames before destroying the native window. We should preserve that ordering principle using our own completion/lifetime contract, rather than copying a fixed number of frame calls.

The Vulkan LRU recycles released device allocations. It is not an automatic semantic cache that deduplicates every image or font. Its budget is not a suitable default for our small-control workload without measurements.

Thread separation permits overlap, but `frame()` still waits for the previous render frame. A shared renderer can still couple surfaces under heavy GPU work. Our GUI must not block indefinitely on frame admission, and workers must not access ordinary Ui controls. Queue synchronization, per-surface fairness, backpressure, obsolete-frame handling and safe close are separate responsibilities.

BGFX sorts submissions for state efficiency and supports sequential view ordering. Our 2D source-over UI must preserve semantic order wherever reordering would change overlapping/translucent results.

## Shader support and showcase

BGFX's [shader tool documentation](https://github.com/Trilec/bgfx/blob/abf165d8a78f962ad05da05f10adf0380bce286d/docs/tools.rst) describes build-time `shaderc` compilation of its GLSL-like source into backend variants. The [example loader](https://github.com/Trilec/bgfx/blob/abf165d8a78f962ad05da05f10adf0380bce286d/examples/common/bgfx_utils.cpp#L99) selects the backend-specific shader directory; the draw-stress example also demonstrates generated embedded shader headers. BGFX shader binaries carry their own header/metadata; they are not plain SPIR-V modules to pass directly to our RHI.

Our renderer already creates internal SPIR-V shader modules and neutral pipeline IDs. Built-in solid and sampled-image bytecode currently lives in `render/RenderGpu2D/RenderGpu2DBase.inc`. A device-level Vulkan pipeline cache does not by itself share those renderer-owned shader modules or pipeline handles across presenters.

Recommended small showcase: an animated gradient/ripple fragment shader in the existing GpuSurfaceDemo, driven by time/speed/scale/colour inputs. That demonstrates a programmable surface more directly than randomized CPU-authored shapes. Keep two surfaces and prove one program's reuse, independent parameters and safe survivor behavior.

Prerequisites remain open:

- source-controlled shader source, compiler version/recipe, backend artifacts and binding/layout validation;
- neutral program/uniform/resource ownership;
- a public custom-pass contract for GpuCtrl's owned target, with resize, error and teardown behavior;
- failure diagnostics and a deliberate fallback/unavailable policy;
- no per-frame shader compilation or pipeline creation.

[Slang](https://github.com/shader-slang/slang) is an alternative authoring/compiler candidate. Compiler choice does not supply the control lifecycle, renderer integration, memory budget or scheduling model. Adopt one reproducible path first; adding multiple shader toolchains is not necessary for the initial showcase.

## Release implications

Prioritize resource identity, budgets and measurable responsiveness before adding backend breadth. Current images/glyphs/vectors remain presenter-owned; cached image entries persist until renderer close. Existing renderer counters cover several per-frame operations and buffer capacities, but not complete retained CPU/GPU allocation accounting or UI latency.

Use [RELEASE_CANDIDATE.md](RELEASE_CANDIDATE.md) for the proposed Windows/Vulkan version 1 gates and [DEMO_ROADMAP.md](DEMO_ROADMAP.md) for demo scope. Custom shaders remain an explicit open capability until implemented and accepted.
