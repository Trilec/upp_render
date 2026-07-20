#include "RenderPlatformWin32.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Upp {

GpuResult GetGpuNativeWindowDesc(const TopWindow& window, GpuNativeWindowDesc& out, String& error)
{
	out = GpuNativeWindowDesc();
	if(!window.IsOpen()) {
		error = "window not open";
		return GpuResult::InvalidState;
	}
	HWND hwnd = window.GetHWND();
	if(!hwnd || !IsWindow(hwnd)) {
		error = "invalid native handle";
		return GpuResult::InvalidHandle;
	}
	out.kind = GpuNativeWindowKind::Win32;
	out.handle = (uintptr_t)hwnd;
	return GpuResult::Ok;
}

}
