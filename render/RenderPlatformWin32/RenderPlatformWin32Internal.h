#pragma once

#include <RenderRhi/RenderRhi.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Upp {

// Shared by RenderPlatformWin32 and GpuCtrl so Win32 descriptor validation stays
// in one narrow place.
bool BuildWin32GpuNativeWindowDesc(HWND hwnd, GpuNativeWindowDesc& out, String& error);

// Precise host wake only: no rendering or control access on the waiting thread.
// One pending window message; Stop joins before HWND teardown; stale starts are rejected.
class Win32GpuFrameClock {
public:
	Win32GpuFrameClock();
	~Win32GpuFrameClock();
	bool Start(HWND hwnd, int pulse_hz = 120);
	void Stop();
	bool IsActive() const;
	bool Consume(WPARAM generation);
	static UINT Message();
private:
	struct Impl;
	One<Impl> impl;
};

}
