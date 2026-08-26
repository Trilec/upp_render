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
		Vector<Point> grouped;
		grouped << Point(46, 50) << Point(86, 50) << Point(86, 88) << Point(46, 88)
		        << Point(57, 60) << Point(75, 60) << Point(75, 77) << Point(57, 77)
		        << Point(98, 58) << Point(128, 84) << Point(94, 88);
		Vector<int> subpolygons;
		subpolygons << 4 << 4 << 3;
		Vector<int> disjuncts;
		disjuncts << 8 << 3;
		w.DrawPolyPolyPolygon(grouped, subpolygons, disjuncts, Color(108, 176, 232), 1, Color(38, 76, 118));
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
		Add(probe); Add(label); Add(button);
	}
	void Paint(Draw& w) override
	{
		w.DrawRect(GetSize(), Color(25, 32, 45));
		w.DrawRect(166, 10, 182, 96, Color(234, 239, 247));
		w.DrawText(178, 108, "parent paint", SansSerif(15), Color(230, 235, 244));
	}
private:
	ProbeCtrl probe; Label label; Button button;
};

class ArcCtrl : public Ctrl {
public:
	ArcCtrl() { SetRect(0, 0, 80, 60); }
	void Paint(Draw& w) override { w.DrawArc(RectC(5, 5, 60, 40), Point(65, 25), Point(5, 25), 2, Red()); }
};

class RotatedTextCtrl : public Ctrl {
public:
	RotatedTextCtrl() { SetRect(0, 0, 130, 120); }
	void Paint(Draw& w) override
	{
		w.DrawText(72, 102, 900, "rotate", SansSerif(18).Bold(), Color(42, 92, 196));
	}
};

class InvertRectCtrl : public Ctrl {
public:
	InvertRectCtrl() { SetRect(0, 0, 64, 48); }
	void Paint(Draw& w) override
	{
		w.DrawRect(GetSize(), Color(38, 52, 72));
		w.DrawRect(10, 10, 20, 20, InvertColor());
	}
};

#ifdef PLATFORM_WIN32
static const Color GLOBAL_STATE_PROBE_COLOR = Color(211, 37, 149);
class GlobalStateProbeCtrl : public Ctrl {
public:
	GlobalStateProbeCtrl() { SetRect(0, 0, 3, 3); BackPaint(FULLBACKPAINT); NoWantFocus(); }
	void Paint(Draw& w) override { w.DrawRect(0, 0, 1, 1, GLOBAL_STATE_PROBE_COLOR); }
};
class GlobalStateProbeDraw : public ImageDraw {
public:
	GlobalStateProbeDraw() : ImageDraw(3, 3) {}
	bool WasDirect() const { return direct; }
	void DrawRectOp(int x, int y, int cx, int cy, Color color) override
	{
		if(x == 0 && y == 0 && cx == 1 && cy == 1 && color == GLOBAL_STATE_PROBE_COLOR) direct = true;
		SystemDraw::DrawRectOp(x, y, cx, cy, color);
	}
private:
	bool direct = false;
};
static bool ProbeGlobalBackBuffer()
{
	GlobalStateProbeCtrl ctrl; GlobalStateProbeDraw draw; ctrl.DrawCtrl(draw, 0, 0); return draw.WasDirect();
}
class NativeDrawCtrl : public Ctrl {
public:
	NativeDrawCtrl() { SetRect(0, 0, 40, 30); }
	void Paint(Draw& w) override { w.BeginNative(); w.EndNative(); }
};
#endif

static bool HasOp(const UiDisplayList& list, UiDisplayOpType type)
{
	for(int i = 0; i < list.GetCount(); i++) if(list[i].type == type) return true;
	return false;
}
static int CountOps(const UiDisplayList& list, UiDisplayOpType type)
{
	int count = 0; for(int i = 0; i < list.GetCount(); i++) if(list[i].type == type) count++; return count;
}
static bool ImageHasVisibleChange(const Image& image, Color background)
{
	RGBA reference = background;
	for(int y = 0; y < image.GetHeight(); y++) for(int x = 0; x < image.GetWidth(); x++) if(image[y][x] != reference) return true;
	return false;
}
static bool Near(double a, double b, double tolerance = 0.01)
{
	return fabs(a - b) <= tolerance;
}
static bool HasCounterClockwiseArc(const UiDisplayList& list)
{
	for(int i = 0; i < list.GetCount(); i++) {
		const UiDisplayOp& op = list[i];
		if(op.type != UiDisplayOpType::StrokePath || op.path.GetCount() < 2)
			continue;
		const UiPathCommand& move = op.path[0];
		const UiPathCommand& cubic = op.path[1];
		if(move.verb == UiPathVerb::MoveTo && cubic.verb == UiPathVerb::CubicTo &&
		   Near(move.p1.x, 65.0) && Near(move.p1.y, 25.0) && cubic.p1.y < 25.0)
			return true;
	}
	return false;
}
static bool HasQuarterTurnTextTransform(const UiDisplayList& list)
{
	for(int i = 0; i < list.GetCount(); i++) {
		if(list[i].type != UiDisplayOpType::ConcatTransform)
			continue;
		const Transform2D& m = list[i].transform;
		if(Near(m.x.x, 0.0, 0.0001) && Near(m.x.y, 1.0, 0.0001) &&
		   Near(m.y.x, -1.0, 0.0001) && Near(m.y.y, 0.0, 0.0001))
			return true;
	}
	return false;
}

GUI_APP_MAIN
{
	bool ok = true;
#ifdef PLATFORM_WIN32
	const bool initial_global_backbuffer = ProbeGlobalBackBuffer();
	Ctrl::GlobalBackBuffer(false);
	ok &= Check(!ProbeGlobalBackBuffer(), "test precondition should observe disabled global backbuffer mode");
#endif
	RecordingRoot root; UiDisplayList list; String error; CtrlDisplayListRecordReport report;
	ok &= Check(RecordCtrlDisplayList(root, list, error, &report), "recursive U++ control tree should record through the public DrawCtrl/SystemDraw path");
	if(!error.IsEmpty()) Cout() << "record error: " << error << EOL;
	ok &= Check(list.IsValid() && list.GetCount() > 0, "recorded control list should be non-empty and valid");
	ok &= Check(report.rect_count > 0, "control recording should contain resolved rectangles");
	ok &= Check(report.text_count > 0, "control recording should contain resolved text");
	ok &= Check(report.image_count > 0, "control recording should contain resolved images");
	ok &= Check(report.path_count >= 5, "control recording should contain simple and grouped vector paths");
	ok &= Check(report.clip_count > 0 && report.transform_count > 0, "recursive child painting should preserve U++ clip and offset state");
	ok &= Check(!report.HasUnsupportedOperation(), "supported control scene should not hit an unsupported Draw operation");
	ok &= Check(HasOp(list, UiDisplayOpType::Save) && HasOp(list, UiDisplayOpType::Restore), "recursive U++ Draw state should map to neutral Save/Restore");
	ok &= Check(HasOp(list, UiDisplayOpType::ConcatTransform) && HasOp(list, UiDisplayOpType::ClipRect), "child positions and views should map to transform/clip operations");
	ok &= Check(HasOp(list, UiDisplayOpType::DrawText) && HasOp(list, UiDisplayOpType::DrawImage), "resolved text and image drawing should remain semantic display-list operations");
	ok &= Check(HasOp(list, UiDisplayOpType::FillPath) && HasOp(list, UiDisplayOpType::StrokePath), "ellipse/polygon/line drawing should use the existing neutral vector path contract");
	ok &= Check(CountOps(list, UiDisplayOpType::FillPath) >= 4, "grouped U++ polygon input should remain separate neutral fill-path groups");
#ifdef PLATFORM_WIN32
	ok &= Check(!ProbeGlobalBackBuffer(), "recording should restore a previously disabled U++ global backbuffer mode");
#endif
	const String dump = list.Dump();
	UiDisplayList second; CtrlDisplayListRecordReport second_report;
	ok &= Check(RecordCtrlDisplayList(root, second, error, &second_report), "same resolved control tree should record repeatedly");
	ok &= Check(second.Dump() == dump, "unchanged control state should produce a deterministic display list");
#ifdef PLATFORM_WIN32
	Ctrl::GlobalBackBuffer(true);
	ok &= Check(ProbeGlobalBackBuffer(), "test precondition should observe enabled global backbuffer mode");
	UiDisplayList direct_list; CtrlDisplayListRecordReport direct_report; error.Clear();
	ok &= Check(RecordCtrlDisplayList(root, direct_list, error, &direct_report), "recording should also work when U++ direct backbuffer mode was already enabled");
	ok &= Check(ProbeGlobalBackBuffer(), "recording should preserve a previously enabled U++ global backbuffer mode");
	ok &= Check(direct_list.Dump() == dump, "U++ global backbuffer state must not change recorded semantic output");
	Ctrl::GlobalBackBuffer(false);
#endif
	ImagePainter painter(root.GetSize()); painter.DrawRect(Rect(root.GetSize()), Color(3, 4, 5)); SoftwareUiRenderer software;
	ok &= Check(software.Replay(list, painter), "software reference should replay recorded U++ control output");
	Image output = painter.GetResult();
	ok &= Check(ImageHasVisibleChange(output, Color(3, 4, 5)), "recorded control replay should produce visible output");
	ok &= Check(list.Dump() == dump, "software replay must not mutate recorded control intent");

	ArcCtrl arc; UiDisplayList arc_list; CtrlDisplayListRecordReport arc_report; error.Clear();
	ok &= Check(RecordCtrlDisplayList(arc, arc_list, error, &arc_report), "DrawArc should record through the neutral vector path contract");
	if(!error.IsEmpty()) Cout() << "arc record error: " << error << EOL;
	ok &= Check(!arc_report.HasUnsupportedOperation() && HasOp(arc_list, UiDisplayOpType::StrokePath), "DrawArc should remain semantic and supported");
	ok &= Check(HasCounterClockwiseArc(arc_list), "DrawArc should preserve Win32/U++ default counter-clockwise direction");
	ImagePainter arc_painter(arc.GetSize()); arc_painter.DrawRect(Rect(arc.GetSize()), White());
	ok &= Check(software.Replay(arc_list, arc_painter), "software reference should replay recorded DrawArc output");
	ok &= Check(ImageHasVisibleChange(arc_painter.GetResult(), White()), "recorded DrawArc should produce visible output");

	RotatedTextCtrl rotated_text; UiDisplayList rotated_list; CtrlDisplayListRecordReport rotated_report; error.Clear();
	ok &= Check(RecordCtrlDisplayList(rotated_text, rotated_list, error, &rotated_report), "rotated DrawText should record through neutral transform plus text operations");
	if(!error.IsEmpty()) Cout() << "rotated text record error: " << error << EOL;
	ok &= Check(!rotated_report.HasUnsupportedOperation() && rotated_report.text_count > 0, "rotated DrawText should remain semantic and supported");
	ok &= Check(HasOp(rotated_list, UiDisplayOpType::DrawText) && HasQuarterTurnTextTransform(rotated_list), "90-degree U++ text should preserve tenth-degree rotation convention");
	ImagePainter rotated_painter(rotated_text.GetSize()); rotated_painter.DrawRect(Rect(rotated_text.GetSize()), White());
	ok &= Check(software.Replay(rotated_list, rotated_painter), "software reference should replay rotated DrawText output");
	ok &= Check(ImageHasVisibleChange(rotated_painter.GetResult(), White()), "recorded rotated DrawText should produce visible output");

	InvertRectCtrl invert_rect; UiDisplayList invert_list; CtrlDisplayListRecordReport invert_report; error.Clear();
	ok &= Check(RecordCtrlDisplayList(invert_rect, invert_list, error, &invert_report),
	            "destination-invert rectangle should record through the neutral display-list contract");
	ok &= Check(error.IsEmpty() && !invert_report.HasUnsupportedOperation(),
	            "destination-invert rectangle should not report an unsupported operation");
	ok &= Check(HasOp(invert_list, UiDisplayOpType::InvertRect),
	            "destination-invert rectangle should remain a first-class display-list operation");
	ok &= Check(invert_list.Dump().Find("InvertRect 10 10 30 30") >= 0,
	            "destination-invert rectangle dump should preserve deterministic geometry");
	ImagePainter invert_painter(invert_rect.GetSize());
	invert_painter.DrawRect(Rect(invert_rect.GetSize()), Color(3, 4, 5));
	ok &= Check(software.Replay(invert_list, invert_painter),
	            "software reference should replay destination-invert rectangle output");
	Image invert_output = invert_painter.GetResult();
	ok &= Check(invert_output[15][15] != invert_output[2][2],
	            "destination-invert software replay should change pixels inside the inverted region");
 #ifdef PLATFORM_WIN32
	NativeDrawCtrl native_draw; UiDisplayList native_draw_list; CtrlDisplayListRecordReport native_draw_report; error.Clear();
	ok &= Check(!RecordCtrlDisplayList(native_draw, native_draw_list, error, &native_draw_report), "native SystemDraw/GDI semantics should not be silently omitted");
	ok &= Check(error.Find("native") >= 0 && native_draw_report.HasUnsupportedOperation(), "native drawing boundary should be explicit in evidence");
	DHCtrl native_child; native_child.SetRect(0, 0, 40, 30); UiDisplayList native_list; CtrlDisplayListRecordReport native_report; error.Clear();
	ok &= Check(!RecordCtrlDisplayList(native_child, native_list, error, &native_report), "native child-window controls should not be silently omitted");
	ok &= Check(error.Find("ExcludeClip") >= 0 && native_report.HasUnsupportedOperation(), "native child-window exclusion boundary should be explicit in evidence");
	Ctrl::GlobalBackBuffer(initial_global_backbuffer);
	ok &= Check(ProbeGlobalBackBuffer() == initial_global_backbuffer, "focused test should restore the process global backbuffer state it inherited");
#endif
	if(ok) { Cout() << "RenderCtrlBridgeTest passed" << EOL; return; }
	SetExitCode(1);
}
