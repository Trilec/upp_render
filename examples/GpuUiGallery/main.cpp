#include <GpuRender/GpuRender.h>

using namespace Upp;

class ParticleSceneCtrl : public Ctrl {
public:
	ParticleSceneCtrl()
	{
		NoWantFocus();
		ImageDraw d(44, 44);
		d.DrawRect(0, 0, 44, 44, Color(27, 35, 52));
		d.DrawEllipse(6, 6, 32, 32, Color(86, 190, 255), 2, Color(210, 238, 255));
		d.DrawEllipse(16, 13, 9, 9, Color(255, 216, 112));
		badge = d;
		SetTimeCallback(-33, [=] { Refresh(); });
	}

	void Paint(Draw& w) override
	{
		const Size sz = GetSize();
		w.DrawRect(sz, Color(18, 24, 36));
		for(int x = 24; x < sz.cx; x += 48)
			w.DrawLine(x, 0, x, sz.cy, 1, Color(27, 35, 50));
		for(int y = 24; y < sz.cy; y += 48)
			w.DrawLine(0, y, sz.cx, y, 1, Color(27, 35, 50));

		const double t = GetTickCount() / 1000.0;
		const Color palette[] = {
			Color(91, 173, 255), Color(111, 220, 181), Color(252, 190, 92),
			Color(194, 137, 255), Color(255, 120, 150), Color(115, 210, 232)
		};
		const double rx = max(28.0, sz.cx * 0.34);
		const double ry = max(22.0, sz.cy * 0.30);
		const double cx = sz.cx * 0.50;
		const double cy = sz.cy * 0.54;
		for(int i = 0; i < 12; ++i) {
			const double phase = i * 0.73;
			const double x = cx + sin(t * (0.32 + (i % 3) * 0.035) + phase) * rx;
			const double y = cy + cos(t * (0.27 + (i % 4) * 0.028) + phase * 1.31) * ry;
			const int r = 7 + (i % 4) * 2;
			w.DrawEllipse(RectC((int)x - r, (int)y - r, 2 * r, 2 * r), palette[i % 6], 1, Color(225, 236, 248));
		}

		w.DrawImage(max(8, sz.cx - 58), 14, badge);
		w.DrawText(16, 14, "U++ Draw -> display list -> Vulkan", SansSerif(15).Bold(), Color(232, 239, 248));
		w.DrawText(16, 36, "A normal custom Ctrl; no Vulkan code here.", SansSerif(12), Color(156, 169, 190));

		const Rect arc_box = RectC(18, max(70, sz.cy - 66), 86, 42);
		w.DrawArc(arc_box, arc_box.RightCenter(), arc_box.LeftCenter(), 2, Color(111, 220, 181));
		w.DrawText(22, max(54, sz.cy - 22), "DrawArc", SansSerif(11).Bold(), Color(166, 231, 209));
		w.DrawText(max(110, sz.cx - 18), max(115, sz.cy - 18), 900,
		           "ROTATED DRAW", SansSerif(11).Bold(), Color(194, 137, 255));
	}

private:
	Image badge;
};

class GalleryDialog : public GpuTopWindow {
public:
	GalleryDialog()
	{
		Title("GPU modal dialog").SetRect(0, 0, 430, 235);
		title.SetLabel("This dialog is another GpuTopWindow surface.");
		name.SetData("Separate surface, shared GPU context");
		option.SetLabel("Keep GPU composition enabled");
		option <<= true;
		close.SetLabel("Close");
		close.WhenAction = [=] { Close(); };
		Add(title.HSizePos(22, 22).TopPos(20, 28));
		Add(name.HSizePos(22, 22).TopPos(64, 28));
		Add(option.HSizePos(22, 22).TopPos(108, 26));
		Add(close.RightPos(22, 100).BottomPos(20, 32));
	}
private:
	Label title;
	EditString name;
	Option option;
	Button close;
};

class GpuUiGallery : public GpuTopWindow {
public:
	GpuUiGallery()
	{
		Title("GpuRender — Full Vulkan U++ UI Gallery").Sizeable().Zoomable().SetRect(0, 0, 1040, 680);

		heading.SetLabel("Full U++ control tree on one Vulkan root surface");
		subheading.SetLabel("Layout, input, focus, state and theme remain U++; rendering is recorded and GPU-composited.");

		name.SetData("Editable text");
		enabled.SetLabel("Enabled option");
		enabled <<= true;
		mode.Add("Balanced");
		mode.Add("Quiet");
		mode.Add("Responsive");
		mode.SetIndex(0);

		level.Range(100);
		level <<= 58;
		progress.Set(58, 100);
		level.WhenAction = [=] {
			progress.Set((int)~level, 100);
			status.SetLabel("Slider changed through ordinary U++ input: " + AsString((int)~level));
		};

		apply.SetLabel("Apply");
		apply.WhenAction = [=] { status.SetLabel("Button action handled by U++ — GPU redraw requested normally."); };
		open_dialog.SetLabel("Open GPU dialog");
		open_dialog.WhenAction = [=] {
			GalleryDialog dlg;
			dlg.Run();
			status.SetLabel("Modal GpuTopWindow closed; parent surface remained alive.");
		};

		table.AddColumn("Control / area");
		table.AddColumn("Purpose");
		table.Add("Button + Option", "state / theme");
		table.Add("EditString + DropList", "text / popup entry point");
		table.Add("Slider + Progress", "interactive value drawing");
		table.Add("ArrayCtrl", "scrollable data view");
		table.Add("ParticleSceneCtrl", "animated custom Draw path");

		status.SetLabel("Waiting for interaction.");
		gpu_state.SetLabel("GPU status will update while the window is open.");

		Add(heading.HSizePos(24, 24).TopPos(18, 28));
		Add(subheading.HSizePos(24, 24).TopPos(48, 24));
		Add(name.LeftPos(24, 230).TopPos(92, 28));
		Add(enabled.LeftPos(274, 180).TopPos(92, 28));
		Add(mode.LeftPos(474, 180).TopPos(92, 28));
		Add(apply.LeftPos(674, 110).TopPos(90, 32));
		Add(open_dialog.LeftPos(800, 180).TopPos(90, 32));
		Add(level.LeftPos(24, 300).TopPos(142, 28));
		Add(progress.LeftPos(344, 310).TopPos(146, 20));
		Add(table.LeftPos(24, 440).VSizePos(198, 72));
		Add(particles.HSizePos(484, 24).VSizePos(198, 72));
		Add(status.HSizePos(24, 24).BottomPos(40, 24));
		Add(gpu_state.HSizePos(24, 24).BottomPos(14, 22));

		SetTimeCallback(-250, [=] {
			String e = GetGpuError();
			gpu_state.SetLabel(e.IsEmpty() ? "Vulkan root compositor active" : "GPU compositor: " + e);
		});
	}

private:
	Label heading;
	Label subheading;
	EditString name;
	Option enabled;
	DropList mode;
	SliderCtrl level;
	ProgressIndicator progress;
	Button apply;
	Button open_dialog;
	ArrayCtrl table;
	ParticleSceneCtrl particles;
	Label status;
	Label gpu_state;
};

GUI_APP_MAIN
{
	GpuUiGallery().Run();
}
