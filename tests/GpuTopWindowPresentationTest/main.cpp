#include <CtrlLib/CtrlLib.h>
#include <GpuTopWindow/GpuTopWindow.h>
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

class RootPresentationWindow : public GpuTopWindow {
public:
	RootPresentationWindow()
	{
		Title("GpuTopWindowPresentationTest");
		SetRect(120, 120, 720, 420);
		SetValidation(true);
	}

	int GetFrameBuildCount() const
	{
		return frame_build_count;
	}

protected:
	bool BuildGpuFrame(Size size, UiDisplayList& list,
	                   Rgba8& background, String& error) override
	{
		++frame_build_count;
		background = Rgba8(18, 30, 52, 255);
		error.Clear();
		UiDisplayListBuilder builder;
		if(size.cx > 0 && size.cy > 0) {
			const double w = size.cx;
			const double h = size.cy;
			const double unit = max(1.0, min(w, h));
			builder.FillRect(Rectf(0.08 * w, 0.10 * h, 0.44 * w, 0.48 * h),
			                 Rgba8(224, 88, 36, 255));
			builder.StrokeRect(Rectf(0.12 * w, 0.16 * h, 0.70 * w, 0.76 * h),
			                   max(1.0, unit * 0.02), Rgba8(70, 220, 148, 220));
			builder.Save();
			builder.ClipRect(Rectf(0.34 * w, 0.20 * h, 0.92 * w, 0.90 * h));
			Transform2D transform;
			transform.x.x = 0.94;
			transform.x.y = 0.10;
			transform.y.x = -0.08;
			transform.y.y = 0.96;
			transform.t = Pointf(0.04 * w, 0.02 * h);
			builder.ConcatTransform(transform);
			struct RoundedRect rounded(Rectf(0.42 * w, 0.30 * h, 0.86 * w, 0.78 * h),
			                           max(2.0, unit * 0.06));
			builder.FillRoundedRect(rounded, Rgba8(72, 128, 238, 190));
			builder.Restore();
		}
		if(!builder.Finish(list)) {
			error = builder.GetError();
			return false;
		}
		return true;
	}

private:
	int frame_build_count = 0;
};

} // namespace

GUI_APP_MAIN
{
	VulkanTestHooks::ClearVulkanRuntimeDeviceDiagnostics();
	bool ok = true;
	{
		RootPresentationWindow win;
		win.Open();
		ok &= Check(win.IsOpen(), "root GPU window should open");
		if(!win.IsOpen()) {
			SetExitCode(1);
			return;
		}

		VulkanTestHooks::VulkanRuntimeDeviceDiagnostics active;
		for(int i = 0; i < 500; ++i) {
			Ctrl::ProcessEvents();
			active = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
			if(win.IsGpuReady() && active.swapchain_live_count == 1)
				break;
			Ctrl::GuiSleep(2);
		}

		ok &= Check(win.IsGpuReady(), "root TopWindow Vulkan presenter should become ready");
		ok &= Check(win.GetGpuError().IsEmpty(), "root presenter should have no error");
		ok &= Check(win.GetFrameBuildCount() > 0, "root WM_PAINT should build and present a neutral frame");
		ok &= Check(active.surface_live_count == 1 && active.device_live_count == 1 &&
		            active.swapchain_live_count == 1,
		            "root GPU window should own exactly one presentation surface/device/swapchain");

		const uint64_t stable_swapchain_creates = active.swapchain_create_count;
		const int stable_frame_builds = win.GetFrameBuildCount();
		PumpEvents(100);
		auto stable = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		ok &= Check(stable.swapchain_create_count == stable_swapchain_creates,
		            "idle event processing must not recreate the root swapchain");
		ok &= Check(stable.swapchain_live_count == 1,
		            "idle event processing should keep one root swapchain live");

		win.RequestGpuRefresh();
		PumpEvents(80);
		auto refreshed = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		ok &= Check(win.GetFrameBuildCount() > stable_frame_builds,
		            "explicit root refresh should build another frame");
		ok &= Check(refreshed.swapchain_create_count == stable_swapchain_creates,
		            "same-size root refresh must not recreate the swapchain");
		ok &= Check(win.GetGpuError().IsEmpty(), "explicit root refresh should remain error-free");

		win.SetRect(120, 120, 860, 500);
		PumpEvents(120);
		auto resized = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		ok &= Check(resized.swapchain_create_count >= stable_swapchain_creates + 1,
		            "root resize should recreate the presentation swapchain");
		ok &= Check(resized.swapchain_live_count == 1,
		            "root resize should still leave exactly one live swapchain");
		ok &= Check(win.IsGpuReady() && win.GetGpuError().IsEmpty(),
		            "root presenter should remain ready after resize");

		win.Hide();
		PumpEvents(40);
		ok &= Check(win.IsGpuReady(), "hiding root GPU window should not tear down the session");
		win.Show();
		win.RequestGpuRefresh();
		PumpEvents(80);
		ok &= Check(win.IsGpuReady() && win.GetGpuError().IsEmpty(),
		            "root presenter should remain ready after hide/show");

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
	            "root GPU presentation lifecycle should finish with zero Vulkan ownership");

	if(ok) {
		Cout() << "GpuTopWindowPresentationTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
