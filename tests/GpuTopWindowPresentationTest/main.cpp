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

class UnsupportedPopupCtrl : public Ctrl {
public:
	int paints = 0;
	void Paint(Draw& w) override { ++paints; w.BeginNative(); w.EndNative(); }
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

	for(int cycle = 0; cycle < 3; ++cycle) {
		RootPresentationWindow win;
		win.SetAsyncPresentation().SetFrameClock();
		win.Open();
		for(int i = 0; i < 500 && win.GetGpuStats().presented_frames == 0; ++i)
			PumpEvents(1);
		ok &= Check(win.IsGpuReady() && win.GetGpuStats().presented_frames > 0 &&
		            win.GetGpuError().IsEmpty(),
		            "async root must present a recorded control tree");
		ok &= Check(win.IsFrameClockActive(), "async animated root must start its precise host clock");
		const auto cold = win.GetGpuStats();
		ok &= Check(!cold.adapter_name.IsEmpty() && cold.first_frame_cpu_ms >= 0 &&
		            cold.first_renderer.display_op_count > 0,
		            "first successful frame retains cold renderer preparation and adapter identity");
		for(int i = 0; i < 50; ++i) {
			win.RequestGpuRefresh();
			Ctrl::ProcessEvents();
		}
		const auto warm = win.GetGpuStats();
		ok &= Check(warm.first_frame_cpu_ms == cold.first_frame_cpu_ms &&
		            warm.first_renderer.display_op_count == cold.first_renderer.display_op_count &&
		            warm.first_renderer.texture_upload_count == cold.first_renderer.texture_upload_count,
		            "warm frames preserve the first-frame diagnostic snapshot");
		ok &= Check(win.GetGpuStats().pending_frames <= 1,
		            "async admission must retain at most one pending frame");
		win.SetRect(120, 120, 740 + cycle * 10, 430);
		PumpEvents(20);
		win.Close(); // drain/cancel before HWND destruction
		ok &= Check(!win.IsFrameClockActive(), "closing an animated root must stop its host clock");
		PumpEvents(20);
		const auto closed = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		ok &= Check(closed.runtime_live_count == 0 && closed.device_live_count == 0 &&
		            closed.surface_live_count == 0 && closed.swapchain_live_count == 0,
		            "async close/reopen cycles must leave zero native ownership");
	}


	// Required mode must consume native paint without entering software rendering,
	// including startup failure, post-record failure and asynchronous admission.
	for(int async = 0; async < 2; ++async) {
		FallbackPresentationWindow win;
		win.SetRequireGpu().SetAsyncPresentation(async != 0).SetFrameClock();
		win.Open();
		for(int i = 0; i < 500 && win.GetGpuStats().presented_frames == 0; ++i)
			PumpEvents(1);
		ok &= Check(win.IsGpuReady() && win.GetGpuStats().presented_frames > 0,
		            "required GPU mode must successfully present before fault injection");
		ok &= Check(win.IsGpuRequired() && win.GetSoftwareFallbackCount() == 0,
		            "required GPU startup must never enter root software painting");
		const int left_before = win.GetLeftPaintCount();
		const int right_before = win.GetRightPaintCount();
		win.ArmPostRecordFailure();
		win.RefreshLeftProbe();
		PumpEvents(100);
		ok &= Check(!win.IsGpuReady() &&
		            win.GetGpuError().Find("intentional post-record root frame failure") >= 0,
		            "required GPU failure must stop presentation and retain its diagnostic");
		ok &= Check(!win.IsFrameClockActive(), "required GPU failure must stop its host clock");
		ok &= Check(win.GetSoftwareFallbackCount() == 0 &&
		            win.GetLeftPaintCount() == left_before + 1 &&
		            win.GetRightPaintCount() == right_before + 1,
		            "failed recording must not be followed by a software repaint");
		const int failed = win.GetFailedBuildCount();
		win.RequestGpuRefresh();
		PumpEvents(30);
		ok &= Check(win.GetFailedBuildCount() == failed && win.GetSoftwareFallbackCount() == 0,
		            "required failure must remain stable without retry or software oscillation");
		win.ArmPostRecordFailure(false);
		win.RetryGpuInit();
		for(int i = 0; i < 500 && win.GetGpuStats().presented_frames == 0; ++i)
			PumpEvents(1);
		ok &= Check(win.IsGpuReady() && win.GetGpuError().IsEmpty() &&
		            win.GetGpuStats().presented_frames > 0 && win.GetSoftwareFallbackCount() == 0,
		            "explicit required-mode retry must restore GPU presentation");
		ok &= Check(win.IsFrameClockActive(), "required GPU retry must restart its host clock");
		win.Close();
		PumpEvents(20);
	}
	{
		RootPresentationWindow win;
		win.SetRequireGpu().SetBackend(GpuBackendKind::Metal);
		win.Open();
		PumpEvents(30);
		ok &= Check(!win.IsGpuReady() && !win.GetGpuError().IsEmpty() &&
		            win.GetSoftwareFallbackCount() == 0 && win.GetRootPaintCount() == 0,
		            "unsupported required backend must not silently paint through software");
		win.Close();
		PumpEvents(20);
	}


	{
		RootPresentationWindow win;
		win.SetRequireGpu();
		win.Open();
		for(int i = 0; i < 500 && win.GetGpuStats().presented_frames == 0; ++i)
			PumpEvents(1);
		UnsupportedPopupCtrl popup;
		popup.SetRect(200, 200, 120, 60);
		popup.PopUp(&win, true, false, false, false);
		PumpEvents(100);
		ok &= Check(win.GetGpuError().Find("native SystemDraw/GDI") >= 0 &&
		            !win.IsGpuReady() && win.GetSoftwareFallbackCount() == 0,
		            "required popup failure must propagate to its root without software fallback");
		ok &= Check(popup.paints == 1,
		            "unsupported required popup must record once and never repaint in software");
		popup.Refresh();
		PumpEvents(30);
		ok &= Check(popup.paints == 1,
		            "failed required popup must consume further paint without retrying");
		popup.Close();
		win.Close();
		PumpEvents(20);
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
