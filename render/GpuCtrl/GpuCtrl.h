#pragma once

#include <CtrlLib/CtrlLib.h>
#include <RenderRhi/RenderRhi.h>

namespace Upp {

class GpuCtrl : public Ctrl {
public:
	GpuCtrl();
	~GpuCtrl() override;

	bool   IsNativeHostReady() const;
	bool   IsGpuReady() const;
	String GetGpuError() const;
	void   RequestGpuRefresh();

	GpuCtrl& SetBackend(GpuBackendKind kind);
	GpuCtrl& SetValidation(bool validation = true);
	GpuCtrl& SetShutdownCallback(Callback3<bool, bool, const String&> cb);

protected:
	void State(int reason) override;
	void Layout() override;

private:
	struct Impl;
	One<Impl> impl;
};

}
