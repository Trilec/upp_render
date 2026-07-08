#include <CtrlLib/CtrlLib.h>
#include <RenderSoftware/RenderSoftware.h>

using namespace Upp;

namespace {

struct DemoScene {
	Size size = Size(360, 240);
	String dump;
	Image direct;
	Image replayed;
};

static Rectf FRect(double x, double y, double cx, double cy)
{
	return Rectf(x, y, x + cx, y + cy);
}

static Xform2D ToXform(const Transform2D& m)
{
	Xform2D x;
	x.x = m.x;
	x.y = m.y;
	x.t = m.t;
	return x;
}

static RGBA MakeRgba(byte r, byte g, byte b, byte a = 255)
{
	RGBA c;
	c.r = r;
	c.g = g;
	c.b = b;
	c.a = a;
	return c;
}

static struct RoundedRect MakeRoundedRect(const Rectf& rect, double radius)
{
	struct RoundedRect rr(rect, radius);
	return rr;
}

static void PaintScene(Painter& p)
{
	p.DrawRect(Rect(0, 0, 360, 240), Color(24, 28, 34));

	p.Draw::Clip(Rect(20, 20, 340, 180));
	p.Transform(ToXform(Transform2D::Translation(24, 20)));
	p.Rectangle(FRect(0, 0, 160, 100));
	p.Fill(MakeRgba(40, 70, 120));
	p.Rectangle(FRect(24, 18, 120, 44));
	p.Fill(MakeRgba(220, 90, 70, 180));
	p.RoundedRectangle(FRect(20, 60, 96, 30), 10);
	p.Fill(MakeRgba(70, 170, 120, 220));
	p.Begin();
	p.Transform(ToXform(Transform2D::Scale(1.25, 0.85)));
	p.RoundedRectangle(FRect(98, 42, 72, 48), 14);
	p.Fill(MakeRgba(210, 200, 90, 180));
	p.End();
	p.Rectangle(FRect(8, 8, 128, 84));
	p.Stroke(3, MakeRgba(240, 240, 245));
	p.End();

	p.Rectangle(FRect(214, 28, 88, 88));
	p.Fill(MakeRgba(90, 130, 240, 140));
	p.RoundedRectangle(FRect(230, 44, 84, 64), 18);
	p.Fill(MakeRgba(240, 110, 170, 180));
}

static void RecordScene(UiCanvas& canvas)
{
	canvas.FillRect(FRect(0, 0, 360, 240), Rgba8(24, 28, 34, 255));
	canvas.Save();
	canvas.ClipRect(Rectf(20, 20, 340, 180));
	canvas.ConcatTransform(Transform2D::Translation(24, 20));
	canvas.FillRect(FRect(0, 0, 160, 100), Rgba8(40, 70, 120, 255));
	canvas.FillRect(FRect(24, 18, 120, 44), Rgba8(220, 90, 70, 180));
	canvas.FillRoundedRect(MakeRoundedRect(FRect(20, 60, 96, 30), 10), Rgba8(70, 170, 120, 220));
	canvas.Save();
	canvas.ConcatTransform(Transform2D::Scale(1.25, 0.85));
	canvas.FillRoundedRect(MakeRoundedRect(FRect(98, 42, 72, 48), 14), Rgba8(210, 200, 90, 180));
	canvas.Restore();
	canvas.StrokeRect(FRect(8, 8, 128, 84), 3, Rgba8(240, 240, 245, 255));
	canvas.Restore();
	canvas.FillRect(FRect(214, 28, 88, 88), Rgba8(90, 130, 240, 140));
	canvas.FillRoundedRect(MakeRoundedRect(FRect(230, 44, 84, 64), 18), Rgba8(240, 110, 170, 180));
}

static Image RenderDirect(Size size)
{
	ImagePainter p(size);
	PaintScene(p);
	return p.GetResult();
}

static Image RenderRecorded(Size size, String& dump)
{
	UiDisplayListBuilder builder;
	RecordScene(builder);
	UiDisplayList list;
	if(!builder.Finish(list))
		throw Exc(builder.GetError());
	dump = list.Dump();
	ImagePainter p(size);
	p.DrawRect(Rect(0, 0, size.cx, size.cy), Color(24, 28, 34));
	SoftwareUiRenderer renderer;
	if(!renderer.Replay(list, p))
		throw Exc(renderer.GetError());
	return p.GetResult();
}

static DemoScene BuildScene()
{
	DemoScene s;
	s.direct = RenderDirect(s.size);
	s.replayed = RenderRecorded(s.size, s.dump);
	return s;
}

class DemoView : public Ctrl {
public:
	DemoView() {
		NoWantFocus();
		scene_ = BuildScene();
		Cout() << scene_.dump << EOL;
	}

	virtual void Paint(Draw& w) override
	{
		Rect r = GetSize();
		w.DrawRect(r, Color(30, 32, 36));
		Font title = SansSerifZ(DPI(12));
		Font mono = MonospaceZ(DPI(10));

		w.DrawText(DPI(16), DPI(14), "Direct reference", title, SWhite());
		w.DrawText(DPI(390), DPI(14), "Recorded and replayed", title, SWhite());

		Rect left(DPI(16), DPI(36), DPI(16) + scene_.size.cx, DPI(36) + scene_.size.cy);
		Rect right(DPI(390), DPI(36), DPI(390) + scene_.size.cx, DPI(36) + scene_.size.cy);
		w.DrawRect(left, Color(10, 10, 10));
		w.DrawRect(right, Color(10, 10, 10));
		w.DrawImage(left, scene_.direct);
		w.DrawImage(right, scene_.replayed);

		Rect dump_rc(DPI(16), DPI(300), GetSize().cx - DPI(16), GetSize().cy - DPI(16));
		w.DrawText(dump_rc.left, dump_rc.top - DPI(6), "Display list dump", title, SWhite());
		int y = dump_rc.top;
		for(const String& line : Split(scene_.dump, '\n')) {
			w.DrawText(dump_rc.left, y, line, mono, Color(180, 220, 255));
			y += DPI(14);
		}
	}

private:
	DemoScene scene_;
};

}

GUI_APP_MAIN
{
	TopWindow win;
	win.Title("DisplayListDemo");
	win.Sizeable().Zoomable();
	DemoView view;
	win.Add(view.SizePos());
	win.SetRect(0, 0, DPI(780), DPI(560));
	win.Run();
}
