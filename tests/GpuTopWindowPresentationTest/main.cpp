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

class ProbeCtrl : public Ctrl {
public:
	int GetPaintCount() const
	{
		return paint_count;
	}

	void Paint(Draw& w) override
	{
		++paint_count;
		w.DrawRect(GetSize(), Color(44, 90, 156));
		w.DrawText(10, 10, "Recorded child", StdFont(), Color(238, 245, 255));
		w.DrawLine(10, 38, max(11, GetSize().cx - 10), 38, 2, Color(240, 176, 72));
	}

private:
	int paint_count = 0;
};

class RootPresentationWindow : public GpuTopWindow {
public:
	RootPresentationWindow()
	{
		Title("GpuTopWindowPresentationTest");
		SetRect(120, 120, 720, 420);
		SetValidation(true);

		label.SetRect(24, 28, 250, 28);
		button.SetRect(24, 72, 190, 34);
		probe.SetRect(250, 72, 260, 120);
		label.SetLabel("Recorded U++ Label");
		button.SetLabel("Recorded U++ Button");
		Add(label);
		Add(button);
		Add(probe);
	}

	int GetFrameBuildCount() const
	{
		return frame_build_count;
	}

	int GetRootPaintCount() const
	{
		return root_paint_count;
	}

	int GetProbePaintCount() const
	{
		return probe.GetPaintCount();
	}

	bool SawRecordedText() const
	{
		return saw_recorded_text;
	}

	bool SawRecordedGeometry() const
	{
		return saw_recorded_geometry;
	}

protected:
	void Paint(Draw& w) override
	{
		++root_paint_count;
		TopWindow::Paint(w);
	}

	bool BuildGpuFrame(Size size, UiDisplayList& list,
	                   Rgba8& background, String& error) override
	{
		++frame_build_count;
		if(!GpuTopWindow::BuildGpuFrame(size, list, background, error))
			return false;
		for(int i = 0; i < list.GetCount(); ++i) {
			const UiDisplayOpType type = list[i].type;
			if(type == UiDisplayOpType::DrawText)
				saw_recorded_text = true;
			if(type == UiDisplayOpType::FillRect || type == UiDisplayOpType::StrokeRect ||
			   type == UiDisplayOpType::FillPath || type == UiDisplayOpType::StrokePath ||
			   type == UiDisplayOpType::DrawImage)
				saw_recorded_geometry = true;
		}
		return true;
	}

private:
	Label label;
	Button button;
	ProbeCtrl probe;
	int frame_build_count = 0;
	int root_paint_count = 0;
	bool saw_recorded_text = false;
	bool saw_recorded_geometry = false;
};

class FallbackPresentationWindow : public GpuTopWindow {
public:
	FallbackPresentationWindow()
	{
		Title("GpuTopWindowFallbackTest");
		SetRect(160, 160, 480, 260);
		SetValidation(true);
	}

	int GetFailedBuildCount() const
	{
		return failed_build_count;
	}

	int GetSoftwarePaintCount() const
	{
		return software_paint_count;
	}

protected:
	void Paint(Draw& w) override
	{
		++software_paint_count;
		TopWindow::Paint(w);
		w.DrawText(20, 20, "Software fallback", StdFont(), Color(36, 72, 112));
	}

	bool BuildGpuFrame(Size, UiDisplayList&, Rgba8&, String& error) override
	{
		++failed_build_count;
		error = "intentional root frame failure";
		return false;
	}

private:
	int failed_build_count = 0;
	int software_paint_count = 0;
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
			if(win.IsGpuReady() && active.swapchain_live_count == 1 && win.GetFrameBuildCount() > 0)
				break;
			Ctrl::GuiSleep(2);
		}

		ok &= Check(win.IsGpuReady(), "root TopWindow Vulkan presenter should become ready");
		ok &= Check(win.GetGpuError().IsEmpty(), "root presenter should have no error");
		ok &= Check(win.GetFrameBuildCount() > 0, "root WM_PAINT should build and present a neutral frame");
		ok &= Check(win.GetRootPaintCount() > 0,
		            "default root frame should record TopWindow painting through DrawCtrl");
		ok &= Check(win.GetProbePaintCount() > 0,
		            "default root frame should recursively record child control painting");
		ok &= Check(win.SawRecordedText(),
		            "default root display list should contain resolved U++ text intent");
		ok &= Check(win.SawRecordedGeometry(),
		            "default root display list should contain resolved U++ geometry intent");
		ok &= Check(active.surface_live_count == 1 && active.device_live_count == 1 &&
		            active.swapchain_live_count == 1,
		            "root GPU window should own exactly one presentation surface/device/swapchain");

		const uint64_t stable_swapchain_creates = active.swapchain_create_count;
		const int stable_frame_builds = win.GetFrameBuildCount();
		const int stable_root_paints = win.GetRootPaintCount();
		const int stable_probe_paints = win.GetProbePaintCount();
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
		ok &= Check(win.GetRootPaintCount() > stable_root_paints &&
		            win.GetProbePaintCount() > stable_probe_paints,
		            "explicit root refresh should re-record root and child painting");
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

	{
		FallbackPresentationWindow win;
		win.Open();
		ok &= Check(win.IsOpen(), "fallback root window should open");
		if(win.IsOpen()) {
			for(int i = 0; i < 500 && !win.IsGpuReady(); ++i) {
				Ctrl::ProcessEvents();
				Ctrl::GuiSleep(2);
			}
			ok &= Check(win.IsGpuReady(), "fallback test should initialize the GPU presenter");
			const int failed_before = win.GetFailedBuildCount();
			const int software_before = win.GetSoftwarePaintCount();
			win.RequestGpuRefresh();
			PumpEvents(80);
			ok &= Check(win.GetFailedBuildCount() > failed_before,
			            "failed root frame should still attempt the GPU frame builder");
			ok &= Check(win.GetSoftwarePaintCount() > software_before,
			            "failed root frame should fall through to normal U++ software painting");
			ok &= Check(win.GetGpuError().Find("intentional root frame failure") >= 0,
			            "failed root frame should retain diagnostic error evidence");
			win.Close();
			PumpEvents(20);
		}
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
