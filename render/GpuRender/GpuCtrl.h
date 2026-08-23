#pragma once

#include <CtrlLib/CtrlLib.h>
#include <RenderCanvas/RenderCanvas.h>
#include <RenderCanvas/GpuPainter.h>
#include <RenderRhi/RenderRhi.h>

namespace Upp {

// Embedded accelerated drawing surface for an ordinary U++ layout.
class GpuCtrl : public Ctrl {
public:
	GpuCtrl();
	~GpuCtrl() override;

	// Ordinary drawing callback. GpuPainter exposes the live surface size and
	// records neutral drawing intent; application code never owns a swapchain.
	Function<void(GpuPainter&)> WhenGpuPaint;
	GpuCtrl& SetGpuPaint(Function<void(GpuPainter&)> paint)
	{
		WhenGpuPaint = paint;
		RequestGpuRefresh();
		return *this;
	}

	// Advanced neutral frame source. When set, this deliberately bypasses the
	// GpuPainter virtual/callback path while presentation ownership stays here.
	Function<bool(Size, UiDisplayList&, Rgba8&, String&)> WhenBuildFrame;

	bool IsNativeHostReady() const;
	bool IsGpuReady() const;
	String GetGpuError() const;
	void RequestGpuRefresh();
	GpuCtrl& RetryGpuInit();
	GpuCtrl& SetBackend(GpuBackendKind kind);
	GpuCtrl& SetValidation(bool validation = true);

protected:
	// Subclass-friendly equivalent of WhenGpuPaint. Default draws nothing.
	virtual void GpuPaint(GpuPainter& painter);
	void State(int reason) override;
	void Layout() override;

private:
	struct Impl;
	One<Impl> impl;
};

}
