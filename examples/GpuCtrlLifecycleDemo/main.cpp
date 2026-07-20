#include <GpuCtrl/GpuCtrl.h>

using namespace Upp;

namespace {

class Demo : public TopWindow {
public:
	Demo()
	{
		Title("GpuCtrl lifecycle");
		Add(gpu.SizePos());
		gpu.SetBackend(GpuBackendKind::Vulkan);
		SetRect(0, 0, 480, 320);
		QueueStep();
	}

	void RunSequence()
	{
		if(step == 0)
			Cout() << "GpuCtrlLifecycleDemo started" << EOL;
		if(step == 1)
			Cout() << "GpuCtrl ready: " << (gpu.IsNativeHostReady() ? "yes" : "no") << ", error: " << gpu.GetGpuError() << EOL;
		if(step == 2) {
			Cout() << "GpuCtrl resize: 640x360" << EOL;
			SetRect(0, 0, 640, 360);
		}
		else if(step == 3) {
			Cout() << "GpuCtrl resize: 520x280" << EOL;
			SetRect(0, 0, 520, 280);
		}
		else if(step == 4) {
			Cout() << "GpuCtrl minimize" << EOL;
			Minimize();
		}
		else if(step == 5) {
			Cout() << "GpuCtrl restore" << EOL;
			Overlap();
		}
		else if(step == 6) {
			Cout() << "GpuCtrl hide" << EOL;
			Hide();
		}
		else if(step == 7) {
			Cout() << "GpuCtrl show" << EOL;
			Show();
		}
		else if(step == 8) {
			Cout() << "GpuCtrlLifecycleDemo closing" << EOL;
			Close();
			return;
		}

		gpu.RequestGpuRefresh();
		++step;
		QueueStep();
	}

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
	int step = 0;
};

}

GUI_APP_MAIN
{
	Demo win;
	win.Open();
	win.Run();
	Cout() << "GpuCtrlLifecycleDemo exited" << EOL;
}
