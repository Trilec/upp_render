#pragma once

#include <CtrlLib/CtrlLib.h>
#include <RenderRhi/RenderRhi.h>

namespace Upp {

class GpuCtrl : public Ctrl {
public:
	GpuCtrl();
	~GpuCtrl() override;

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
