#include <CtrlLib/CtrlLib.h>
#include <RenderCtrlBridge/RenderCtrlBridge.h>
#include <RenderSoftware/RenderSoftware.h>

using namespace Upp;

static bool Check(bool condition, const char *message)
{
	if(!condition)
		Cout() << "FAIL: " << message << EOL;
	return condition;
}

static Image MakeProbeImage()
{
	ImageDraw draw(18, 18);
	draw.DrawRect(0, 0, 18, 18, Color(38, 52, 72));
	draw.DrawEllipse(3, 3, 12, 12, Color(245, 184, 62));
	return draw;
}

class ProbeCtrl : public Ctrl {
public:
	ProbeCtrl()
	{
		image = MakeProbeImage();
		NoWantFocus();
	}

	void Paint(Draw& w) override
	{
		Size sz = GetSize();
		w.DrawRect(sz, Color(244, 247, 251));
		w.DrawLine(4, 5, max(5, sz.cx - 5), 5, 2, Color(56, 118, 220));
		w.DrawEllipse(8, 14, 34, 26, Color(94, 201, 151), 2, Color(28, 88, 68));
		Vector<Point> triangle;
		triangle << Point(52, 14) << Point(82, 40) << Point(44, 42);
		w.DrawPolygon(triangle, Color(226, 96, 112), 1, Color(122, 38, 54));
		w.DrawImage(92, 16, image, Color(72, 132, 238));
		w.DrawText(8, max(48, sz.cy - 24), "custom child", SansSerif(14).Bold(), Color(35, 42, 55));
	}

private:
	Image image;
};

class RecordingRoot : public Ctrl {
public:
	RecordingRoot()
	{
		SetRect(0, 0, 360, 190);
		SetFrame(BlackFrame());
		probe.SetRect(14, 14, 142, 104);
		label.SetRect(178, 18, 160, 28);
		button.SetRect(178, 58, 112, 34);
		label.SetLabel("Resolved U++ label");
		button.SetLabel("Action");
		Add(probe);
		Add(label);
		Add(button);
	}

	void Paint(Draw& w) override
	{
		w.DrawRect(GetSize(), Color(25, 32, 45));
		w.DrawRect(166, 10, 182, 96, Color(234, 239, 247));
		w.DrawText(178, 108, "parent paint", SansSerif(15), Color(230, 235, 244));
	}

private:
	ProbeCtrl probe;
	Label label;
	Button button;
};

class ArcCtrl : public Ctrl {
public:
	ArcCtrl() { SetRect(0, 0, 80, 60); }
	void Paint(Draw& w) override
	{
		w.DrawArc(RectC(5, 5, 60, 40), Point(65, 25), Point(5, 25), 2, Red());
	}
};

static bool HasOp(const UiDisplayList& list, UiDisplayOpType type)
{
	for(int i = 0; i < list.GetCount(); i++)
		if(list[i].type == type)
			return true;
	return false;
}

static bool ImageHasVisibleChange(const Image& image, Color background)
{
	RGBA reference = background;
	for(int y = 0; y < image.GetHeight(); y++)
		for(int x = 0; x < image.GetWidth(); x++)
			if(image[y][x] != reference)
				return true;
	return false;
}

GUI_APP_MAIN
{
	bool ok = true;

	RecordingRoot root;
	UiDisplayList list;
	String error;
	CtrlDisplayListRecordReport report;
	ok &= Check(RecordCtrlDisplayList(root, list, error, &report),
	            "recursive U++ control tree should record through public Paint/frame APIs");
	if(!error.IsEmpty())
		Cout() << "record error: " << error << EOL;
	ok &= Check(list.IsValid() && list.GetCount() > 0, "recorded control list should be non-empty and valid");
	ok &= Check(report.rect_count > 0, "control recording should contain resolved rectangles");
	ok &= Check(report.text_count > 0, "control recording should contain resolved text");
	ok &= Check(report.image_count > 0, "control recording should contain resolved images");
	ok &= Check(report.path_count > 0, "control recording should contain line/polygon/ellipse paths");
	ok &= Check(report.clip_count > 0 && report.transform_count > 0,
	            "recursive child painting should preserve U++ clip and offset state");
	ok &= Check(!report.HasUnsupportedOperation(), "supported control scene should not hit an unsupported Draw operation");
	ok &= Check(HasOp(list, UiDisplayOpType::Save) && HasOp(list, UiDisplayOpType::Restore),
	            "recursive U++ Draw state should map to neutral Save/Restore");
	ok &= Check(HasOp(list, UiDisplayOpType::ConcatTransform) && HasOp(list, UiDisplayOpType::ClipRect),
	            "child positions and views should map to transform/clip operations");
	ok &= Check(HasOp(list, UiDisplayOpType::DrawText) && HasOp(list, UiDisplayOpType::DrawImage),
	            "resolved text and image drawing should remain semantic display-list operations");
	ok &= Check(HasOp(list, UiDisplayOpType::FillPath) && HasOp(list, UiDisplayOpType::StrokePath),
	            "ellipse/polygon/line drawing should use the existing neutral vector path contract");

	const String dump = list.Dump();
	UiDisplayList second;
	CtrlDisplayListRecordReport second_report;
	ok &= Check(RecordCtrlDisplayList(root, second, error, &second_report),
	            "same resolved control tree should record repeatedly");
	ok &= Check(second.Dump() == dump, "unchanged control state should produce a deterministic display list");

	ImagePainter painter(root.GetSize());
	painter.DrawRect(Rect(root.GetSize()), Color(3, 4, 5));
	SoftwareUiRenderer software;
	ok &= Check(software.Replay(list, painter), "software reference should replay recorded U++ control output");
	Image output = painter.GetResult();
	ok &= Check(ImageHasVisibleChange(output, Color(3, 4, 5)), "recorded control replay should produce visible output");
	ok &= Check(list.Dump() == dump, "software replay must not mutate recorded control intent");

	ArcCtrl unsupported;
	UiDisplayList unsupported_list;
	CtrlDisplayListRecordReport unsupported_report;
	error.Clear();
	ok &= Check(!RecordCtrlDisplayList(unsupported, unsupported_list, error, &unsupported_report),
	            "unsupported U++ Draw semantics should fail explicitly");
	ok &= Check(error.Find("DrawArc") >= 0 && unsupported_report.HasUnsupportedOperation(),
	            "unsupported operation should be named in deterministic evidence");

	if(ok) {
		Cout() << "RenderCtrlBridgeTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
