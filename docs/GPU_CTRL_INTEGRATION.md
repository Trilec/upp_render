# U++ GPU integration

The application-facing integration is consolidated in package `GpuRender`.

## Ownership

### `GpuCtrl`

`GpuCtrl` owns a private native child host because embedded accelerated content needs its own presentation surface. U++ layout owns the rectangle around it. Native handles, backend sessions and swapchains remain private.

### `GpuWindow` / `GpuTopWindow`

Top-level GPU windows bind presentation directly to the top-level native client window and do not create an extra child host.

### `GpuContext`

`GpuContext` is the application/backend ownership domain. Ordinary presenters use `GpuContext::Default()`.

Compatible Vulkan presenters share one expensive backend/device domain: runtime, instance, logical device, queue handles and device-level pipeline cache. Every presenter still owns an independent native surface, swapchain and frame lifecycle.

`UiRenderer2D` caches and image/glyph RHI handles remain presenter-owned until a resource identity/lifetime contract makes cross-presenter sharing safe and useful.

## Backend provider boundary

The provider registry is backend-neutral and lives in `RenderRhi`. `RenderVulkan` implements and registers the Vulkan provider; generic `GpuRender` presentation receives only the neutral provider/session/device contracts.

`GpuRender.upp` currently depends on `RenderVulkan` so adding the one public package gives Windows/Vulkan applications the default provider automatically. That package composition does not expose Vulkan types in application drawing or presentation APIs.

## Why this differs from `GLCtrl`

U++ `GLCtrl` demonstrates a useful property: many GL controls can reuse one expensive OpenGL context. We preserve that goal but not OpenGL's global mutable-state mechanism.

For Vulkan/Metal/WebGPU the model is:

- shared compatible application backend/device ownership;
- independent native surface and swapchain/drawable per presentation target;
- command/render state local to each frame/presenter;
- explicit compatibility when a second device/context is genuinely required.

On Vulkan, surface teardown waits only that surface's graphics/present queues when submitted/presented work exists. The final shared-device release retains the device-wide idle boundary required before device-owned resources are destroyed.

## Full U++ UI path

`GpuTopWindow` records resolved U++ painting through the public `Ctrl::DrawCtrl()` route. U++ itself remains recursive control/frame/layout/theme authority. `RenderCtrlBridge` translates resulting `SystemDraw` semantics into the neutral display list.

Unsupported semantics fail explicitly. A recording/presentation failure falls through to normal U++ software painting instead of consuming the paint and producing a blank window.

## Native child airspace

`GpuCtrl` is correct for explicitly embedded GPU content but must not be used to implement every ordinary UI control. Native child windows have overlap/airspace constraints. A whole GPU-rendered interface uses one `GpuTopWindow` root surface.

## Platform direction

Current concrete hosting is Win32 + Vulkan. Public controls and painter APIs deliberately avoid those types so future platform adapters/providers can provide:

- Cocoa/CAMetalLayer-style Metal surfaces on macOS;
- iOS/iPadOS Metal drawable hosting if/when the U++ platform layer supports that target;
- browser canvas/WebGPU surfaces in a WebAssembly/browser U++ host.
