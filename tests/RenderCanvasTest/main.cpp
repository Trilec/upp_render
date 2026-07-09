#include <Painter/Painter.h>
#include <RenderSoftware/RenderSoftware.h>

using namespace Upp;

static bool Check(bool cond, const char *msg)
{
	if(!cond)
		Cout() << "FAIL: " << msg << EOL;
	return cond;
}

static Rectf FRect(double x, double y, double cx, double cy)
{
	return Rectf(x, y, x + cx, y + cy);
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

static bool ImagesEqual(const Image& a, const Image& b)
{
	if(a.GetSize() != b.GetSize())
		return false;
	for(int y = 0; y < a.GetHeight(); ++y)
		for(int x = 0; x < a.GetWidth(); ++x)
			if(a[y][x] != b[y][x])
				return false;
	return true;
}

static void BuildRecordedScene(UiCanvas& c)
{
	c.Save();
	c.ClipRect(Rectf(0, 0, 64, 64));
	c.ConcatTransform(Transform2D::Translation(4, 5));
	c.FillRect(FRect(0, 0, 20, 20), Rgba8(10, 20, 30, 40));
	c.FillRoundedRect(MakeRoundedRect(FRect(6, 7, 8, 9), 3), Rgba8(50, 60, 70, 80));
	c.StrokeRect(FRect(2, 3, 10, 11), 2.5, Rgba8(90, 100, 110, 120));
	c.Restore();
}

static bool TestRecording()
{
	UiDisplayList empty;
	if(!Check(!empty.IsValid(), "default display list should start invalid")) return false;
	if(!Check(!empty.GetError().IsEmpty(), "default display list should explain invalid state")) return false;

	UiDisplayListBuilder empty_builder;
	UiDisplayList empty_list;
	if(!Check(empty_builder.Finish(empty_list), "empty builder should finish")) return false;
	if(!Check(empty_list.IsValid(), "finished empty list should be valid")) return false;
	if(!Check(empty_list.GetCount() == 0, "finished empty list should be empty")) return false;

	UiDisplayListBuilder builder;
	BuildRecordedScene(builder);
	UiDisplayList list;
	if(!Check(builder.Finish(list), "balanced builder should finish")) return false;
	if(!Check(list.IsValid(), "finished list should be valid")) return false;
	if(!Check(list.GetCount() == 7, "operation count")) return false;
	if(!Check(list[0].type == UiDisplayOpType::Save, "save op type")) return false;
	if(!Check(list[1].type == UiDisplayOpType::ClipRect, "clip op type")) return false;
	if(!Check(list[2].type == UiDisplayOpType::ConcatTransform, "transform op type")) return false;
	if(!Check(list[3].type == UiDisplayOpType::FillRect, "fill op type")) return false;
	if(!Check(list[4].type == UiDisplayOpType::FillRoundedRect, "rounded fill op type")) return false;
	if(!Check(list[5].type == UiDisplayOpType::StrokeRect, "stroke op type")) return false;
	if(!Check(list[6].type == UiDisplayOpType::Restore, "restore op type")) return false;
	if(!Check(list[3].rect.left == 0 && list[3].rect.bottom == 20, "rect values preserved")) return false;
	if(!Check(list[4].rounded.radius == 3, "rounded radius preserved")) return false;
	if(!Check(list[5].width == 2.5, "stroke width preserved")) return false;
	if(!Check(list[3].color.a == 40, "alpha preserved")) return false;
	String dump = list.Dump();
	if(!Check(dump.Find("FillRect") >= 0, "dump contains operations")) return false;
	UiDisplayListBuilder closed;
	BuildRecordedScene(closed);
	UiDisplayList closed_list;
	if(!Check(closed.Finish(closed_list), "closed builder should finish")) return false;
	String before = closed_list.Dump();
	closed.Save();
	closed.FillRect(FRect(9, 9, 9, 9), Rgba8(1, 2, 3, 4));
	closed.Restore();
	UiDisplayList ignored;
	if(!Check(!closed.Finish(ignored), "finished builder must reject further recording")) return false;
	if(!Check(!closed.GetError().IsEmpty(), "finished builder should explain rejection")) return false;
	if(!Check(before == closed_list.Dump(), "completed list must remain immutable")) return false;
	return true;
}

static bool TestSaveRestore()
{
	UiDisplayListBuilder builder;
	builder.Restore();
	UiDisplayList bad;
	if(!Check(!builder.Finish(bad), "restore underflow should fail")) return false;
	if(!Check(!bad.IsValid(), "failed builder should not be valid")) return false;
	if(!Check(!bad.GetError().IsEmpty(), "failed builder should report error")) return false;

	UiDisplayListBuilder nested;
	nested.Save();
	nested.Save();
	nested.FillRect(FRect(0, 0, 1, 1), Rgba8());
	UiDisplayList unfinished;
	if(!Check(!nested.Finish(unfinished), "unfinished save depth should fail")) return false;
	if(!Check(!unfinished.IsValid(), "unfinished list should be invalid")) return false;
	return true;
}

static bool TestTransforms()
{
	Transform2D id = Transform2D::Identity();
	Transform2D t = Transform2D::Translation(10, 20);
	Transform2D s = Transform2D::Scale(2, 3);
	Pointf p(1, 2);
	Pointf pi = id.TransformPoint(p);
	Pointf pt = t.TransformPoint(p);
	Pointf ps = s.TransformPoint(p);
	Transform2D ts = s * t;
	Pointf pts = ts.TransformPoint(p);
	if(!Check(pi == p, "identity transform")) return false;
	if(!Check(pt.x == 11 && pt.y == 22, "translation transform")) return false;
	if(!Check(ps.x == 2 && ps.y == 6, "scale transform")) return false;
	if(!Check(pts.x == 12 && pts.y == 26, "concatenation order")) return false;
	return true;
}

static bool TestDeterminism()
{
	UiDisplayListBuilder a;
	BuildRecordedScene(a);
	UiDisplayList la;
	if(!Check(a.Finish(la), "first recording should finish")) return false;
	UiDisplayListBuilder b;
	BuildRecordedScene(b);
	UiDisplayList lb;
	if(!Check(b.Finish(lb), "second recording should finish")) return false;
	String da = la.Dump();
	String db = lb.Dump();
	if(!Check(da == db, "equivalent dumps should match")) return false;
	if(!Check(da == la.Dump(), "repeated dump should match")) return false;
	if(!Check(da.Find("0x") < 0, "dump should not contain pointer-like values")) return false;
	UiDisplayListBuilder fmt;
	fmt.ConcatTransform(Transform2D::Translation(1.25, 2.0));
	UiDisplayList formatted;
	if(!Check(fmt.Finish(formatted), "format recording should finish")) return false;
	String fdump = formatted.Dump();
	if(!Check(fdump.Find("1.25") >= 0, "dump should show trimmed float text")) return false;
	if(!Check(fdump.Find("1.250000") < 0, "dump should not keep trailing zeros")) return false;
	return true;
}

static Xform2D ToXform(const Transform2D& m)
{
	Xform2D x;
	x.x = m.x;
	x.y = m.y;
	x.t = m.t;
	return x;
}

static Image RenderSceneDirect()
{
	ImagePainter p(Size(64, 64));
	p.DrawRect(Rect(0, 0, 64, 64), Color(0, 0, 0));
	p.Rectangle(Rectf(0, 0, 64, 64));
	p.Fill(MakeRgba(0, 0, 0));
	p.Begin();
	p.Draw::Clip(Rect(0, 0, 32, 32));
	p.Rectangle(Rectf(0, 0, 64, 64));
	p.Fill(MakeRgba(255, 0, 0));
	p.End();
	p.Begin();
	p.Transform(ToXform(Transform2D::Translation(10, 0)));
	p.DrawRect(Rect(0, 0, 10, 10), Color(0, 255, 0));
	p.End();
	return p.GetResult();
}

static Image RenderSceneRecorded(bool valid)
{
	UiDisplayListBuilder builder;
	if(valid) {
		builder.FillRect(Rectf(0, 0, 64, 64), Rgba8(0, 0, 0, 255));
		builder.Save();
		builder.ClipRect(Rectf(0, 0, 32, 32));
		builder.FillRect(Rectf(0, 0, 64, 64), Rgba8(255, 0, 0, 255));
		builder.Restore();
		builder.Save();
		builder.ConcatTransform(Transform2D::Translation(10, 0));
		builder.FillRect(Rectf(0, 0, 10, 10), Rgba8(0, 255, 0, 255));
		builder.Restore();
	}
	else {
		builder.Restore();
	}
	UiDisplayList list;
	builder.Finish(list);
	ImagePainter p(Size(64, 64));
	p.DrawRect(Rect(0, 0, 64, 64), Color(0, 0, 0));
	SoftwareUiRenderer renderer;
	if(valid)
		Check(renderer.Replay(list, p), "valid list should replay");
	else
		Check(!renderer.Replay(list, p), "invalid list should reject");
	return p.GetResult();
}

static bool TestReplay()
{
	Image direct = RenderSceneDirect();
	Image replayed = RenderSceneRecorded(true);
	if(!Check(ImagesEqual(direct, replayed), "direct and replayed images should match")) return false;

	UiDisplayListBuilder root_clip_builder;
	root_clip_builder.FillRect(Rectf(0, 0, 32, 32), Rgba8(0, 0, 0, 255));
	root_clip_builder.ClipRect(Rectf(0, 0, 8, 8));
	root_clip_builder.FillRect(Rectf(0, 0, 32, 32), Rgba8(255, 255, 255, 255));
	UiDisplayList root_clip_list;
	if(!Check(root_clip_builder.Finish(root_clip_list), "root clip list should finish")) return false;
	ImagePainter root_clip_painter(Size(32, 32));
	root_clip_painter.DrawRect(Rect(0, 0, 32, 32), Color(0, 0, 0));
	SoftwareUiRenderer renderer;
	if(!Check(renderer.Replay(root_clip_list, root_clip_painter), "root clip replay should succeed")) return false;
	root_clip_painter.DrawRect(Rect(0, 0, 32, 32), Color(255, 255, 255));
	Image root_clip = root_clip_painter.GetResult();
	if(!Check(root_clip[0][0] == MakeRgba(255, 255, 255), "root clip should not leak after replay")) return false;
	if(!Check(root_clip[31][31] == MakeRgba(255, 255, 255), "root clip isolation should cover full image")) return false;

	UiDisplayListBuilder root_transform_builder;
	root_transform_builder.FillRect(Rectf(0, 0, 32, 32), Rgba8(0, 0, 0, 255));
	root_transform_builder.ConcatTransform(Transform2D::Translation(8, 0));
	root_transform_builder.FillRect(Rectf(0, 0, 8, 8), Rgba8(255, 255, 255, 255));
	UiDisplayList root_transform_list;
	if(!Check(root_transform_builder.Finish(root_transform_list), "root transform list should finish")) return false;
	ImagePainter root_transform_painter(Size(32, 32));
	root_transform_painter.DrawRect(Rect(0, 0, 32, 32), Color(0, 0, 0));
	if(!Check(renderer.Replay(root_transform_list, root_transform_painter), "root transform replay should succeed")) return false;
	root_transform_painter.DrawRect(Rect(0, 0, 32, 32), Color(255, 255, 255));
	Image root_transform = root_transform_painter.GetResult();
	if(!Check(root_transform[0][0] == MakeRgba(255, 255, 255), "root transform should not leak after replay")) return false;
	if(!Check(root_transform[31][31] == MakeRgba(255, 255, 255), "root transform isolation should cover full image")) return false;

	if(!Check(direct[5][5] == MakeRgba(255, 0, 0), "clipped red pixel")) return false;
	if(!Check(direct[40][40] == MakeRgba(0, 0, 0), "outside clip remains background")) return false;
	if(!Check(direct[1][1] == MakeRgba(255, 0, 0), "direct clip applies")) return false;
	if(!(direct[5][15] == MakeRgba(0, 255, 0))) {
		return Check(false, "translated green pixel");
	}

	UiDisplayListBuilder invalid_builder;
	invalid_builder.Save();
	invalid_builder.ClipRect(Rectf(0, 0, 8, 8));
	invalid_builder.FillRect(Rectf(0, 0, 32, 32), Rgba8(255, 0, 0, 255));
	UiDisplayList invalid_list;
	if(!Check(!invalid_builder.Finish(invalid_list), "unfinished list should fail")) return false;
	ImagePainter invalid_painter(Size(32, 32));
	invalid_painter.DrawRect(Rect(0, 0, 32, 32), Color(0, 0, 0));
	if(!Check(!renderer.Replay(invalid_list, invalid_painter), "invalid replay should reject")) return false;
	invalid_painter.DrawRect(Rect(0, 0, 32, 32), Color(255, 255, 255));
	Image invalid = invalid_painter.GetResult();
	if(!Check(invalid[0][0] == MakeRgba(255, 255, 255), "failed replay should not leak state")) return false;
	if(!Check(invalid[31][31] == MakeRgba(255, 255, 255), "failed replay isolation should cover full image")) return false;
	return true;
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	ok &= TestRecording();
	ok &= TestSaveRestore();
	ok &= TestTransforms();
	ok &= TestDeterminism();
	ok &= TestReplay();
	if(!ok) {
		SetExitCode(1);
		return;
	}
	Cout() << "RenderCanvasTest passed" << EOL;
}
