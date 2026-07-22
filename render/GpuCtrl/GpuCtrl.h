#pragma once

#include <CtrlLib/CtrlLib.h>
#include <RenderRhi/RenderRhi.h>
#include <RenderVulkan/RenderVulkanSurfaceSession.h>

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
	bool BuildNativeWindowDesc(GpuNativeWindowDesc& desc, String& error) const;
	void OnHostState(int reason);
	void SetError(const String& message);
	void ClearError();
	void StartGpuSession();
	void StopGpuSession();

	Host host;
	VulkanSurfaceSession session;
	String error;
	GpuBackendKind backend_kind = GpuBackendKind::Unknown;
	bool validation_requested = false;
	bool host_ready = false;
	bool gpu_ready = false;
	bool destroying = false;
};

}
