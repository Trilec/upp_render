#include <CtrlLib/CtrlLib.h>
#include <GpuCtrl/GpuCtrl.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>

using namespace Upp;

namespace {

static bool Check(bool condition, const char *message)
{
	if(!condition)
		Cout() << "FAIL: " << message << EOL;
	return condition;
}

static void PumpEvents(int count)
{
	for(int i = 0; i < count; ++i) {
		Ctrl::ProcessEvents();
		Ctrl::GuiSleep(2);
	}
}

class PresentationWindow : public TopWindow {
public:
	PresentationWindow()
	{
		Title("GpuCtrlPresentationTest");
		SetRect(100, 100, 900, 360);
		first.SetValidation(true);
		second.SetValidation(true);
		fallback.SetBackend(GpuBackendKind::Null);
		Add(first);
		Add(second);
		Add(fallback);
	}

	void Layout() override
	{
		Size sz = GetSize();
		int w = max(1, sz.cx / 3);
		first.SetRect(0, 0, w, sz.cy);
		second.SetRect(w, 0, w, sz.cy);
		fallback.SetRect(2 * w, 0, max(0, sz.cx - 2 * w), sz.cy);
	}

	GpuCtrl first;
	GpuCtrl second;
	GpuCtrl fallback;
};

} // namespace

GUI_APP_MAIN
{
	VulkanTestHooks::ClearVulkanRuntimeDeviceDiagnostics();
	bool ok = true;
	{
		PresentationWindow win;
		win.Open();
		ok &= Check(win.IsOpen(), "test window should open");
		if(!win.IsOpen()) {
			SetExitCode(1);
			return;
		}

		VulkanTestHooks::VulkanRuntimeDeviceDiagnostics active;
		for(int i = 0; i < 500; ++i) {
			Ctrl::ProcessEvents();
			active = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
			if(win.first.IsGpuReady() && win.second.IsGpuReady() &&
			   win.fallback.IsNativeHostReady() && active.swapchain_live_count == 2)
				break;
			Ctrl::GuiSleep(2);
		}

		ok &= Check(win.first.IsGpuReady(), "first Vulkan control should be ready");
		ok &= Check(win.second.IsGpuReady(), "second Vulkan control should be ready");
		ok &= Check(win.first.GetGpuError().IsEmpty(), "first Vulkan control should have no error");
		ok &= Check(win.second.GetGpuError().IsEmpty(), "second Vulkan control should have no error");
		ok &= Check(win.fallback.IsNativeHostReady(), "fallback control should keep its native host");
		ok &= Check(!win.fallback.IsGpuReady(), "unsupported backend should use fallback rather than report GPU readiness");
		ok &= Check(win.fallback.GetGpuError() == "backend not supported", "fallback should report unsupported backend");
		ok &= Check(active.swapchain_create_count >= 2, "two Vulkan controls should create independent swapchains");
		ok &= Check(active.swapchain_live_count == 2, "two Vulkan swapchains should be live while the window is open");
		ok &= Check(active.surface_live_count == 2 && active.device_live_count == 2,
		            "two Vulkan controls should retain isolated surfaces and devices");

		uint64_t stable_swapchain_creates = active.swapchain_create_count;
		PumpEvents(100);
		auto stable = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		ok &= Check(stable.swapchain_create_count == stable_swapchain_creates,
		            "idle event processing must not recreate swapchains");
		ok &= Check(stable.swapchain_live_count == 2,
		            "idle event processing should keep both swapchains live");

		win.first.RequestGpuRefresh();
		win.second.RequestGpuRefresh();
		win.fallback.RequestGpuRefresh();
		PumpEvents(80);
		auto refreshed = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		ok &= Check(refreshed.swapchain_create_count == stable_swapchain_creates,
		            "explicit refresh should present without recreating same-size swapchains");
		ok &= Check(win.first.GetGpuError().IsEmpty() && win.second.GetGpuError().IsEmpty(),
		            "explicit refresh should remain presentation-clean");

		win.SetRect(100, 100, 1050, 420);
		PumpEvents(120);
		auto resized = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		ok &= Check(resized.swapchain_create_count >= stable_swapchain_creates + 2,
		            "resizing should recreate one swapchain per Vulkan control");
		ok &= Check(resized.swapchain_live_count == 2,
		            "resize recreation should leave exactly two live swapchains");
		ok &= Check(win.first.IsGpuReady() && win.second.IsGpuReady(),
		            "both Vulkan controls should remain ready after resize");
		ok &= Check(win.first.GetGpuError().IsEmpty() && win.second.GetGpuError().IsEmpty(),
		            "resize recreation should remain error-free");

		win.first.Hide();
		PumpEvents(40);
		ok &= Check(win.second.IsGpuReady() && win.second.GetGpuError().IsEmpty(),
		            "hiding one control must not disturb its sibling");
		win.first.Show();
		win.first.RequestGpuRefresh();
		win.second.RequestGpuRefresh();
		PumpEvents(80);
		ok &= Check(win.first.IsGpuReady() && win.second.IsGpuReady(),
		            "both controls should remain ready after hide/show");
		ok &= Check(win.first.GetGpuError().IsEmpty() && win.second.GetGpuError().IsEmpty(),
		            "hide/show presentation should remain error-free");

		win.Close();
		PumpEvents(20);
	}

	auto final_diag = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
	ok &= Check(final_diag.runtime_live_count == 0 &&
	            final_diag.instance_live_count == 0 &&
	            final_diag.debug_messenger_live_count == 0 &&
	            final_diag.surface_live_count == 0 &&
	            final_diag.device_live_count == 0 &&
	            final_diag.swapchain_live_count == 0,
	            "GpuCtrl presentation lifecycle should finish with zero live Vulkan ownership");
	if(ok) {
		Cout() << "GpuCtrlPresentationTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
