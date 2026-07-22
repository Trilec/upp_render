# GpuCtrl Integration

## Purpose

`GpuCtrl` is the application-facing U++ control for hosting GPU content inside a normal `TopWindow` layout.

The intended user experience is simple: add a control, size it with layout, attach a render callback later. The programmer should not need to manage HWNDs, surface descriptors, queue-family selection, swapchains, acquisition, presentation, or cleanup ceremony.

## Intended Public Shape

Planned shape:

```cpp
class GpuCtrl : public Ctrl {
public:
    Event<GpuPainter&> WhenRender;

    bool IsGpuReady() const;
    String GetGpuError() const;
    void RequestGpuRefresh();

    GpuCtrl& SetBackend(GpuBackendKind kind);
};
```

The current prototype is lifecycle-only. `GpuPainter` does not exist yet.

## Selected Native Host Mechanism

The prototype uses U++'s existing child-window mechanism through `DHCtrl`, wrapped privately inside `GpuCtrl`.

That gives a real embedded child HWND without exposing native handles in the public API.

Mechanism selected:

- child HWND creation is owned by the private `DHCtrl` host
- the host uses U++'s registered Win32 class from `Ctrl::InitWin32()`
- positioning is driven from `GpuCtrl::Layout()`
- visibility follows normal U++ show/hide propagation through the host state path
- destruction is driven by the normal U++ close and member-destruction order

## Lifecycle Model

Planned ownership order:

1. `TopWindow` owns `GpuCtrl` as an ordinary child control.
2. `GpuCtrl` owns a private child host control.
3. The host owns the native child HWND.
4. Future backend objects will hang off the host or control state, not the other way around.

Destruction order:

- backend resources must be released before the child HWND disappears
- the child HWND must be destroyed before the C++ control object is fully gone
- the current prototype has no backend resources yet, but it already follows the child-host shutdown path

## Resize and Visibility

`GpuCtrl` keeps the host rectangle synchronized from `Layout()`.

Repeated `Layout()` calls are safe.
Repeated `OPEN`/`CLOSE`/`SHOW`/`ENABLE` notifications are treated as idempotent state updates.

Visibility follows the surrounding U++ control tree where practical. The host is not a replacement window manager. It is just the small window inside the larger one that happened to be invited.

## Error Handling

The prototype exposes a concise string error state through `GetGpuError()`.

Current error cases are limited to native-host readiness problems. No Vulkan surface or swapchain error path is implemented in this task.

## Refresh Semantics

`RequestGpuRefresh()` is a thin host invalidation request.

In the current prototype it is intentionally boring: if the native host is ready, it requests a repaint; otherwise it does nothing.

## Future Swapchain Ownership

Swapchain ownership is intentionally not part of the current control boundary.

Future backend objects should live behind the host lifecycle, after the native child window is stable and before any actual frame presentation begins.

## Future `GpuPainter`

`GpuPainter` should be a backend-agnostic painter front end, not a Vulkan-shaped public API.

Expected flow:

`U++/upp_Ui control state` -> `theme/style resolution` -> `GpuPainter` or `UiRenderer2D` -> `RenderRhi` -> `Vulkan`

The existing theme system remains authoritative. The GPU layer consumes resolved presentation data; it does not invent a parallel theme database out of ambition.

## Optional `GpuTopWindow`

A whole-window convenience API may later be useful:

```cpp
class GpuTopWindow : public TopWindow
```

This should remain optional. `GpuCtrl` is the primary boundary because it composes naturally inside normal layouts.

## Relationship to `RenderRhi`

`RenderRhi` remains the neutral lower-level GPU contract.

`GpuCtrl` does not bypass it. The control only provides the application-facing hosting boundary and lifecycle entry point.

## Separation from `RenderPlatformWin32`

`RenderPlatformWin32` is the native-window descriptor bridge for future surface creation.

`GpuCtrl` is the UI control boundary.

The two should stay separate so layout code does not become a surface factory wearing a control badge.

## OpenGL Integration Findings

### Files and methods inspected

- `uppsrc/GLCtrl/GLCtrl.h`
- `uppsrc/GLCtrl/GLCtrl.cpp`
- `uppsrc/GLCtrl/Win32GLCtrl.cpp`
- `uppsrc/GLCtrl/XGLCtrl.cpp`
- `uppsrc/CtrlCore/DHCtrl.cpp`
- `uppsrc/CtrlCore/CtrlChild.cpp`
- `uppsrc/CtrlCore/TopWindow.cpp`
- `uppsrc/CtrlCore/TopWin32.cpp`
- `uppsrc/CtrlCore/Win32Wnd.cpp`
- `uppsrc/CtrlCore/Win32Ctrl.h`
- `render/RenderPlatformWin32/RenderPlatformWin32.h`
- `render/RenderPlatformWin32/RenderPlatformWin32.cpp`
- `render/RenderRhi/RenderRhi.h`
- `render/RenderRhi/RenderRhi.cpp`
- `examples/DisplayListDemo/main.cpp`

### Conventions worth preserving

- `Ctrl`-based placement inside ordinary U++ layouts
- creation tied to `OPEN` and cleanup tied to `CLOSE`
- resize synchronization from layout or state notifications
- a small public surface with the native details kept private
- callback-based rendering entry points rather than a global render loop

### OpenGL-specific details not to copy

- global singleton OpenGL context ownership
- WGL/GLX-specific context and pixel-format setup in control code
- `Draw`-layer viewport hacks for the fixed-function pipeline
- fixed-pipeline helpers such as `StdView()`
- assumptions that painting must be immediate-mode GL

### Lifecycle weaknesses to avoid

- relying on a global rendering context shared by all controls
- making the control public API expose native handles or backend-specific state
- mixing host-window ownership with rendering backend ownership
- treating visibility or focus as backend-only concerns instead of control lifecycle concerns

## Decision Summary

Retained from OpenGL:

- ordinary U++ control embedding
- creation and destruction tied to control lifecycle
- private native host ownership

Rejected from OpenGL:

- singleton context model
- fixed-function assumptions
- implicit global drawing state
- public exposure of native window details

## UI Rendering Direction

The existing U++ and upp_Ui rendering path should remain theme-driven and neutral.

Potential future responsibilities:

- filled rectangles
- rounded rectangles
- strokes and borders
- gradients
- clipping and transforms
- images and icons
- text and glyph atlases
- opacity and batching
- dirty-region rendering or retained caching where beneficial

Implementation options to evaluate later:

- direct painter operations from controls
- a neutral display list
- an adapter over existing Draw operations
- a hybrid model with recording plus backend-specific replay

The safest likely path is a neutral recording layer with software replay as the reference implementation.

## Compute Readiness

Future compute should fit beside the graphics path without leaking into `GpuCtrl`.

Needed neutral ideas later:

- compute pipeline
- storage buffer
- storage image
- resource binding
- descriptor binding abstraction
- dispatch
- barriers and synchronization
- queue capability discovery

`RenderRhi` already leaves room for this direction because its current surface does not hard-code swapchains into the application-facing control API.

The main thing to avoid is adding graphics-only assumptions to `GpuCtrl` now and then discovering compute had to sit in the waiting room with no chair.
