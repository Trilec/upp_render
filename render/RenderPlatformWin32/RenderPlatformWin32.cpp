#include "RenderPlatformWin32.h"
#include "RenderPlatformWin32Internal.h"

namespace Upp {

bool BuildWin32GpuNativeWindowDesc(HWND hwnd, GpuNativeWindowDesc& out, String& error)
{
	out = GpuNativeWindowDesc();
	// Keep the native handle opaque; callers only get a typed descriptor, never
	// a logged pointer value.
	if(!hwnd || !IsWindow(hwnd)) {
		error = "invalid native handle";
		return false;
	}
	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if(pid != GetCurrentProcessId()) {
		error = "native window belongs to another process";
		return false;
	}
	out.kind = GpuNativeWindowKind::Win32;
	out.handle = (uintptr_t)hwnd;
	return true;
}

GpuResult GetGpuNativeWindowDesc(const TopWindow& window, GpuNativeWindowDesc& out, String& error)
{
	out = GpuNativeWindowDesc();
	if(!window.IsOpen()) {
		error = "window not open";
		return GpuResult::InvalidState;
	}
	if(!BuildWin32GpuNativeWindowDesc(window.GetHWND(), out, error))
		return GpuResult::InvalidHandle;
	return GpuResult::Ok;
}

}
