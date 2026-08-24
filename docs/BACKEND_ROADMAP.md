# Backend roadmap — Vulkan, Metal and WebGPU

## Neutral contract

Applications should write the same `GpuPainter` / display-list code regardless of backend. Backend choice belongs below `GpuRender` presentation/context ownership.

The backend boundary covers:

- runtime/adapter/device creation;
- queues and synchronization;
- buffers/textures/samplers;
- render pipelines and resource binding;
- surface creation and resize;
- frame acquire/present;
- uploads and destruction;
- diagnostics/capabilities.

Backend registration is implemented in the neutral `RenderRhi` layer. A backend package supplies a provider context/session and exposes a neutral `GpuDevice`; generic presentation owns `UiRenderer2D`, logical surface/swapchain orchestration and out-of-date retry behavior.

## Vulkan

Current production backend and acceptance authority.

The productized Vulkan ownership model now provides:

- one compatibility-keyed runtime/instance/logical-device domain for compatible surfaces;
- queue handles owned by the shared device domain;
- one shared device-level `VkPipelineCache`;
- independent surface, swapchain and frame lifecycle per presenter;
- queue-scoped per-surface teardown, with device-wide idle retained only for final device destruction;
- explicit grouped-surface accounting and survivor/final-close tests.

`UiRenderer2D` caches and image/glyph RHI handles remain presenter-owned. Sharing those resources is a future optimization only if a clear identity, synchronization and lifetime policy justifies it.

After the current consolidated Windows/Vulkan acceptance, Vulkan follow-up should focus on output parity/readback, device-loss policy and longer-running multi-surface stress rather than another ownership redesign.

## Metal

### macOS

Metal maps naturally to the current separation between application context and presentation surface:

- shared `MTLDevice`/queue/resource domain behind a Metal provider context;
- independent drawable/layer presentation target per native window/control;
- same `UiRenderer2D` semantic input through a Metal `GpuDevice` implementation.

The public API must not assume Win32 message delivery, Vulkan swapchains or explicit Vulkan-style surface handles.

### iOS / iPadOS

The renderer architecture should remain viable, but full support also depends on the U++ platform/control/event layer running on those operating systems. A Metal backend alone is not sufficient. Keep drawable/native-window adaptation isolated so an eventual UIKit/CAMetalLayer host can feed the same RHI/painter stack.

## WebGPU

WebGPU is attractive because it offers a modern GPU contract in browsers and native environments.

### Renderer work

A WebGPU provider would implement the same neutral RHI concepts using WebGPU buffers, textures, pipelines, bind groups, command encoding and canvas/surface presentation.

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
WebGPU RenderRhi provider
        |
HTML canvas / browser compositor
```

This is feasible as an architectural direction but is not implemented by this repository today. The current provider/device contracts keep rendering APIs from becoming HWND/Vulkan-shaped.

## Provider composition

Today `RenderVulkan` registers the Vulkan provider and `GpuRender.upp` pulls that package into the ordinary Windows/Vulkan application build. This preserves the one-package developer experience while keeping provider construction out of generic presentation code.

Future Metal/WebGPU platform compositions should register through the same `RenderRhi` provider seam. Do not add placeholder providers before a real platform bring-up slice exists.
