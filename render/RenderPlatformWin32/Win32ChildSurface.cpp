// Copyright (c) 2026 upp_render contributors. Apache-2.0; see LICENSE.
#include "Win32ChildSurface.h"

#ifdef GUI_WIN
namespace Upp {
namespace {
const wchar_t* SurfaceClassName() { return L"UppRenderOwnedChildSurfaceV1"; }
}

Win32ChildSurface::Win32ChildSurface()
{
    NoWantFocus();
    // Actual pixels belong to the HWND. Keep this logical container transparent
    // so ordinary ancestors can repaint any area uncovered by its region.
    Transparent();
}

Win32ChildSurface::~Win32ChildSurface()
{
    CloseNative();
}

LRESULT CALLBACK Win32ChildSurface::Dispatch(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    GuiLock lock;
    Win32ChildSurface* self = reinterpret_cast<Win32ChildSurface*>(
        ::GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if(message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lp);
        self = static_cast<Win32ChildSurface*>(create->lpCreateParams);
        if(!self) return FALSE;
        self->hwnd_ = hwnd;
        ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if(!self) return ::DefWindowProcW(hwnd, message, wp, lp);
    Ptr<Win32ChildSurface> alive(self);
    const LRESULT result = self->WindowProc(message, wp, lp);
    // A callback may destroy the public Ctrl or close its window. Never access
    // the original pointer after invoking a virtual message handler.
    if(message == WM_NCDESTROY) {
        ::SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        if(alive && alive->hwnd_ == hwnd) alive->hwnd_ = nullptr;
    }
    return result;
}

void Win32ChildSurface::OpenNative()
{
    CloseNative();
    Ctrl* top = GetTopCtrl();
    HWND parent = top && top != this ? top->GetHWND() : nullptr;
    if(!parent) { error_ = "Native surface requires an open U++ top window."; return; }
    const HINSTANCE instance = ::GetModuleHandleW(nullptr);
    WNDCLASSEXW definition{};
    definition.cbSize = sizeof(definition);
    definition.lpfnWndProc = Dispatch;
    definition.hInstance = instance;
    definition.lpszClassName = SurfaceClassName();
    definition.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    if(!::RegisterClassExW(&definition)) {
        if(::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            error_ = Format("Native surface class registration failed (%d).", int(::GetLastError()));
            return;
        }
        WNDCLASSEXW existing{};
        existing.cbSize = sizeof(existing);
        if(!::GetClassInfoExW(instance, SurfaceClassName(), &existing) ||
           existing.lpfnWndProc != Dispatch) {
            error_ = "Native surface class name is already owned by a different implementation.";
            return;
        }
    }
    last_rect_ = Null;
    last_visible_ = false;
    HWND created = ::CreateWindowExW(0, SurfaceClassName(), L"",
        WS_CHILD | WS_DISABLED | WS_CLIPSIBLINGS, 0, 0, 1, 1,
        parent, nullptr, instance, this);
    if(!created) {
        error_ = Format("Native surface window creation failed (%d).", int(::GetLastError()));
        return;
    }
    error_.Clear();
    SyncNative();
}

void Win32ChildSurface::CloseNative()
{
    if(hwnd_) {
        const HWND closing = hwnd_;
        ::DestroyWindow(closing);
        if(hwnd_ == closing) hwnd_ = nullptr;
    }
    last_rect_ = Null;
    last_visible_ = false;
}

void Win32ChildSurface::SyncNative()
{
    if(syncing_ || !hwnd_) return;
    Ctrl* top = GetTopCtrl();
    HWND parent = top && top != this ? top->GetHWND() : nullptr;
    if(!parent || ::GetParent(hwnd_) != parent) {
        error_ = "Native surface parent changed without a Ctrl close/open transition.";
        return;
    }
    POINT origin{0, 0};
    if(!::ClientToScreen(parent, &origin)) {
        error_ = "Native surface parent client origin is unavailable.";
        return;
    }
    const Rect rect = GetScreenView() - Point(origin.x, origin.y);
    const bool visible = IsVisible() && !rect.IsEmpty();
    syncing_ = true;
    if(rect != last_rect_) {
        if(!::SetWindowPos(hwnd_, nullptr, rect.left, rect.top,
                           max(0, rect.Width()), max(0, rect.Height()),
                           SWP_NOACTIVATE | SWP_NOZORDER)) {
            error_ = Format("Native surface geometry update failed (%d).", int(::GetLastError()));
            syncing_ = false;
            return;
        }
        last_rect_ = rect;
    }
    if(visible != last_visible_) {
        ::ShowWindow(hwnd_, visible ? SW_SHOWNA : SW_HIDE);
        last_visible_ = visible;
    }
    syncing_ = false;
    error_.Clear();
}

void Win32ChildSurface::RequestNativeRefresh()
{
    if(hwnd_ && IsVisible()) ::InvalidateRect(hwnd_, nullptr, FALSE);
}

void Win32ChildSurface::State(int reason)
{
    Ctrl::State(reason);
    if(reason == CLOSE) { CloseNative(); return; }
    if(reason == OPEN) OpenNative();
    else SyncNative();
    if(reason == SHOW || reason == ENABLE) RequestNativeRefresh();
}

void Win32ChildSurface::Layout()
{
    Ctrl::Layout();
    SyncNative();
}

LRESULT Win32ChildSurface::WindowProc(UINT message, WPARAM wp, LPARAM lp)
{
    if(message == WM_ERASEBKGND) return 1;
    return ::DefWindowProcW(hwnd_, message, wp, lp);
}

}
#endif
