#pragma once

#include <RenderRhi/RenderRhi.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Upp {

// Shared by RenderPlatformWin32 and GpuCtrl so Win32 descriptor validation stays
// in one narrow place.
bool BuildWin32GpuNativeWindowDesc(HWND hwnd, GpuNativeWindowDesc& out, String& error);

}
