// Copyright (c) 2026 upp_render contributors. Apache-2.0; see LICENSE.
#pragma once

#include <CtrlCore/CtrlCore.h>

#ifdef GUI_WIN
namespace Upp {

// GUI-thread-only native surface owned by an ordinary Ctrl. The full logical
// rectangle is preserved; consumers apply their ancestor-visible HRGN without
// resizing the render target or changing its projection.
//
// This deliberately does not derive from DHCtrl. In U++18468, its global GDI
// exclusion ignores the logical ancestor clip, leaving stale pixels above a
// scrolled surface. Windows' ordinary child-region clipping supplies the correct
// exclusion here. No installed framework code or private class registration is
// used. Input stays with U++: the child HWND is always WS_DISABLED.
//
// Native pixel producers explicitly call RequestNativeRefresh(). This class
// manages only HWND lifetime/geometry, not a second GUI paint/event system.
class Win32ChildSurface : public Ctrl {
public:
    Win32ChildSurface();
    ~Win32ChildSurface() override;

    HWND GetHWND() const { return hwnd_; }
    const String& GetNativeWindowError() const { return error_; }
    void RequestNativeRefresh();

protected:
    void State(int reason) override;
    void Layout() override;
    LRESULT WindowProc(UINT message, WPARAM wp, LPARAM lp) override;

private:
    HWND hwnd_ = nullptr;
    Rect last_rect_ = Null;
    bool last_visible_ = false;
    bool syncing_ = false;
    String error_;

    void OpenNative();
    void CloseNative();
    void SyncNative();
    static LRESULT CALLBACK Dispatch(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);
};

}
#endif
