#include "../GpuCtrlDemoSupport.h"

using namespace Upp;

namespace {

static bool HasArg(const Vector<String>& args, const char *name)
{
	for(const String& arg : args)
		if(arg == name)
			return true;
	return false;
}

class Demo : public TopWindow {
public:
	Demo(bool validation, bool auto_close)
		: auto_close(auto_close)
	{
		Title("GpuCtrlBasicDemo").Sizeable().Zoomable();
		SetRect(0, 0, 720, 420);
		Add(pane.SizePos());
		pane.SetTitle("Embedded GPU surface");
		pane.SetValidation(validation);
		pane.UpdateStatus();
	}

	void Start()
	{
		for(int i = 0; i < 100 && !pane.gpu.IsGpuReady() && pane.gpu.GetGpuError().IsEmpty(); ++i)
			Ctrl::ProcessEvents();
		Cout() << "GpuCtrlBasicDemo start: " << pane.DescribeStatus() << EOL;
		Tick();
		if(auto_close)
			SetTimeCallback(800, callback(this, &Demo::CloseNow));
	}

	bool NativeHostReady() const { return pane.gpu.IsNativeHostReady(); }
	bool GpuReady() const { return pane.gpu.IsGpuReady(); }
	String GpuError() const { return pane.gpu.GetGpuError(); }

private:
	void Tick()
	{
		pane.UpdateStatus();
		if(IsOpen())
			SetTimeCallback(100, callback(this, &Demo::Tick));
	}

	void CloseNow()
	{
		Close();
	}

	GpuCtrlPane pane;
	bool auto_close = false;
};

}

GUI_APP_MAIN
{
	Vector<String> args;
	for(int i = 1; i < __argc; ++i)
		args.Add(__argv[i]);

	bool validation = HasArg(args, "--validation");
	bool auto_close = HasArg(args, "--auto-close");
	VulkanTestHooks::ClearVulkanRuntimeDeviceDiagnostics();
	Cout() << "GUI_APP_MAIN entered" << EOL;
	bool opened = false;
	bool ready = false;
	String error;
	{
		Demo win(validation, auto_close);
		win.Open();
		opened = win.IsOpen();
		if(!opened) {
			Cout() << "FAIL: window did not open" << EOL;
			SetExitCode(1);
			return;
		}
		for(int i = 0; i < 200 && !win.GpuReady() && win.GpuError().IsEmpty(); ++i)
			Ctrl::ProcessEvents();
		ready = win.GpuReady();
		error = win.GpuError();
		if(auto_close) {
			if(!ready || !error.IsEmpty()) {
				Cout() << "FAIL: ready=" << GpuBoolText(ready) << " error='" << error << "'" << EOL;
				SetExitCode(1);
				return;
			}
		}
		win.Start();
		win.Run();
	}
	auto diag = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
	Cout() << "GpuCtrlBasicDemo diagnostics: " << GpuDiagText(diag) << EOL;
	if(auto_close) {
		if(!opened || !ready || !error.IsEmpty() || diag.runtime_live_count != 0 || diag.instance_live_count != 0 || diag.device_live_count != 0 || diag.surface_live_count != 0) {
			SetExitCode(1);
			return;
		}
	}
	Cout() << "GUI_APP_MAIN exited" << EOL;
}
