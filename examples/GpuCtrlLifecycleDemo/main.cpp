#include <GpuCtrl/GpuCtrl.h>

using namespace Upp;

namespace {

static bool HasArg(const Vector<String>& args, const char *name)
{
	for(const String& arg : args)
		if(arg == name)
			return true;
	return false;
}

static String BoolText(bool value)
{
	return value ? "yes" : "no";
}

class Demo : public TopWindow {
public:
	Demo(bool validation)
		: validation(validation)
	{
		Title("GpuCtrl lifecycle");
		Add(gpu.SizePos());
		SetRect(0, 0, 480, 320);
		gpu.SetBackend(GpuBackendKind::Vulkan);
		gpu.SetValidation(validation);
		Cout() << "GpuCtrlLifecycleDemo config: backend=Vulkan validation=" << BoolText(validation) << EOL;
	}

	void RunSequence()
	{
		switch(step) {
		case 0:
			Cout() << "GpuCtrlLifecycleDemo step=0 host-ready=" << BoolText(gpu.IsNativeHostReady())
			       << " gpu-ready=" << BoolText(gpu.IsGpuReady())
			       << " validation=" << BoolText(validation)
			       << " backend=Vulkan"
			       << " error='" << gpu.GetGpuError() << "'" << EOL;
			break;
		case 1:
			Cout() << "GpuCtrlLifecycleDemo step=1 reject backend change while open" << EOL;
			gpu.SetBackend(GpuBackendKind::Null);
			Cout() << "GpuCtrlLifecycleDemo host-ready=" << BoolText(gpu.IsNativeHostReady())
			       << " gpu-ready=" << BoolText(gpu.IsGpuReady())
			       << " error='" << gpu.GetGpuError() << "'" << EOL;
			break;
		case 2:
			Cout() << "GpuCtrlLifecycleDemo step=2 reject validation change while open" << EOL;
			gpu.SetValidation(!validation);
			Cout() << "GpuCtrlLifecycleDemo host-ready=" << BoolText(gpu.IsNativeHostReady())
			       << " gpu-ready=" << BoolText(gpu.IsGpuReady())
			       << " error='" << gpu.GetGpuError() << "'" << EOL;
			break;
		case 3:
			Cout() << "GpuCtrlLifecycleDemo resize: 640x360" << EOL;
			SetRect(0, 0, 640, 360);
			break;
		case 4:
			Cout() << "GpuCtrlLifecycleDemo resize: 520x280" << EOL;
			SetRect(0, 0, 520, 280);
			break;
		case 5:
			Cout() << "GpuCtrlLifecycleDemo minimize" << EOL;
			Minimize();
			break;
		case 6:
			Cout() << "GpuCtrlLifecycleDemo restore" << EOL;
			Overlap();
			break;
		case 7:
			Cout() << "GpuCtrlLifecycleDemo hide" << EOL;
			Hide();
			break;
		case 8:
			Cout() << "GpuCtrlLifecycleDemo show" << EOL;
			Show();
			break;
		case 9:
			Cout() << "GpuCtrlLifecycleDemo closing" << EOL;
			Close();
			return;
		}

		gpu.RequestGpuRefresh();
		++step;
		QueueStep();
	}

	void Start() { QueueStep(); }

private:
	void QueueStep()
	{
		Ptr<Demo> self = this;
		PostCallback([self] {
			if(self)
				self->RunSequence();
		});
	}

	GpuCtrl gpu;
	bool validation = false;
	int step = 0;
};

}

GUI_APP_MAIN
{
	Vector<String> args;
	for(int i = 1; i < __argc; ++i)
		args.Add(__argv[i]);

	bool validation = HasArg(args, "--validation");
	Cout() << "GUI_APP_MAIN entered" << EOL;
	{
		Demo win(validation);
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
