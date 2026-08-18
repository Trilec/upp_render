#pragma once

#include <CtrlLib/CtrlLib.h>
#include <RenderCanvas/RenderCanvas.h>
#include <RenderRhi/RenderRhi.h>

namespace Upp {

class GpuCtrl : public Ctrl {
public:
	GpuCtrl();
	~GpuCtrl() override;

	// Optional neutral frame source for embedded accelerated content. If unset,
	// GpuCtrl keeps its built-in reference scene. The callback owns only drawing
	// intent; native/session/swapchain ownership stays inside GpuCtrl.
	Function<bool(Size, UiDisplayList&, Rgba8&, String&)> WhenBuildFrame;

	// Advanced diagnostics for host/native lifecycle issues.
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
