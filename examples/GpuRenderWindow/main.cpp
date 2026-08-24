#include <GpuRender/GpuRender.h>

using namespace Upp;

class FullGpuWindow : public GpuWindow {
public:
	FullGpuWindow()
	{
		Title("GpuRender — custom GPU window").Sizeable().SetRect(0, 0, 900, 560);
	}

protected:
	void GpuPaint(GpuPainter& w) override
	{
		Size sz = w.GetSize();
		w.Clear(Color(18, 24, 36));
		if(sz.cx <= 0 || sz.cy <= 0)
			return;
		w.FillRect(Rectf(40, 40, sz.cx - 40, 130), Color(50, 90, 180));
		w.DrawText(Pointf(68, 68), String("GpuWindow"), SansSerif(34).Bold(), White());
		w.DrawText(Pointf(68, 112), String("The entire client area is your GPU drawing surface."), SansSerif(18), Color(220, 230, 248));
	}
};

GUI_APP_MAIN
{
	FullGpuWindow().Run();
}
