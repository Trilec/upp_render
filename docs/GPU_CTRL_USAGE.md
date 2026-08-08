# GpuCtrl Usage

## Minimal Use

```cpp
#include <CtrlLib/CtrlLib.h>
#include <GpuCtrl/GpuCtrl.h>

using namespace Upp;

GUI_APP_MAIN
{
	TopWindow win;
	GpuCtrl gpu;
	win.Add(gpu.SizePos());
	win.Title("GpuCtrl sample").SetRect(100, 100, 800, 500);
	win.Open();
	if(!win.IsOpen())
		return;
	win.Run();
}
```

That is the ordinary starting point. No HWND code. No Vulkan code. No swapchain ceremony.

## Explicit Validation

Validation is optional and must be set before the window opens.

```cpp
GpuCtrl gpu;
gpu.SetValidation(true);
```

Use the `--validation` flag in developer builds when you want loader and validation diagnostics.

## Readiness And Errors

`IsNativeHostReady()` is an advanced diagnostic for host-window lifecycle checks.

`IsGpuReady()` means the embedded surface-level GPU session is open and ready.

`GetGpuError()` returns the most recent API rejection or session failure.

Suggested usage:

```cpp
if(gpu.IsGpuReady())
	Cout() << "GPU session ready" << EOL;
else if(!gpu.GetGpuError().IsEmpty())
	Cout() << "GPU error: " << gpu.GetGpuError() << EOL;
```

## Backend Selection

The current implementation defaults to Vulkan so ordinary code can just embed the control.

Explicit backend selection remains available for testing and future backends:

```cpp
gpu.SetBackend(GpuBackendKind::Vulkan);
```

Other backend values are currently treated as unsupported.

## Retry

Initialization makes one automatic attempt when the native host becomes ready.

After a failure, retry is deterministic and explicit:

```cpp
gpu.RetryGpuInit();
```

This keeps repeated layout or visibility notifications from redoing expensive startup work by accident.

## Resize And Lifecycle

`GpuCtrl` handles host sizing and child-window lifecycle automatically.

The Vulkan backend now owns a private swapchain for the child window and presents
the accepted S14 clear frame on ordinary paint invalidation. A real size change
reconciles the swapchain on the next paint; zero-size states do not spin or poll.

`RequestGpuRefresh()` requests one host repaint. It does not start a render loop.
If presentation is unavailable, the child uses normal GDI fallback painting and
keeps the presentation error available through `GetGpuError()`. A later resize,
show or explicit refresh may recover without recreating the whole control.

## Planned Render Callback

Future shape, not currently compilable:

```cpp
gpu.WhenRender = [&](GpuPainter& painter) {
	painter.Clear(Black());
};
```

## Multiple Controls

Independent `GpuCtrl` instances are supported. Each owns its own native host and session state.

Later work may share GPU device resources, but that is a backend optimization, not a requirement for using the control.

## Current Limitation

`GpuCtrl` currently presents only the fixed S14 clear colour. There is still no
public painter callback, general 2D renderer, text/image pipeline, or shared GPU
device context. Those remain later renderer milestones; the hosting and
presentation lifecycle is now live.
