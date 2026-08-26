#include <CtrlLib/CtrlLib.h>
#include <GpuRender/GpuRender.h>
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
	int GetPaintCount() const { return paint_count; }

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

	int GetFrameBuildCount() const { return frame_build_count; }
	int GetRootPaintCount() const { return root_paint_count; }
	int GetProbePaintCount() const { return probe.GetPaintCount(); }
	bool SawRecordedText() const { return saw_recorded_text; }
	bool SawRecordedGeometry() const { return saw_recorded_geometry; }

protected:
	void Paint(Draw& w) override
	{
		++root_paint_count;
		TopWindow::Paint(w);
	}

	bool BuildGpuFrame(Size size, UiDisplayList& list, Rgba8& background, String& error) override
	{
		++frame_build_count;
		if(!GpuTopWindow::BuildGpuFrame(size, list, background, error))
			return false;
		for(int i = 0; i < list.GetCount(); ++i) {
			auto type = list[i].type;
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
		SetRect(160, 160, 560, 280);
		SetValidation(true);
		left.SetRect(24, 56, 150, 90);
		right.SetRect(340, 56, 150, 90);
		Add(left);
		Add(right);
	}

	void ArmPostRecordFailure(bool enable = true) { fail_after_record = enable; }
	void RefreshLeftProbe() { left.Refresh(); }
	void RefreshRightProbe() { right.Refresh(); }
	int GetFrameBuildCount() const { return frame_build_count; }
	int GetFailedBuildCount() const { return failed_build_count; }
	int GetRootPaintCount() const { return root_paint_count; }
	int GetLeftPaintCount() const { return left.GetPaintCount(); }
	int GetRightPaintCount() const { return right.GetPaintCount(); }

protected:
	void Paint(Draw& w) override
	{
		++root_paint_count;
		TopWindow::Paint(w);
		w.DrawText(20, 20, "GPU failure must become one complete software fallback", StdFont(), Color(36, 72, 112));
	}

	bool BuildGpuFrame(Size size, UiDisplayList& list, Rgba8& background, String& error) override
	{
		++frame_build_count;
		if(!GpuTopWindow::BuildGpuFrame(size, list, background, error))
			return false;
		if(fail_after_record) {
			++failed_build_count;
			error = "intentional post-record root frame failure";
			return false;
		}
		return true;
	}

private:
	ProbeCtrl left;
	ProbeCtrl right;
	int frame_build_count = 0;
	int failed_build_count = 0;
	int root_paint_count = 0;
	bool fail_after_record = false;
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
		ok &= Check(win.GetRootPaintCount() > 0, "default root frame should record TopWindow painting through DrawCtrl");
		ok &= Check(win.GetProbePaintCount() > 0, "default root frame should recursively record child control painting");
		ok &= Check(win.SawRecordedText(), "default root display list should contain resolved U++ text intent");
		ok &= Check(win.SawRecordedGeometry(), "default root display list should contain resolved U++ geometry intent");
		ok &= Check(active.surface_live_count == 1 && active.device_live_count == 1 && active.swapchain_live_count == 1,
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
		ok &= Check(win.GetRootPaintCount() > stable_root_paints && win.GetProbePaintCount() > stable_probe_paints,
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
			for(int i = 0; i < 500; ++i) {
				Ctrl::ProcessEvents();
				if(win.IsGpuReady() && win.GetFrameBuildCount() > 0)
					break;
				Ctrl::GuiSleep(2);
			}
			ok &= Check(win.IsGpuReady(), "fallback test should first establish a successful GPU frame");
			ok &= Check(win.GetGpuError().IsEmpty(), "fallback test should start without a GPU error");

			const int failed_before = win.GetFailedBuildCount();
			const int left_before = win.GetLeftPaintCount();
			const int right_before = win.GetRightPaintCount();
			const int root_before = win.GetRootPaintCount();
			win.ArmPostRecordFailure();
			win.RefreshLeftProbe();
			PumpEvents(100);

			ok &= Check(win.GetFailedBuildCount() > failed_before,
			            "post-record failure should occur after the full U++ tree was recorded");
			ok &= Check(!win.IsGpuReady(),
			            "failed root GPU frame should leave the HWND in stable software mode");
			ok &= Check(win.GetGpuError().Find("intentional post-record root frame failure") >= 0,
			            "software fallback should retain the root GPU failure diagnostic");
			ok &= Check(win.GetRootPaintCount() >= root_before + 2,
			            "post-record GPU failure should force one complete root software repaint");
			ok &= Check(win.GetLeftPaintCount() >= left_before + 2,
			            "dirty probe should paint once during recording and again during full software fallback");
			ok &= Check(win.GetRightPaintCount() >= right_before + 2,
			            "clean probe must also repaint during full software fallback after recording consumed refresh state");

			auto fallback_diag = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
			ok &= Check(fallback_diag.surface_live_count == 0 && fallback_diag.swapchain_live_count == 0,
			            "software fallback should release the failed root presentation surface/swapchain");

			const int stable_failed_builds = win.GetFailedBuildCount();
			const int software_right_before = win.GetRightPaintCount();
			win.RefreshRightProbe();
			PumpEvents(60);
			ok &= Check(win.GetFailedBuildCount() == stable_failed_builds,
			            "software fallback should not automatically oscillate back into GPU frame attempts");
			ok &= Check(win.GetRightPaintCount() > software_right_before,
			            "ordinary U++ software repaint should remain functional after GPU fallback");

			win.ArmPostRecordFailure(false);
			const int builds_before_retry = win.GetFrameBuildCount();
			win.RetryGpuInit();
			for(int i = 0; i < 500; ++i) {
				Ctrl::ProcessEvents();
				if(win.IsGpuReady() && win.GetFrameBuildCount() > builds_before_retry && win.GetGpuError().IsEmpty())
					break;
				Ctrl::GuiSleep(2);
			}
			ok &= Check(win.IsGpuReady() && win.GetGpuError().IsEmpty(),
			            "explicit RetryGpuInit should restore GPU presentation after a stable software fallback");
			ok &= Check(win.GetFrameBuildCount() > builds_before_retry,
			            "GPU retry should build and present a new root frame");

			win.Close();
			PumpEvents(20);
		}
	}

	auto final_diag = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
	ok &= Check(final_diag.runtime_live_count == 0 && final_diag.instance_live_count == 0 &&
	            final_diag.debug_messenger_live_count == 0 && final_diag.surface_live_count == 0 &&
	            final_diag.device_live_count == 0 && final_diag.swapchain_live_count == 0,
	            "root GPU presentation lifecycle should finish with zero Vulkan ownership");

	if(ok) {
		Cout() << "GpuTopWindowPresentationTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
