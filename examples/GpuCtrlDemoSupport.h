#pragma once

#include <GpuCtrl/GpuCtrl.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>

namespace Upp {

inline String GpuBoolText(bool value)
{
	return value ? "yes" : "no";
}

inline String GpuDiagText(const VulkanTestHooks::VulkanRuntimeDeviceDiagnostics& diag)
{
	String text;
	text << "runtime=" << diag.runtime_create_count << "/" << diag.runtime_live_count
	     << " instance=" << diag.instance_create_count << "/" << diag.instance_live_count
	     << " device=" << diag.device_create_count << "/" << diag.device_live_count
	     << " surface=" << diag.surface_create_count << "/" << diag.surface_live_count
	     << " discovery=" << diag.physical_device_discovery_count
	     << " runtime-id=" << diag.runtime_id
	     << " device-id=" << diag.device_id
	     << " surface-id=" << diag.surface_id;
	return text;
}

struct GpuCtrlPane : ParentCtrl {
	GpuCtrlPane()
	{
		heading.SetLabel("GpuCtrl");
		status.SetText("");
		retry.SetLabel("Retry GPU init");
		retry.WhenAction = callback(this, &GpuCtrlPane::OnRetry);

		Add(heading.LeftPos(8, 220).TopPos(8, 20));
		Add(status.HSizePos(8, 96).TopPos(30, 34));
		Add(retry.RightPos(8, 92).TopPos(28, 24));
		Add(gpu.HSizePos(8, 8).VSizePos(72, 8));
	}

	void SetTitle(const char *text)
	{
		heading.SetLabel(text);
	}

	void SetValidation(bool validation)
	{
		validation_requested = validation;
		gpu.SetValidation(validation);
		UpdateStatus();
	}

	void SetBackend(GpuBackendKind kind)
	{
		gpu.SetBackend(kind);
		UpdateStatus();
	}

	void RetryGpuInit()
	{
		gpu.RetryGpuInit();
		UpdateStatus();
	}

	void UpdateStatus()
	{
		status.SetText(DescribeStatus());
	}

	String DescribeStatus() const
	{
		String text;
		text << "host=" << GpuBoolText(gpu.IsNativeHostReady())
		     << " gpu=" << GpuBoolText(gpu.IsGpuReady())
		     << " validation=" << GpuBoolText(validation_requested)
		     << " error='" << gpu.GetGpuError() << "'";
		return text;
	}

	GpuCtrl gpu;
	Label heading;
	Label status;
	Button retry;
	bool validation_requested = false;

private:
	void OnRetry()
	{
		RetryGpuInit();
	}
};

}
