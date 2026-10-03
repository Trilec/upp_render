# GpuSurfaceDemo

An ordinary U++ / Ui application containing one or two bounded Vulkan `GpuCtrl` surfaces. It follows the UiLabel / UiProgressRing demo shell: title/actions, live preview, production PropertyEditor and a Code page.

- Width and Height size each surface independently (logical pixels, scaled once for DPI).
- Mixed, Balls, Rectangles, Oblongs and Rounded squares exercise the public 2D painter.
- Shape count (1–200), Speed, Grid, Text and Background change the scene.
- Antialias shapes is on by default. It uses bounded cached Painter coverage images drawn by Vulkan; off uses direct single-sample GPU geometry. This is a shape-demo comparison, not Vulkan MSAA or a renderer-wide AA switch.
- Randomize creates new positions, velocities, colours and sizes.
- Animate A / Animate B stop their respective animation timers.
- Show surface B destroys/recreates the second control; A retains its scene and presenter.
- Light/Dark affects the surrounding Ui shell; the authored surface background is independent.
- Code shows a complete minimal public-API application with the current dimensions/background. It is a usage example, not a serialization of the bouncing scene.

## Build

Use the installed UMK and Vulkan build method. This checkout requires the `render` nest as well as examples and the Ui/Animation/U++ nests:

```powershell
& 'E:/upp-18468/umk.exe' 'E:/apps/github/upp_render/render,E:/apps/github/upp_render/tools,E:/apps/github/upp_render/examples,E:/apps/github/upp_render,E:/apps/github/upp_Ui,E:/apps/github/upp_animation,E:/upp-18468/uppsrc' GpuSurfaceDemo 'E:/apps/github/upp_render/CLANGx64_Vulkan.bm' --out-dir 'E:/apps/github/upp_render/build/cache-surface' -ab +GUI 'E:/apps/github/upp_render/build/GpuSurfaceDemo.exe'
```

Use `-abr` for a clean Release build. Paths are this workstation's configuration, not portable requirements.

Run with `--self-test` for a bounded native smoke. Expected summary: `GpuSurfaceDemo smoke: 16 checks / 0 failures`. It checks fractional edge coverage, switching AA off/on, both presenters, drawing, independent pause, count/size projection, randomize, sibling destruction/recreation, theme/page switching and close. Compile and runtime evidence do not replace visual review.

Use `--write-usage <file.cpp>` to save the exact generated Code output for a standalone compile check.

## Surface independence and threading

Each GpuCtrl owns a native child window, surface, swapchain and presenter renderer state. Compatible presenters share the application device domain and pipeline cache. Scene data and animation timers in this demo are per instance.

This does **not** provide one render thread per surface or asynchronous UI isolation. GpuCtrl records and presents in its native GUI paint handler; Vulkan frame acquisition/fence waits can block that path. A slow callback or GPU wait can therefore delay the GUI and other surfaces. Keep scene preparation bounded. Future off-thread preparation/render scheduling requires an explicit contract for GUI-owned controls, immutable frame handoff, synchronized shared queues and shutdown; do not call arbitrary Ui controls from worker threads.

For a whole-window GPU UI, use `GpuTopWindow` / `GpuUiGallery`. This demo deliberately keeps normal Ui controls around the embedded Vulkan surfaces. No custom-shader/compute hook or new backend API is introduced.

## Capturing the demo

The agent's Execute previews have an explicit time limit and the process tree is stopped when that limit expires. The earlier preview stopped at 180 seconds with timed_out / exit 124; it was not a recorded crash. Launch build/GpuSurfaceDemo.exe normally on your desktop for an untimed screen recording; --self-test intentionally closes after its checks.
