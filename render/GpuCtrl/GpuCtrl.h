#pragma once

#include <CtrlLib/CtrlLib.h>
#include <RenderRhi/RenderRhi.h>

namespace Upp {

class GpuCtrl : public Ctrl {
public:
	GpuCtrl();
	~GpuCtrl() override;

	bool   IsNativeHostReady() const;
	String GetGpuError() const;
	void   RequestGpuRefresh();

	GpuCtrl& SetBackend(GpuBackendKind kind);

protected:
	void State(int reason) override;
	void Layout() override;

private:
	class Host : public DHCtrl {
	public:
		Host();

		void   State(int reason) override;
		LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;

		void   Attach(GpuCtrl& owner);

	private:
		GpuCtrl *owner = nullptr;
	};

	friend class Host;

	void SyncHostBounds();
	void OnHostState(int reason);
	void SetError(const String& message);
	void ClearError();

	Host host;
	String error;
	GpuBackendKind backend_kind = GpuBackendKind::Unknown;
	bool host_ready = false;
	bool destroying = false;
};

}
