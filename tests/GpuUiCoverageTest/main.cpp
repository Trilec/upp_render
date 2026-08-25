#include <CtrlLib/CtrlLib.h>
#include <GpuRender/RenderCtrlBridge.h>
#include <RenderSoftware/RenderSoftware.h>

using namespace Upp;

static bool Check(bool condition, const char *message)
{
	if(!condition)
		Cout() << "FAIL: " << message << EOL;
	return condition;
}

class CoverageScene : public Ctrl {
public:
	CoverageScene()
	{
		ImageDraw d(36, 36);
		d.DrawRect(0, 0, 36, 36, Color(31, 39, 55));
		d.DrawEllipse(5, 5, 26, 26, Color(92, 186, 250), 2, Color(232, 242, 252));
		image = d;
	}

	void Paint(Draw& w) override
	{
		const Size sz = GetSize();
		w.DrawRect(sz, Color(20, 27, 39));
		w.DrawImage(12, 12, image);
		w.DrawText(58, 14, "custom U++ Draw scene", SansSerif(14).Bold(), Color(235, 241, 248));
		w.DrawEllipse(18, 62, 34, 34, Color(101, 208, 167), 2, Color(220, 244, 236));
		Vector<Point> poly;
		poly << Point(78, 62) << Point(116, 76) << Point(96, 108) << Point(66, 96);
		w.DrawPolygon(poly, Color(239, 174, 82), 2, Color(255, 232, 196));
		w.DrawLine(132, 66, max(133, sz.cx - 16), 100, 2, Color(181, 133, 244));
	}

private:
	Image image;
};

class CoverageRoot : public Ctrl {
public:
	CoverageRoot()
	{
		SetRect(0, 0, 980, 620);
		heading.SetLabel("Representative U++ control recording coverage");
		button.SetLabel("Button");
		option.SetLabel("Option");
		option <<= true;
		edit.SetData("EditString");
		drop.Add("Balanced");
		drop.Add("Quiet");
		drop.Add("Responsive");
		drop.SetIndex(0);
		slider.Range(100);
		slider <<= 61;
		progress.Set(61, 100);
		table.AddColumn("Control");
		table.AddColumn("State");
		table.Add("Button", "ready");
		table.Add("EditString", "ready");
		table.Add("DropList", "ready");
		table.Add("ArrayCtrl", "ready");

		Add(heading.HSizePos(20, 20).TopPos(16, 28));
		Add(button.LeftPos(20, 110).TopPos(62, 32));
		Add(option.LeftPos(150, 130).TopPos(64, 28));
		Add(edit.LeftPos(300, 210).TopPos(62, 30));
		Add(drop.LeftPos(530, 170).TopPos(62, 30));
		Add(slider.LeftPos(20, 300).TopPos(116, 28));
		Add(progress.LeftPos(340, 300).TopPos(120, 20));
		Add(table.LeftPos(20, 450).TopPos(170, 360));
		Add(scene.LeftPos(490, 460).TopPos(170, 210));
	}

	void Paint(Draw& w) override
	{
		w.DrawRect(GetSize(), Color(238, 242, 247));
		w.DrawRect(10, 10, GetSize().cx - 20, 145, Color(250, 252, 255));
	}

private:
	Label heading;
	Button button;
	Option option;
	EditString edit;
	DropList drop;
	SliderCtrl slider;
	ProgressIndicator progress;
	ArrayCtrl table;
	CoverageScene scene;
};

static bool ImageChanged(const Image& image, Color background)
{
	RGBA b = background;
	for(int y = 0; y < image.GetHeight(); ++y)
		for(int x = 0; x < image.GetWidth(); ++x)
			if(image[y][x] != b)
				return true;
	return false;
}

GUI_APP_MAIN
{
	bool ok = true;
	CoverageRoot root;
	UiDisplayList list;
	String error;
	CtrlDisplayListRecordReport report;

	ok &= Check(RecordCtrlDisplayList(root, list, error, &report),
	           "representative U++ control tree should record without unsupported Draw semantics");
	if(!error.IsEmpty())
		Cout() << "record error: " << error << EOL;
	ok &= Check(!report.HasUnsupportedOperation(), "control gallery must not report an unsupported operation");
	ok &= Check(list.IsValid() && list.GetCount() > 0, "control gallery should produce a valid non-empty display list");
	ok &= Check(report.rect_count > 0, "control gallery should contain rectangles");
	ok &= Check(report.text_count > 0, "control gallery should contain text");
	ok &= Check(report.image_count > 0, "control gallery should contain images");
	ok &= Check(report.path_count > 0, "control gallery should contain paths");
	ok &= Check(report.clip_count > 0 && report.transform_count > 0, "child controls should exercise clip/offset recording");

	if(list.IsValid()) {
		ImagePainter painter(root.GetSize());
		const Color background(3, 4, 5);
		painter.DrawRect(Rect(root.GetSize()), background);
		SoftwareUiRenderer software;
		ok &= Check(software.Replay(list, painter), "software reference should replay the gallery display list");
		ok &= Check(ImageChanged(painter.GetResult(), background), "software replay should produce visible gallery output");
	}

	Cout() << "rect=" << report.rect_count
	       << " text=" << report.text_count
	       << " image=" << report.image_count
	       << " path=" << report.path_count
	       << " clip=" << report.clip_count
	       << " transform=" << report.transform_count << EOL;

	if(ok) {
		Cout() << "GpuUiCoverageTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
