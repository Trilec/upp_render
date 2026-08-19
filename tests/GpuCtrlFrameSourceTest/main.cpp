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

class FrameSourceWindow : public TopWindow {
public:
	enum class Mode {
		Valid,
		Fail,
		Invalid,
	};

	FrameSourceWindow()
	{
		Title("GpuCtrlFrameSourceTest");
		SetRect(100, 100, 640, 360);
		gpu.SetValidation(true);
		gpu.WhenBuildFrame = [this](Size size, UiDisplayList& list,
		                            Rgba8& background, String& error) {
			return BuildFrame(size, list, background, error);
		};
		Add(gpu.SizePos());
	}

	void SetMode(Mode value)
	{
		mode = value;
	}

	int GetCallbackCount() const
	{
		return callback_count;
	}

	Size GetLastCallbackSize() const
	{
		return last_callback_size;
	}

	GpuCtrl gpu;

private:
	bool BuildFrame(Size size, UiDisplayList& list, Rgba8& background, String& error)
	{
		++callback_count;
		last_callback_size = size;
		background = Rgba8(14, 28, 52, 255);
		error.Clear();

		if(mode == Mode::Fail) {
			error = "intentional frame source failure";
			return false;
		}

		UiDisplayListBuilder builder;
		if(mode == Mode::Invalid) {
			builder.Restore();
			builder.Finish(list);
			return true;
		}

		if(size.cx > 0 && size.cy > 0) {
			const double w = size.cx;
			const double h = size.cy;
			builder.FillRect(Rectf(0.08 * w, 0.10 * h, 0.56 * w, 0.62 * h),
			                 Rgba8(72, 132, 238, 255));
			builder.StrokeRect(Rectf(0.18 * w, 0.22 * h, 0.88 * w, 0.84 * h),
			                   3.0, Rgba8(238, 174, 72, 220));
		}
		if(!builder.Finish(list)) {
			error = builder.GetError();
			return false;
		}
		return true;
	}

	Mode mode = Mode::Valid;
	int callback_count = 0;
	Size last_callback_size = Size(0, 0);
};

} // namespace

GUI_APP_MAIN
{
	VulkanTestHooks::ClearVulkanRuntimeDeviceDiagnostics();
	bool ok = true;
	{
		FrameSourceWindow win;
		win.Open();
		ok &= Check(win.IsOpen(), "callback test window should open");
		if(!win.IsOpen()) {
			SetExitCode(1);
			return;
		}

		VulkanTestHooks::VulkanRuntimeDeviceDiagnostics active;
		for(int i = 0; i < 500; ++i) {
			Ctrl::ProcessEvents();
			active = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
			if(win.gpu.IsGpuReady() && active.swapchain_live_count == 1 &&
			   win.GetCallbackCount() > 0)
				break;
			Ctrl::GuiSleep(2);
		}

		ok &= Check(win.gpu.IsNativeHostReady(), "GpuCtrl native host should be ready");
		ok &= Check(win.gpu.IsGpuReady(), "GpuCtrl Vulkan presenter should become ready");
		ok &= Check(win.gpu.GetGpuError().IsEmpty(), "valid callback frame should present without error");
		ok &= Check(win.GetCallbackCount() > 0, "WhenBuildFrame should be invoked for the live control");
		ok &= Check(win.GetLastCallbackSize() == win.gpu.GetSize() &&
		            win.GetLastCallbackSize().cx > 0 && win.GetLastCallbackSize().cy > 0,
		            "WhenBuildFrame should receive the live native-host size");
		ok &= Check(active.surface_live_count == 1 && active.device_live_count == 1 &&
		            active.swapchain_live_count == 1,
		            "callback-backed GpuCtrl should own exactly one surface/device/swapchain");

		const uint64 stable_swapchain_creates = active.swapchain_create_count;
		const int refresh_before = win.GetCallbackCount();
		win.gpu.RequestGpuRefresh();
		PumpEvents(80);
		auto refreshed = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		ok &= Check(win.GetCallbackCount() > refresh_before,
		            "explicit refresh should invoke WhenBuildFrame again");
		ok &= Check(refreshed.swapchain_create_count == stable_swapchain_creates,
		            "same-size callback refresh must not recreate the swapchain");
		ok &= Check(win.gpu.GetGpuError().IsEmpty(),
		            "same-size callback refresh should remain presentation-clean");

		win.SetRect(100, 100, 820, 460);
		PumpEvents(120);
		auto resized = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		ok &= Check(win.GetLastCallbackSize() == win.gpu.GetSize() &&
		            win.GetLastCallbackSize().cx > 0 && win.GetLastCallbackSize().cy > 0,
		            "resize should propagate the new live control size to WhenBuildFrame");
		ok &= Check(resized.swapchain_create_count >= stable_swapchain_creates + 1,
		            "callback-backed control resize should recreate its swapchain");
		ok &= Check(resized.swapchain_live_count == 1,
		            "callback-backed control resize should retain exactly one live swapchain");
		ok &= Check(win.gpu.IsGpuReady() && win.gpu.GetGpuError().IsEmpty(),
		            "callback-backed control should remain ready after resize");

		win.SetMode(FrameSourceWindow::Mode::Fail);
		const int fail_before = win.GetCallbackCount();
		win.gpu.RequestGpuRefresh();
		PumpEvents(80);
		auto failed = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		ok &= Check(win.GetCallbackCount() > fail_before,
		            "failing callback should still be invoked on refresh");
		ok &= Check(win.gpu.GetGpuError().Find("intentional frame source failure") >= 0,
		            "callback failure should preserve caller diagnostic text");
		ok &= Check(win.gpu.IsGpuReady() && failed.swapchain_live_count == 1,
		            "callback failure must not tear down the GPU session or swapchain");

		win.SetMode(FrameSourceWindow::Mode::Valid);
		win.gpu.RequestGpuRefresh();
		PumpEvents(80);
		ok &= Check(win.gpu.IsGpuReady() && win.gpu.GetGpuError().IsEmpty(),
		            "a later valid callback frame should recover from callback failure");

		win.SetMode(FrameSourceWindow::Mode::Invalid);
		const int invalid_before = win.GetCallbackCount();
		win.gpu.RequestGpuRefresh();
		PumpEvents(80);
		auto invalid = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		ok &= Check(win.GetCallbackCount() > invalid_before,
		            "invalid-list callback should be invoked on refresh");
		ok &= Check(win.gpu.GetGpuError().Find("restore without matching save") >= 0,
		            "GpuCtrl should reject an invalid callback display list before presentation");
		ok &= Check(win.gpu.IsGpuReady() && invalid.swapchain_live_count == 1,
		            "invalid callback output must not tear down the GPU session or swapchain");

		win.SetMode(FrameSourceWindow::Mode::Valid);
		win.gpu.RequestGpuRefresh();
		PumpEvents(80);
		ok &= Check(win.gpu.IsGpuReady() && win.gpu.GetGpuError().IsEmpty(),
		            "valid callback output should recover after invalid-list rejection");

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
	            "callback-backed GpuCtrl lifecycle should finish with zero Vulkan ownership");

	if(ok) {
		Cout() << "GpuCtrlFrameSourceTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
