#include <CtrlLib/CtrlLib.h>
#include <RenderPlatformWin32/RenderPlatformWin32.h>
#include <RenderVulkan/RenderVulkanSurfaceSession.h>

using namespace Upp;

static bool HasArg(const Vector<String>& args, const char *name)
{
	for(const String& arg : args)
		if(arg == name)
			return true;
	return false;
}

static int ParseFrames(const Vector<String>& args)
{
	for(int i = 0; i + 1 < args.GetCount(); ++i)
		if(args[i] == "--frames")
			return max(1, StrInt(args[i + 1]));
	return 8;
}

GUI_APP_MAIN
{
	Vector<String> args;
	for(int i = 1; i < __argc; ++i)
		args.Add(__argv[i]);
	bool validation = HasArg(args, "--validation");
	bool hold = HasArg(args, "--hold");
	int frames = ParseFrames(args);

	TopWindow win;
	win.Title("Vulkan Clear Frame Demo - expected blue").SetRect(120, 120, 720, 480);
	win.Open();
	if(!win.IsOpen()) {
		Cout() << "FAIL: window did not open" << EOL;
		SetExitCode(1);
		return;
	}
	Ctrl::ProcessEvents();

	GpuNativeWindowDesc native_window;
	String error;
	if(GetGpuNativeWindowDesc(win, native_window, error) != GpuResult::Ok) {
		Cout() << "FAIL: " << error << EOL;
		SetExitCode(1);
		return;
	}

	VulkanSurfaceSession session;
	if(!session.Open(validation, native_window) || !session.IsReady()) {
		Cout() << "FAIL: session open: " << session.GetError() << EOL;
		SetExitCode(1);
		return;
	}
	Size size = win.GetSize();
	if(!session.CreateSwapchain(size)) {
		Cout() << "FAIL: swapchain: " << session.GetReport().swapchain_error << EOL;
		SetExitCode(1);
		return;
	}

	constexpr float kRed = 0.08f;
	constexpr float kGreen = 0.24f;
	constexpr float kBlue = 0.58f;
	constexpr float kAlpha = 1.0f;
	bool ok = true;
	for(int i = 0; i < frames && win.IsOpen(); ++i) {
		if(!session.PresentClearFrame(kRed, kGreen, kBlue, kAlpha)) {
			Cout() << "FAIL: clear/present: " << session.GetFrameReport().error << EOL;
			ok = false;
			break;
		}
		Ctrl::ProcessEvents();
		Ctrl::GuiSleep(16);
	}

	if(ok) {
		const VulkanFrameReport& frame = session.GetFrameReport();
		const VulkanSurfaceReport& surface = session.GetReport();
		ok = frame.cleared && frame.presented && frame.clear_count == (uint64_t)frames &&
		     surface.validation_warning_count == 0 && surface.validation_error_count == 0;
		Cout() << "Expected clear RGB: 0.08 0.24 0.58" << EOL;
		Cout() << "Presented clear frames: " << frame.clear_count << EOL;
		Cout() << "Validation warnings: " << surface.validation_warning_count << EOL;
		Cout() << "Validation errors: " << surface.validation_error_count << EOL;
	}

	if(ok && hold) {
		while(win.IsOpen()) {
			bool quit = false;
			Ctrl::ProcessEvents(&quit);
			if(quit)
				break;
			Ctrl::GuiSleep(50);
		}
	}

	session.Close();
	win.Close();
	Ctrl::ProcessEvents();
	if(!ok) {
		SetExitCode(1);
		return;
	}
	Cout() << "VulkanClearFrameDemo passed" << EOL;
}
