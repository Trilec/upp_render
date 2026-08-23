#pragma once

#include <CtrlLib/CtrlLib.h>
#include <RenderCanvas/RenderCanvas.h>
#include <RenderCanvas/GpuPainter.h>
#include <RenderRhi/RenderRhi.h>

namespace Upp {

class GpuCtrl : public Ctrl {
public:
	GpuCtrl();
	~GpuCtrl() override;

	// Simple application-facing paint path. Drawing is recorded through
	// GpuPainter into the neutral display list and replayed by the selected
	// backend. No backend/swapchain objects are exposed to application code.
	GpuCtrl& SetGpuPaint(Function<void(GpuPainter&)> paint)
	{
		WhenBuildFrame = [paint](Size, UiDisplayList& list, Rgba8& background, String& error) mutable {
			GpuPainter painter;
			if(paint)
				paint(painter);
			return painter.FinishFrame(list, background, error);
		};
		return *this;
	}

	// Advanced neutral frame source. Use this only when the caller needs to own
	// immutable display-list construction directly. Native/session/swapchain
	// ownership always stays inside GpuCtrl.
	Function<bool(Size, UiDisplayList&, Rgba8&, String&)> WhenBuildFrame;

	bool   IsNativeHostReady() const;
	bool   IsGpuReady() const;
	String GetGpuError() const;
	void   RequestGpuRefresh();
	GpuCtrl& RetryGpuInit();

	GpuCtrl& SetBackend(GpuBackendKind kind);
	GpuCtrl& SetValidation(bool validation = true);

protected:
	void State(int reason) override;
	void Layout() override;

private:
	struct Impl;
	One<Impl> impl;
};

}
