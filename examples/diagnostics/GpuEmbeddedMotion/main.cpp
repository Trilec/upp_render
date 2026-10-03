#include <GpuRender/GpuRender.h>

using namespace Upp;

class EmbeddedMotionWindow : public TopWindow {
public:
	EmbeddedMotionWindow()
	{
		Title("GpuRender — Embedded GPU Motion").Sizeable().Zoomable().SetRect(0, 0, 960, 560);

		heading.SetLabel("Ordinary U++ window with one bounded Vulkan GpuCtrl");
		detail.SetLabel("The surrounding controls remain normal U++/GDI; only the dark motion panel owns a GPU surface.");
		name.SetData("Embedded GPU content");
		enabled.SetLabel("Animate");
		enabled <<= true;
		pause.SetLabel("Pause / Resume");
		pause.WhenAction = [=] {
			if(!paused)
				frozen_time = GetTickCount() / 1000.0;
			paused = !paused;
			status.SetLabel(paused ? "Animation paused" : "Animation running");
			gpu.RequestGpuRefresh();
		};
		status.SetLabel("Animation running");

		gpu.SetGpuPaint([=](GpuPainter& w) {
			const Size sz = w.GetSize();
			w.Clear(Rgba8(15, 20, 30, 255));
			if(sz.cx <= 0 || sz.cy <= 0)
				return;

			for(int x = 24; x < sz.cx; x += 48)
				w.FillRect(Rectf(x, 0, x + 1, sz.cy), Rgba8(27, 35, 50, 255));
			for(int y = 24; y < sz.cy; y += 48)
				w.FillRect(Rectf(0, y, sz.cx, y + 1), Rgba8(27, 35, 50, 255));

			const double t = paused ? frozen_time : GetTickCount() / 1000.0;
			const Rgba8 palette[] = {
				Rgba8(91, 173, 255, 230), Rgba8(111, 220, 181, 225), Rgba8(252, 190, 92, 230),
				Rgba8(194, 137, 255, 225), Rgba8(255, 120, 150, 225), Rgba8(115, 210, 232, 230)
			};
			const double rx = max(26.0, sz.cx * 0.35);
			const double ry = max(22.0, sz.cy * 0.31);
			const double cx = sz.cx * 0.50;
			const double cy = sz.cy * 0.56;
			for(int i = 0; i < 14; ++i) {
				const double phase = i * 0.67;
				const double x = cx + sin(t * (0.31 + (i % 3) * 0.035) + phase) * rx;
				const double y = cy + cos(t * (0.25 + (i % 4) * 0.031) + phase * 1.27) * ry;
				const double r = 7.0 + (i % 5) * 1.8;
				struct RoundedRect disc(Rectf(x - r, y - r, x + r, y + r), r);
				w.FillRoundedRect(disc, palette[i % 6]);
			}

			w.DrawText(Pointf(18, 16), String("Embedded GpuCtrl"), SansSerif(16).Bold(), Color(235, 241, 250));
			w.DrawText(Pointf(18, 39), String("One shared GPU context; independent child surface/swapchain"), SansSerif(12), Color(155, 170, 193));
		});

		Add(heading.HSizePos(24, 24).TopPos(20, 28));
		Add(detail.HSizePos(24, 24).TopPos(50, 24));
		Add(name.LeftPos(24, 250).TopPos(92, 28));
		Add(enabled.LeftPos(294, 120).TopPos(92, 28));
		Add(pause.LeftPos(434, 150).TopPos(90, 32));
		Add(status.LeftPos(604, 260).TopPos(94, 24));
		Add(gpu.HSizePos(24, 24).VSizePos(142, 24));

		SetTimeCallback(-33, [=] {
			if(!paused && (bool)~enabled)
				gpu.RequestGpuRefresh();
		});
	}

private:
	Label heading;
	Label detail;
	EditString name;
	Option enabled;
	Button pause;
	Label status;
	GpuCtrl gpu;
	bool paused = false;
	double frozen_time = 0.0;
};

GUI_APP_MAIN
{
	EmbeddedMotionWindow().Run();
}
