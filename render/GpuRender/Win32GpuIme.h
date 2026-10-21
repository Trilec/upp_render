#pragma once
#include <CtrlCore/CtrlCore.h>
#include <RenderCore/GpuImeClient.h>
namespace Upp {
#ifdef PLATFORM_WIN32
// Called before Core's legacy preedit popup handling, only for GPU hosts.
// Unhandled messages retain the ordinary native window procedure.
bool HandleWin32GpuIme(Ctrl& root,HWND hwnd,UINT message,WPARAM wparam,LPARAM& lparam,LRESULT& result);
#endif
}
