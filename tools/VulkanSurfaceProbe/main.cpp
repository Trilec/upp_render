#include <CtrlLib/CtrlLib.h>
#include <RenderPlatformWin32/RenderPlatformWin32.h>
#include <RenderVulkan/RenderVulkan.h>

using namespace Upp;

static bool HasArg(const Vector<String>& args, const char *name)
{
	for(const String& arg : args)
		if(arg == name)
			return true;
	return false;
}

static int ParseCycles(const Vector<String>& args)
{
	for(int i = 0; i + 1 < args.GetCount(); ++i)
		if(args[i] == "--cycles")
			return max(1, StrInt(args[i + 1]));
	return 1;
}

static bool RunCycle(TopWindow& win, bool validation, int cycle, const GpuNativeWindowDesc& native_window, VulkanSurfaceReport& baseline)
{
	VulkanSurfaceProbe probe;
	VulkanSurfaceReport report = probe.Run(validation, native_window);
	Cout() << "Cycle " << cycle << EOL << probe.Dump(report) << EOL;
	if(cycle == 0)
		baseline = pick(report);
	else {
		if(report.selected_device.name != baseline.selected_device.name)
			return false;
		if(report.graphics_queue_family_index != baseline.graphics_queue_family_index)
			return false;
		if(report.present_queue_family_index != baseline.present_queue_family_index)
			return false;
	}
	return report.status == VulkanProbeStatus::Ok || report.status == VulkanProbeStatus::ValidationErrorsReported;
}

GUI_APP_MAIN
{
	Vector<String> args;
	for(int i = 1; i < __argc; ++i)
		args.Add(__argv[i]);

	bool validation = HasArg(args, "--validation");
	bool hold = HasArg(args, "--hold");
	int cycles = ParseCycles(args);

	TopWindow win;
	win.Title("VulkanSurfaceProbe").SetRect(100, 100, 720, 540);
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

	VulkanSurfaceReport baseline;
	bool ok = true;
	for(int i = 0; i < cycles; ++i)
		ok &= RunCycle(win, validation, i, native_window, baseline);

	if(hold) {
		while(win.IsOpen()) {
			bool quit = false;
			Ctrl::ProcessEvents(&quit);
			if(quit)
				break;
			Ctrl::GuiSleep(50);
		}
	}

	win.Close();
	Ctrl::ProcessEvents();
	if(!ok) {
		SetExitCode(1);
		return;
	}
	Cout() << "VulkanSurfaceProbe passed" << EOL;
}
