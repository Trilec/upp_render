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
	Demo(bool left_validation, bool right_validation, bool auto_close)
		: auto_close(auto_close)
		, left_validation(left_validation)
		, right_validation(right_validation)
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

		left.Create();
		right.Create();
		left->SetValidation(left_validation);
		right->SetValidation(right_validation);
		split.Horz(*left, *right);
		Add(split.HSizePos(8, 8).VSizePos(44, 8));

		left->SetTitle("Left GPU control");
		right->SetTitle("Right GPU control");
		left->UpdateStatus();
		right->UpdateStatus();
	}

	void Start()
	{
		for(int i = 0; i < 100 && (!LeftGpuReady() || !RightGpuReady()) && LeftGpuError().IsEmpty() && RightGpuError().IsEmpty(); ++i)
			Ctrl::ProcessEvents();
		Cout() << "GpuCtrlMultiViewDemo start left: " << left->DescribeStatus() << EOL;
		Cout() << "GpuCtrlMultiViewDemo start right: " << right->DescribeStatus() << EOL;
		left_diag = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		right_diag = left_diag;
		if(auto_close)
			ScriptStep();
		else
			Tick();
	}

	bool LeftGpuReady() const { return left && left->gpu.IsGpuReady(); }
	bool RightGpuReady() const { return right && right->gpu.IsGpuReady(); }
	String LeftGpuError() const { return left ? left->gpu.GetGpuError() : String(); }
	String RightGpuError() const { return right ? right->gpu.GetGpuError() : String(); }

private:
	void OnHideLeft()
	{
		if(left)
			left->Hide();
	}

	void OnShowLeft()
	{
		if(left)
			left->Show();
	}

	void OnHideRight()
	{
		if(right)
			right->Hide();
	}

	void OnShowRight()
	{
		if(right)
			right->Show();
	}

	void DestroyLeft()
	{
		if(left) {
			split.Remove(*left);
			left.Clear();
		}
	}

	void DestroyRight()
	{
		if(right) {
			split.Remove(*right);
			right.Clear();
		}
	}


	void ScriptStep()
	{
		if(left)
			left->UpdateStatus();
		if(right)
			right->UpdateStatus();
		switch(script_step++) {
		case 0:
			SetRect(0, 0, 1140, 700);
			Cout() << "GpuCtrlMultiViewDemo resize wide left: " << left->DescribeStatus() << EOL;
			Cout() << "GpuCtrlMultiViewDemo resize wide right: " << right->DescribeStatus() << EOL;
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
			Cout() << "GpuCtrlMultiViewDemo resize narrow left: " << left->DescribeStatus() << EOL;
			Cout() << "GpuCtrlMultiViewDemo resize narrow right: " << right->DescribeStatus() << EOL;
			break;
		case 4:
			DestroyLeft();
			Ctrl::ProcessEvents();
			right_diag = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
			Cout() << "GpuCtrlMultiViewDemo destroy left diagnostics: " << GpuDiagText(right_diag) << EOL;
			break;
		case 5:
			DestroyRight();
			Ctrl::ProcessEvents();
			Cout() << "GpuCtrlMultiViewDemo destroy right" << EOL;
			break;
		default:
			Close();
			return;
		}
		SetTimeCallback(250, callback(this, &Demo::ScriptStep));
	}

	void Tick()
	{
		if(left)
			left->UpdateStatus();
		if(right)
			right->UpdateStatus();
		if(IsOpen())
			SetTimeCallback(100, callback(this, &Demo::Tick));
	}

	Splitter split;
	One<GpuCtrlPane> left;
	One<GpuCtrlPane> right;
	Button hide_left;
	Button show_left;
	Button hide_right;
	Button show_right;
	bool auto_close = false;
	bool left_validation = false;
	bool right_validation = false;
	int script_step = 0;
	VulkanTestHooks::VulkanRuntimeDeviceDiagnostics left_diag;
	VulkanTestHooks::VulkanRuntimeDeviceDiagnostics right_diag;
};

}

GUI_APP_MAIN
{
	Vector<String> args;
	for(int i = 1; i < __argc; ++i)
		args.Add(__argv[i]);

	bool validation = HasArg(args, "--validation");
	bool mixed_validation = HasArg(args, "--mixed-validation");
	bool auto_close = HasArg(args, "--auto-close");
	VulkanTestHooks::ClearVulkanRuntimeDeviceDiagnostics();
	Cout() << "GUI_APP_MAIN entered" << EOL;
	bool opened = false;
	bool ready = false;
	String left_error;
	String right_error;
	{
		Demo win(mixed_validation || validation, validation && !mixed_validation ? validation : false, auto_close);
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
