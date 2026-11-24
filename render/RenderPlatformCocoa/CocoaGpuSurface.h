#pragma once

#include <CtrlCore/CtrlCore.h>
#include <RenderRhi/RenderRhi.h>

namespace Upp {

class CocoaGpuSurface : public Ctrl {
public:
	CocoaGpuSurface();
	~CocoaGpuSurface() override;

	Event<int> WhenState;
	Function<bool()> WhenPaint;

	bool IsNativeHostReady() const;
	String GetSurfaceError() const { return error; }
	bool Describe(GpuNativeWindowDesc& desc, String& error) const;
	Size GetDrawableSize() const { return drawable; }
	bool SetFrameRate(int fps);
	void RequestNativeRefresh();
	void SyncGeometry();
	void SetOverlayRects(const Vector<Rect>& rects);

protected:
	void State(int reason) override;
	void Layout() override;
	Image MouseEvent(int event, Point pt, int delta, dword flags) override;

private:
	void OpenNative();
	void CloseNative();
	void *native = nullptr;
	TimeCallback paint, clock;
	bool pending = false, mask_dirty = true;
	Rect last_frame = Null, last_clip = Null;
	Size drawable = Size(0, 0);
	double last_scale = 0, last_unit = 0;
	Vector<Rect> overlays;
	String error;
};

}
