#include "Win32GpuSurface.h"

namespace Upp {

bool Win32GpuSurface::IsNativeHostReady() const
{
	DWORD pid = 0;
	return GetHWND() && GetWindowThreadProcessId(GetHWND(), &pid) && pid == GetCurrentProcessId();
}

String Win32GpuSurface::GetSurfaceError() const
{
	return GetNativeWindowError().IsEmpty() ? clipping_error : GetNativeWindowError();
}

bool Win32GpuSurface::Describe(GpuNativeWindowDesc& desc, String& error) const
{
	return BuildWin32GpuNativeWindowDesc(GetHWND(), desc, error);
}

Size Win32GpuSurface::GetDrawableSize() const
{
	RECT rect{};
	return GetClientRect(GetHWND(), &rect) ? Size(rect.right - rect.left, rect.bottom - rect.top) : Size(0, 0);
}

bool Win32GpuSurface::SetFrameRate(int fps)
{
	clock.Stop();
	return !fps || clock.Start(GetHWND(), fps);
}

void Win32GpuSurface::State(int reason)
{
	if(reason == CLOSE) {
		WhenState(reason); // Release Vulkan before destroying the HWND.
		clip_hwnd = nullptr;
		clipping_error.Clear();
	}
	Win32ChildSurface::State(reason);
	if(reason != CLOSE) WhenState(reason);
}

Image Win32GpuSurface::MouseEvent(int event, Point pt, int delta, dword flags)
{
	Ctrl *owner = GetParent();
	if(!owner) return Win32ChildSurface::MouseEvent(event, pt, delta, flags);
	return owner->MouseEvent(event, pt + (GetScreenView().TopLeft() - owner->GetScreenView().TopLeft()), delta, flags);
}

LRESULT Win32GpuSurface::WindowProc(UINT message, WPARAM wp, LPARAM lp)
{
	if(message == WM_WINDOWPOSCHANGED) {
		Ptr<Win32GpuSurface> alive(this);
		LRESULT result = Win32ChildSurface::WindowProc(message, wp, lp);
		if(alive) alive->SyncGeometry();
		return result;
	}
	if(message == Win32GpuFrameClock::Message()) {
		if(clock.Consume(wp)) {
			Ptr<Win32GpuSurface> alive(this);
			if(!timer_callback_active) {
				timer_callback_active = true;
				TimerProc(msecs());
				if(alive) alive->timer_callback_active = false;
			}
			if(alive && IsWindowVisible(alive->GetHWND()) && !IsIconic(GetAncestor(alive->GetHWND(), GA_ROOT)))
				alive->RequestNativeRefresh();
		}
		return 0;
	}
	if(message == WM_PAINT) {
		HWND hwnd = GetHWND();
		PAINTSTRUCT paint{};
		HDC dc = BeginPaint(hwnd, &paint);
		if(!WhenPaint()) FillRect(dc, &paint.rcPaint, GetSysColorBrush(COLOR_WINDOW));
		EndPaint(hwnd, &paint);
		return 0;
	}
	return Win32ChildSurface::WindowProc(message, wp, lp);
}

void Win32GpuSurface::SyncGeometry()
{
	if(syncing_clip || !IsNativeHostReady()) return;
	const HWND hwnd = GetHWND();
	RECT native_rect;
	if(!GetWindowRect(hwnd, &native_rect)) {
		clipping_error = "GPU native child bounds could not be read.";
		return;
	}
	// Win32ChildSurface is a native sibling of its logical U++ ancestors. Windows
	// therefore needs the same complete ancestor clip that Ctrl painting uses.
	// Keep the full surface allocation and clip its visible region; scrolling
	// must never resize or reproject the camera to the remaining visible strip.
	const Rect native_bounds(native_rect.left, native_rect.top,
	                         native_rect.right, native_rect.bottom);
	Rect visible = GetVisibleScreenRect() & native_bounds;
	Rect clip = IsVisible() && !visible.IsEmpty()
	          ? visible - native_bounds.TopLeft() : Rect(0, 0, 0, 0);
	if(clip_hwnd == hwnd && clip == last_clip) return;
	syncing_clip = true;
	HRGN region = CreateRectRgn(clip.left, clip.top, clip.right, clip.bottom);
	if(region && SetWindowRgn(hwnd, region, TRUE)) {
		// SetWindowRgn transfers ownership only after success.
		clip_hwnd = hwnd; last_clip = clip; clipping_error.Clear();
	}
	else {
		if(region) DeleteObject(region);
		clipping_error = "GPU native child visible region could not be applied.";
	}
	syncing_clip = false;
}

}
