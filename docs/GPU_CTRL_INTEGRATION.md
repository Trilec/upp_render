# U++ GPU integration

The application-facing integration is consolidated in package `GpuRender`.

## Ownership

### `GpuCtrl`

`GpuCtrl` owns a private native child host because embedded accelerated content needs its own presentation surface. U++ layout owns the rectangle around it. Native handles, backend sessions and swapchains remain private.

### `GpuWindow` / `GpuTopWindow`

Top-level GPU windows bind presentation directly to the top-level native client window and do not create an extra child host.

### `GpuContext`

`GpuContext` is the application/backend ownership domain. Ordinary presenters use `GpuContext::Default()`. The long-term contract is one compatible device/resource domain feeding many surface/swapchain presentation targets.

## Why this differs from `GLCtrl`

U++ `GLCtrl` demonstrates a useful property: many GL controls can reuse one expensive OpenGL context. We preserve that goal but not OpenGL's global mutable-state mechanism.

For Vulkan/Metal/WebGPU the intended model is:

- shared application backend/device/resource ownership;
- independent native surface and swapchain/drawable per presentation target;
- command/render state local to each frame/presenter;
- explicit compatibility when a second device/context is genuinely required.

## Full U++ UI path

`GpuTopWindow` records resolved U++ painting through the public `Ctrl::DrawCtrl()` route. U++ itself remains recursive control/frame/layout/theme authority. `RenderCtrlBridge` translates resulting `SystemDraw` semantics into the neutral display list.

Unsupported semantics fail explicitly. A recording/presentation failure falls through to normal U++ software painting instead of consuming the paint and producing a blank window.

## Native child airspace

`GpuCtrl` is correct for explicitly embedded GPU content but must not be used to implement every ordinary UI control. Native child windows have overlap/airspace constraints. A whole GPU-rendered interface uses one `GpuTopWindow` root surface.

## Platform direction

Current concrete hosting is Win32 + Vulkan. Public controls and painter APIs deliberately avoid those types so future platform adapters can provide:

- Cocoa/CAMetalLayer-style Metal surfaces on macOS;
- iOS/iPadOS Metal drawable hosting if/when the U++ platform layer supports that target;
- browser canvas/WebGPU surfaces in a WebAssembly/browser U++ host.
