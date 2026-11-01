#include <GpuRender/GpuRender.h>

using namespace Upp;

class EmbeddedGpuDemo : public TopWindow {
public:
	EmbeddedGpuDemo()
	{
		Title("GpuRender — embedded GpuCtrl").Sizeable().SetRect(0, 0, 760, 460);
		Add(gpu.SizePos());
		gpu.SetGpuPaint([](GpuPainter& w) {
			Size sz = w.GetSize();
			w.Clear(Color(24, 31, 45));
			if(sz.cx <= 0 || sz.cy <= 0)
				return;
			double margin = max(18.0, min(sz.cx, sz.cy) * 0.08);
			struct RoundedRect panel(Rectf(margin, margin, sz.cx - margin, sz.cy - margin), 22.0);
			w.FillRoundedRect(panel, Color(62, 112, 214));
			w.DrawText(Pointf(margin + 28, margin + 30), String("GpuCtrl — paint here"), SansSerif(28).Bold(), White());
			w.DrawText(Pointf(margin + 28, margin + 76), String("No HWND, Vulkan or swapchain code in the application."), SansSerif(16), Color(225, 235, 252));
		});
	}
private:
	GpuCtrl gpu;
};

GUI_APP_MAIN
{
	EmbeddedGpuDemo().Run();
}
