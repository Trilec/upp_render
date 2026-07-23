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
	Cout() << "GUI_APP_MAIN entered" << EOL;
	{
		Demo win(validation, auto_close);
		win.Open();
		if(!win.IsOpen()) {
			Cout() << "FAIL: window did not open" << EOL;
			SetExitCode(1);
			return;
		}
		win.Start();
		win.Run();
	}
	Cout() << "GUI_APP_MAIN exited" << EOL;
}
