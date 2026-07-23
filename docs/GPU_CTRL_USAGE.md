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

It currently keeps a valid GPU surface/session only. It does not present visible frames yet.

`RequestGpuRefresh()` is still a no-op style host repaint hook until the rendering callback exists.

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

`GpuCtrl` currently creates a valid embedded GPU surface/session and reports readiness and errors, but it does not render or present visible frames.
