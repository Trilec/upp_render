#include <GpuRender/GpuRender.h>

using namespace Upp;

class GpuUiWindow : public GpuTopWindow {
public:
	GpuUiWindow()
	{
		Title("GpuRender — GPU-composited U++ UI").Sizeable().SetRect(0, 0, 760, 420);
		title.SetLabel("Ordinary U++ controls, one root GPU surface");
		button.SetLabel("U++ Button");
		button.WhenAction = [=] { status.SetLabel("Button action handled by U++"); };
		status.SetLabel("U++ still owns layout, input, focus and state.");
		Add(title.HSizePos(28, 28).TopPos(28, 30));
		Add(button.LeftPos(28, 150).TopPos(82, 34));
		Add(status.HSizePos(28, 28).TopPos(140, 30));
	}
private:
	Label title;
	Button button;
	Label status;
};

GUI_APP_MAIN
{
	GpuUiWindow().Run();
}
