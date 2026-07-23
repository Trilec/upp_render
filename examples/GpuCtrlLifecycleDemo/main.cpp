#include <GpuCtrl/GpuCtrl.h>

using namespace Upp;

namespace {

struct ShutdownState {
	bool called = false;
	bool clean = false;
	bool host_ready = false;
	bool gpu_ready = false;
	String error;
};

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
	Demo(bool validation, ShutdownState& shutdown_state)
		: validation(validation)
		, shutdown_state(&shutdown_state)
	{
		Title("GpuCtrl lifecycle");
		Add(gpu.SizePos());
		SetRect(0, 0, 480, 320);
		gpu.SetBackend(GpuBackendKind::Vulkan);
		gpu.SetBackend(GpuBackendKind::Vulkan);
		gpu.SetBackend(GpuBackendKind::Unknown);
		gpu.SetBackend(GpuBackendKind::Vulkan);
		gpu.SetValidation(validation);
		gpu.SetValidation(validation);
		gpu.SetShutdownCallback(callback(this, &Demo::OnGpuShutdown));
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

	bool NativeHostReady() const { return gpu.IsNativeHostReady(); }
	bool GpuReady() const { return gpu.IsGpuReady(); }
	String GpuError() const { return gpu.GetGpuError(); }
	bool ShutdownCalled() const { return shutdown_called; }
	bool ShutdownClean() const { return shutdown_clean; }
	bool ShutdownHostReady() const { return shutdown_host_ready; }
	bool ShutdownGpuReady() const { return shutdown_gpu_ready; }
	String ShutdownError() const { return shutdown_error; }

private:
	void OnGpuShutdown(bool host_ready, bool gpu_ready, const String& error)
	{
		shutdown_called = true;
		shutdown_host_ready = host_ready;
		shutdown_gpu_ready = gpu_ready;
		shutdown_error = error;
		shutdown_clean = !host_ready && !gpu_ready && error.IsEmpty();
		if(shutdown_state) {
			shutdown_state->called = shutdown_called;
			shutdown_state->clean = shutdown_clean;
			shutdown_state->host_ready = shutdown_host_ready;
			shutdown_state->gpu_ready = shutdown_gpu_ready;
			shutdown_state->error = shutdown_error;
		}
	}

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
	bool shutdown_called = false;
	bool shutdown_clean = false;
	bool shutdown_host_ready = false;
	bool shutdown_gpu_ready = false;
	String shutdown_error;
	ShutdownState* shutdown_state = nullptr;
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
	ShutdownState shutdown_state;
	{
		Demo win(validation, shutdown_state);
		win.Open();
		if(!win.IsOpen()) {
			Cout() << "FAIL: window did not open" << EOL;
			SetExitCode(1);
			return;
		}
		for(int i = 0; i < 100 && !win.NativeHostReady(); ++i)
			Ctrl::ProcessEvents();
		win.Start();
		win.Run();
	}
	Cout() << "GpuCtrlLifecycleDemo final clean shutdown: " << BoolText(shutdown_state.clean)
	       << " called=" << BoolText(shutdown_state.called)
	       << " host-ready=" << BoolText(shutdown_state.host_ready)
	       << " gpu-ready=" << BoolText(shutdown_state.gpu_ready)
	       << " error='" << shutdown_state.error << "'" << EOL;
	Cout() << "GUI_APP_MAIN exited" << EOL;
	if(!shutdown_state.called || !shutdown_state.clean)
		SetExitCode(1);
}
