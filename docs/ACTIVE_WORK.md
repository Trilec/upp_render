# Active Work Status

This file is the recovery checkpoint for active `upp_render` implementation work.
Update it whenever a coherent checkpoint is published so work can resume from repository state rather than chat history.

## Accepted Baseline

- Branch: `main`
- Stage 3 Vulkan backend/bootstrap: **PASS / 100% accepted**
- Stage-3 Windows acceptance HEAD: `6ab33a42a3421643359cabfdae7afed7628ad349`
- Stage 4 GPU 2D renderer: **PASS / 100% accepted**
- Stage-4 Windows acceptance HEAD: `f8e7b24d510b4b5889370823dc1c0a5ef43a7f54`
- Vulkan framebuffer orientation correction: **PASS / accepted**
- Orientation implementation: `c13783aaad1ce10d4ade5ac8f020c56e876ae5f8`
- Orientation Windows revalidation HEAD: `42d74c7bf44bac5f9ce8c92a3e553946943b8738`
- Stage-5 image foundation: **PASS / accepted**
- Stage-5 image implementation: `a11862d138e6b2f06d92067b4b804d8418b69d32`
- Stage-5 image Windows acceptance HEAD: `f2cd2bdf2ff7c05f7b883ef32405653ab198a98b`
- Stage-5 text/glyph-atlas implementation: `f98cce413b1992cfaef55669d4672824fe703b5f`
- Stage-5 text Windows acceptance HEAD: `91f1fe3cad91b5afe00de4afd6398b773e8f4715`
- TASK-010B-W1: **PASS** — text/software/Null Debug+Release PASS; Vulkan text Debug 4/4 + Release 2/2; GPU2D/image/GpuCtrl regressions PASS; validation 0/0; adapter resources 0; Vulkan ownership `0/0/0/0/0/0`
- TASK-010B-W1 mechanical source corrections: `6f322ad4ecc4b2364d020a00bb3676695cbc9cab`
- TASK-011A shared presenter/root presentation boundary: **PASS / accepted**
- TASK-011A implementation: `a4979f17becfb4af6390314cc316eb1ea31e3c92`
- TASK-011A Windows acceptance HEAD: `cb01a20a283ac18e07121a94ccc90bc3d232d8cf`
- TASK-011B embedded neutral frame source: **PASS / accepted**
- TASK-011B frame-source implementation: `3ac69f1971b5770b08ab9d06b7be72654dabe521`
- TASK-011B focused acceptance package: `947a06038bddb6fd116b00aff1ac697a79eab55b`
- TASK-011B frame-source Windows acceptance HEAD: `e3ad497b0bb0d001287eddd2d90e0fae861e00c7`
- TASK-011B CtrlCore semantic recording bridge: **PASS / accepted**
- TASK-011B implementation: `c4210d80a815950df53df5db9dea45a38edbbfdd`
- TASK-011B Windows acceptance HEAD: `d386ba1aa954ea8d16a58a35170fa9f722be1e78`
- TASK-011C root compositor wiring: **PASS / accepted**
- TASK-011C implementation: `21ca529525455408356c38c4d5a2b8361cf950fd`
- TASK-011C Windows acceptance HEAD: `cb01a20a283ac18e07121a94ccc90bc3d232d8cf`

## Current Objective

The renderer capability stack is substantially complete through Stage 6, but the project is now in a **productization and architecture consolidation pass** before effects/compute or additional backends.

Stage 5 remains **IMPLEMENTATION COMPLETE — FINAL WINDOWS/VULKAN ACCEPTANCE PENDING** through `TASK-010-W1`.
The Renderer Showcase GPU visual/orientation boundary is accepted; a short human control-interaction smoke remains useful but is not an architecture blocker.

The immediate product goal is now explicit:
1. embedded `GpuCtrl` that a normal U++ developer can add and paint without Vulkan/display-list ceremony;
2. custom full-surface `GpuWindow` for an application-owned GPU client area;
3. `GpuTopWindow` for a GPU-composited U++/upp_Ui control tree;
4. a shared application GPU context so multiple surfaces do not imply duplicated expensive backend ownership;
5. one public `GpuRender` package/header, with lower `Render*` packages treated as implementation/advanced layers;
6. preserve backend neutrality so Vulkan, Metal and WebGPU can implement the same presentation/render contracts.

## Productization Block P1 — public painter façade

Published implementation checkpoint: `a33ea54dc08d319798bc40135faddcea250f8aeb`
Status: **IMPLEMENTED — WINDOWS COMPILE/BEHAVIOR VALIDATION PENDING**

Added:
- `render/RenderCanvas/GpuPainter.h`: immediate-style application painter that records into the accepted neutral immutable display list; `Clear()` configures frame clear colour; U++ `Color` convenience overloads are provided while existing `Rgba8` paths remain available;
- `GpuCtrl::SetGpuPaint(Function<void(GpuPainter&)>)`: ordinary embedded drawing no longer requires callers to construct `UiDisplayList` values or know presentation internals; advanced `WhenBuildFrame` remains available for explicit neutral-frame ownership;
- new `render/GpuRender` façade package/header: intended ordinary developer entry point;
- new `GpuWindow`: a custom top-level GPU surface whose application code paints through `GpuPainter`; `GpuTopWindow` remains the separate whole-U++-UI compositor.

This block deliberately does not change the accepted renderer/presentation implementation beneath the new public surface.

## Architecture Audit Findings Driving P2/P3

- current repository has many technically meaningful packages, but ordinary users should not have to choose among `RenderCore`, `RenderCanvas`, `RenderPresentation`, `RenderRhi`, `RenderVulkan`, etc.; `GpuRender` is now the intended public package;
- existing README/architecture/usage/plan documents are materially stale and describe milestones that the live code has already passed;
- current ordinary `GpuCtrl` presenters still open isolated default Vulkan surface sessions; explicitly grouped sessions share Vulkan runtime/instance ownership but still create per-session logical devices;
- U++ `GLCtrl` demonstrates the usability benefit of shared expensive backend state across multiple child drawing surfaces; Vulkan must achieve the equivalent benefit without copying OpenGL's global mutable-context model;
- target shared model is application-level backend context/device/resource ownership plus independent per-window surfaces/swapchains;
- the neutral API must remain suitable for future Metal and WebGPU backends; backend selection/context compatibility belongs below `GpuCtrl`/`GpuWindow` rather than in application drawing code.

## Stage 5 - Text, Images and Vector Rendering

Images and text remain accepted. Vector/gradient/AA/SVG production implementation remains at `0d37b2472c4d49e6908f6acbf5f85cc523193006` with final consolidated Windows/Vulkan acceptance still required by `TASK-010-W1`.

The accepted vector design continues to use U++ Painter as semantic/raster authority and the sampled-image pipeline as GPU transport; native tessellation is a later optimization, not a productization prerequisite.

## Stage 6 - U++ Integration

Accepted underlying boundaries remain:
- shared neutral presenter/root presentation;
- embedded frame source;
- CtrlCore semantic recording bridge;
- root compositor wiring;
- Vulkan orientation correction.

Productization work now sits above those accepted boundaries rather than reopening them.

### Renderer Showcase

Published: `094e8807c70fd591bf7e921a5a98ae7069a8b97f`
Status: **PARTIAL — GPU VISUAL/ORIENTATION + AUTOMATED RENDERER COVERAGE ACCEPTED; SHORT HUMAN INTERACTION SMOKE REMAINS**

Post-orientation evidence at `42d74c7bf44bac5f9ce8c92a3e553946943b8738`:
- RendererShowcase Debug/Release build and launch paths pass;
- authored GPU orientation visually correct;
- RenderVulkanGraphics/Image/Text/Vector Debug+Release pass;
- RenderGpu2D Debug pass;
- GpuCtrlFrameSourceTest Debug pass;
- no Vulkan validation/runtime failures observed;
- validation trees clean.

## Backend Direction

### Vulkan

Vulkan 1.3 remains the first production backend and current validation authority.
Next architecture block must introduce shared application context ownership so multiple `GpuCtrl`/`GpuWindow`/`GpuTopWindow` surfaces can reuse compatible expensive backend state while retaining independent surface/swapchain lifecycles.

### Metal

Metal is a first-class target for macOS and should also leave the architecture viable for iOS/iPadOS. No Objective-C/Metal types may leak into `GpuPainter`, `GpuCtrl`, `GpuWindow`, `GpuTopWindow`, `RenderCanvas` or generic presentation contracts.

### WebGPU

WebGPU is a first-class target, including the longer-term possibility of a U++ application/rendering front end hosted in a browser/WebAssembly environment. Current productization must therefore avoid Win32/Vulkan assumptions in public APIs and keep native-window/presentation adaptation isolated below the façade.

No speculative Metal/WebGPU implementation package should pretend to be functional before a platform bring-up slice exists; architecture seams should be prepared now, implementations added as bounded backend stages.

## Recovery Log

BASE: `a33ea54dc08d319798bc40135faddcea250f8aeb` / `main` (public façade commit created; branch publication to verify)
TASK: Productization P1-P3 — public drawing API, shared GPU context, package/folder cleanup and current documentation
TOUCHED: `render/RenderCanvas/GpuPainter.h`, `render/RenderCanvas/RenderCanvas.upp`, `render/GpuCtrl/GpuCtrl.h`, new `render/GpuRender/*`, `docs/ACTIVE_WORK.md`
STATUS: Accepted Vulkan/rendering baseline retained; public painter/façade implemented; shared context/folder consolidation/docs rewrite still active
PUBLISHED: orientation fix `c13783aaad1ce10d4ade5ac8f020c56e876ae5f8`; prior evidence checkpoint `9112e7b26d6170135ee1f0a0034d91258db91f37`; public façade implementation commit `a33ea54dc08d319798bc40135faddcea250f8aeb`
VALIDATION: static source/dependency review only for P1; Windows compile/runtime validation pending after coherent structural blocks land

## Next Action

1. Publish/verify `a33ea54d...` on `main` and use it as the next base.
2. Implement backend-neutral shared `GpuContext` ownership and route ordinary presenters through a default application context, initially reusing the existing Vulkan grouped-instance machinery; extend Vulkan sharing toward compatible logical-device/resource ownership without changing surface/swapchain independence.
3. Consolidate public U++ integration packages under `GpuRender` and remove obsolete top-level application-facing package clutter only after all in-repo includes/package dependencies are migrated coherently.
4. Rewrite README/ARCHITECTURE/usage/project plan around current truth and add three canonical quick starts: embedded `GpuCtrl`, full custom `GpuWindow`, whole-UI `GpuTopWindow`.
5. Run one consolidated Windows validation block covering the new façade, multi-surface sharing, existing renderer regressions and remaining Stage-5 acceptance.