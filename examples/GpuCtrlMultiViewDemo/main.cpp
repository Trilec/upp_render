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
		Title("GpuCtrlMultiViewDemo").Sizeable().Zoomable();
		SetRect(0, 0, 1024, 640);

		hide_left.SetLabel("Hide left");
		show_left.SetLabel("Show left");
		hide_right.SetLabel("Hide right");
		show_right.SetLabel("Show right");

		hide_left.WhenAction = callback(this, &Demo::OnHideLeft);
		show_left.WhenAction = callback(this, &Demo::OnShowLeft);
		hide_right.WhenAction = callback(this, &Demo::OnHideRight);
		show_right.WhenAction = callback(this, &Demo::OnShowRight);

		Add(hide_left.LeftPos(8, 80).TopPos(8, 24));
		Add(show_left.LeftPos(96, 80).TopPos(8, 24));
		Add(hide_right.LeftPos(184, 88).TopPos(8, 24));
		Add(show_right.LeftPos(280, 88).TopPos(8, 24));

		split.Horz(left, right);
		Add(split.HSizePos(8, 8).VSizePos(44, 8));

		left.SetTitle("Left GPU control");
		right.SetTitle("Right GPU control");
		left.SetValidation(validation);
		right.SetValidation(validation);
		left.UpdateStatus();
		right.UpdateStatus();
	}

	void Start()
	{
		for(int i = 0; i < 100 && (!LeftGpuReady() || !RightGpuReady()) && LeftGpuError().IsEmpty() && RightGpuError().IsEmpty(); ++i)
			Ctrl::ProcessEvents();
		Cout() << "GpuCtrlMultiViewDemo start left: " << left.DescribeStatus() << EOL;
		Cout() << "GpuCtrlMultiViewDemo start right: " << right.DescribeStatus() << EOL;
		if(auto_close)
			ScriptStep();
		else
			Tick();
	}

	bool LeftGpuReady() const { return left.gpu.IsGpuReady(); }
	bool RightGpuReady() const { return right.gpu.IsGpuReady(); }
	String LeftGpuError() const { return left.gpu.GetGpuError(); }
	String RightGpuError() const { return right.gpu.GetGpuError(); }

private:
	void OnHideLeft()
	{
		left.Hide();
	}

	void OnShowLeft()
	{
		left.Show();
	}

	void OnHideRight()
	{
		right.Hide();
	}

	void OnShowRight()
	{
		right.Show();
	}


	void ScriptStep()
	{
		left.UpdateStatus();
		right.UpdateStatus();
		switch(script_step++) {
		case 0:
			SetRect(0, 0, 1140, 700);
			Cout() << "GpuCtrlMultiViewDemo resize wide left: " << left.DescribeStatus() << EOL;
			Cout() << "GpuCtrlMultiViewDemo resize wide right: " << right.DescribeStatus() << EOL;
			break;
		case 1:
			OnHideLeft();
			Cout() << "GpuCtrlMultiViewDemo hide left" << EOL;
			break;
		case 2:
			OnShowLeft();
			Cout() << "GpuCtrlMultiViewDemo show left" << EOL;
			break;
		case 3:
			SetRect(0, 0, 980, 620);
			Cout() << "GpuCtrlMultiViewDemo resize narrow left: " << left.DescribeStatus() << EOL;
			Cout() << "GpuCtrlMultiViewDemo resize narrow right: " << right.DescribeStatus() << EOL;
			break;
		default:
			Close();
			return;
		}
		SetTimeCallback(250, callback(this, &Demo::ScriptStep));
	}

	void Tick()
	{
		left.UpdateStatus();
		right.UpdateStatus();
		if(IsOpen())
			SetTimeCallback(100, callback(this, &Demo::Tick));
	}

	Splitter split;
	GpuCtrlPane left;
	GpuCtrlPane right;
	Button hide_left;
	Button show_left;
	Button hide_right;
	Button show_right;
	bool auto_close = false;
	int script_step = 0;
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
	String left_error;
	String right_error;
	{
		Demo win(validation, auto_close);
		win.Open();
		opened = win.IsOpen();
		if(!opened) {
			Cout() << "FAIL: window did not open" << EOL;
			SetExitCode(1);
			return;
		}
		for(int i = 0; i < 200 && (!win.LeftGpuReady() || !win.RightGpuReady()) && win.LeftGpuError().IsEmpty() && win.RightGpuError().IsEmpty(); ++i)
			Ctrl::ProcessEvents();
		ready = win.LeftGpuReady() && win.RightGpuReady();
		left_error = win.LeftGpuError();
		right_error = win.RightGpuError();
		if(auto_close && (!ready || !left_error.IsEmpty() || !right_error.IsEmpty())) {
			Cout() << "FAIL: ready=" << GpuBoolText(ready)
			       << " left-error='" << left_error << "'"
			       << " right-error='" << right_error << "'" << EOL;
			SetExitCode(1);
			return;
		}
		win.Start();
		win.Run();
	}
	auto diag = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
	Cout() << "GpuCtrlMultiViewDemo diagnostics: " << GpuDiagText(diag) << EOL;
	if(auto_close) {
		if(!opened || !ready || !left_error.IsEmpty() || !right_error.IsEmpty() || diag.runtime_live_count != 0 || diag.instance_live_count != 0 || diag.device_live_count != 0 || diag.surface_live_count != 0) {
			SetExitCode(1);
			return;
		}
	}
	Cout() << "GUI_APP_MAIN exited" << EOL;
}
