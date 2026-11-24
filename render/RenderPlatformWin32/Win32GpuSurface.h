#pragma once

#include "Win32ChildSurface.h"
#include "RenderPlatformWin32Internal.h"

namespace Upp {

class Win32GpuSurface : public Win32ChildSurface {
public:
	Event<int> WhenState;
	Function<bool()> WhenPaint;

	bool IsNativeHostReady() const;
	String GetSurfaceError() const;
	bool Describe(GpuNativeWindowDesc& desc, String& error) const;
	Size GetDrawableSize() const;
	bool SetFrameRate(int fps);
	void SyncGeometry();

protected:
	void State(int reason) override;
	Image MouseEvent(int event, Point pt, int delta, dword flags) override;
	LRESULT WindowProc(UINT message, WPARAM wp, LPARAM lp) override;

private:
	Win32GpuFrameClock clock;
	bool timer_callback_active = false;
	bool syncing_clip = false;
	HWND clip_hwnd = nullptr;
	Rect last_clip = Null;
	String clipping_error;
};

}
