# Backend roadmap — Vulkan, Metal and WebGPU

## Neutral contract

Applications should write the same `GpuPainter` / display-list code regardless of backend. Backend choice belongs below `GpuRender` presentation/context ownership.

The backend boundary needs to cover:

- runtime/adapter/device creation;
- queues and synchronization;
- buffers/textures/samplers;
- render pipelines and resource binding;
- surface creation and resize;
- frame acquire/present;
- uploads and destruction;
- diagnostics/capabilities.

## Vulkan

Current production backend and acceptance authority.

Active optimization/productization target:

- one compatibility-keyed device/resource context for many surfaces;
- independent swapchains per `GpuCtrl`/top-level surface;
- shared pipelines/shaders;
- shared image and glyph resources where ownership/lifetime permits;
- deterministic device-loss/teardown policy.

## Metal

### macOS

Metal maps naturally to the current separation between application context and presentation surface:

- shared `MTLDevice`/queue/resource domain behind `GpuContext`;
- independent drawable/layer presentation target per native window/control;
- same `UiRenderer2D` semantic input through a Metal `GpuRhi` implementation.

The public API must not assume Win32 message delivery, Vulkan swapchains or explicit Vulkan-style surface handles.

### iOS / iPadOS

The renderer architecture should remain viable, but full support also depends on the U++ platform/control/event layer running on those operating systems. A Metal backend alone is not sufficient. Keep drawable/native-window adaptation isolated so an eventual UIKit/CAMetalLayer host can feed the same RHI/painter stack.

## WebGPU

WebGPU is attractive because it offers a modern GPU contract in browsers and native environments.

### Renderer work

A WebGPU backend would implement the same RHI concepts using WebGPU buffers, textures, pipelines, bind groups, command encoding and canvas/surface presentation.

### U++ web application goal

Running a real U++ application in the browser requires more than WebGPU:

```text
U++ application/control code
        |
browser/WASM CtrlCore platform host
  events / input / focus / timers
  clipboard / text / IME / cursor
  window/canvas abstraction
        |
GpuRender + RenderCanvas
        |
WebGPU RenderRhi backend
        |
HTML canvas / browser compositor
```

This is feasible as an architectural direction but is not implemented by this repository today. The current work should make it easier by ensuring rendering APIs are not HWND/Vulkan-shaped.

## Backend registration

The next productization architecture slice should move concrete backend construction out of `GpuRender` implementation code into a registration/factory boundary. Then:

- `GpuRender` depends on neutral presentation/RHI contracts;
- a platform/backend package registers Vulkan, Metal or WebGPU availability;
- the default backend can be selected by platform/policy without changing application drawing code;
- tests can inject Null/mock backends without Vulkan linkage.
