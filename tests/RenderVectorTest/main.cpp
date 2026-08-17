#include <RenderSoftware/RenderSoftware.h>
#include <RenderVector/RenderVector.h>

using namespace Upp;

static bool Check(bool condition, const char *message)
{
	if(!condition)
		Cout() << "FAIL: " << message << EOL;
	return condition;
}

static RGBA MakeRgba(byte r, byte g, byte b, byte a = 255)
{
	RGBA color;
	color.r = r;
	color.g = g;
	color.b = b;
	color.a = a;
	return color;
}

static bool ImageDiffersFrom(const Image& image, RGBA reference)
{
	for(int y = 0; y < image.GetHeight(); ++y)
		for(int x = 0; x < image.GetWidth(); ++x)
			if(image[y][x] != reference)
				return true;
	return false;
}

static bool HasPartialAlpha(const Image& image)
{
	for(int y = 0; y < image.GetHeight(); ++y)
		for(int x = 0; x < image.GetWidth(); ++x)
			if(image[y][x].a > 0 && image[y][x].a < 255)
				return true;
	return false;
}

static UiPath MakePath()
{
	UiPath path;
	path.MoveTo(Pointf(8, 8));
	path.LineTo(Pointf(84, 8));
	path.CubicTo(Pointf(96, 18), Pointf(96, 42), Pointf(84, 52));
	path.LineTo(Pointf(8, 52));
	path.QuadraticTo(Pointf(0, 30), Pointf(8, 8));
	path.Close();
	path.MoveTo(Pointf(30, 22));
	path.LineTo(Pointf(62, 22));
	path.LineTo(Pointf(62, 38));
	path.LineTo(Pointf(30, 38));
	path.Close();
	return path;
}

static UiPaint MakeGradient()
{
	UiPaint paint = UiPaint::Linear(Pointf(8, 8), Pointf(92, 52),
	                                Rgba8(30, 90, 210, 230),
	                                Rgba8(235, 90, 40, 210), UiGradientSpread::Reflect);
	paint.AddStop(0.5, Rgba8(120, 220, 160, 220));
	return paint;
}

static UiStrokeStyle MakeStroke()
{
	UiStrokeStyle stroke;
	stroke.width = 3.0;
	stroke.cap = UiLineCap::Round;
	stroke.join = UiLineJoin::Round;
	stroke.miter_limit = 6.0;
	stroke.dash << 7.0 << 3.0;
	stroke.dash_offset = 1.5;
	return stroke;
}

static String SampleSvg()
{
	return "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'>"
	       "<path d='M4 26 L16 4 L28 26 Z' fill='#56a8e8'/>"
	       "<circle cx='16' cy='19' r='4' fill='#ffffff'/></svg>";
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	const UiPath path = MakePath();
	const UiPaint gradient = MakeGradient();
	const UiStrokeStyle stroke = MakeStroke();

	String reason;
	ok &= Check(gradient.IsValid(&reason), "multi-stop reflected linear gradient should be valid");
	ok &= Check(stroke.IsValid(&reason), "round dashed stroke style should be valid");
	UiPaint invalid = UiPaint::Linear(Pointf(1, 1), Pointf(1, 1),
	                                  Rgba8(0, 0, 0), Rgba8(255, 255, 255));
	ok &= Check(!invalid.IsValid(&reason), "zero-length linear gradient should be rejected");

	UiPaint repeat_radial = UiPaint::Radial(Pointf(18, 16), Pointf(24, 22), 18.0,
	                                        Rgba8(255, 255, 255), Rgba8(30, 60, 120),
	                                        UiGradientSpread::Repeat);
	repeat_radial.AddStop(0.4, Rgba8(120, 210, 170));
	ok &= Check(repeat_radial.IsValid(&reason), "multi-stop repeated radial gradient should be valid");
	ok &= Check(repeat_radial.Dump().Find("spread=Repeat") >= 0,
	            "radial paint dump should preserve Repeat spread semantics");

	UiDisplayListBuilder builder;
	builder.Save();
	builder.ClipRect(Rectf(0, 0, 128, 78));
	builder.FillPath(path, gradient, UiFillRule::EvenOdd);
	builder.StrokePath(path, UiPaint::Solid(Rgba8(245, 245, 250, 220)), stroke);
	builder.DrawSvg(Rectf(94, 10, 124, 40), SampleSvg());
	builder.Restore();
	UiDisplayList list;
	ok &= Check(builder.Finish(list), "vector display list should finish");
	ok &= Check(list.IsValid() && list.GetCount() == 6,
	            "vector list should contain Save/Clip/fill/stroke/SVG/Restore");
	if(list.GetCount() == 6) {
		ok &= Check(list[2].type == UiDisplayOpType::FillPath &&
		            list[2].fill_rule == UiFillRule::EvenOdd,
		            "fill path and even-odd rule should be preserved");
		ok &= Check(list[2].path == path && list[2].paint == gradient,
		            "path and gradient values should be deep copied");
		ok &= Check(list[3].type == UiDisplayOpType::StrokePath && list[3].stroke == stroke,
		            "stroke style should be preserved");
		ok &= Check(list[4].type == UiDisplayOpType::DrawSvg && list[4].svg == SampleSvg(),
		            "SVG source should be preserved immutably");
	}

	const String dump = list.Dump();
	ok &= Check(dump.Find("FillPath rule=EvenOdd") >= 0,
	            "dump should expose vector fill rule");
	ok &= Check(dump.Find("C 96 18 96 42 84 52") >= 0 &&
	            dump.Find("Q 0 30 8 8") >= 0,
	            "dump should preserve cubic and quadratic commands");
	ok &= Check(dump.Find("Linear 8 8 92 52") >= 0 &&
	            dump.Find("spread=Reflect") >= 0 &&
	            dump.Find("0.5:rgba(120,220,160,220)") >= 0,
	            "dump should preserve reflected multi-stop gradient evidence");
	ok &= Check(dump.Find("cap=Round join=Round") >= 0 && dump.Find("dash=7,3 offset=1.5") >= 0,
	            "dump should preserve stroke style evidence");
	ok &= Check(dump.Find("DrawSvg 94 10 124 40 bytes=") >= 0 && dump.Find(" hash=") >= 0,
	            "dump should identify SVG by stable content evidence");
	ok &= Check(dump == list.Dump(), "vector display-list dump should be deterministic");

	UiDisplayListBuilder equivalent_builder;
	equivalent_builder.Save();
	equivalent_builder.ClipRect(Rectf(0, 0, 128, 78));
	equivalent_builder.FillPath(MakePath(), MakeGradient(), UiFillRule::EvenOdd);
	equivalent_builder.StrokePath(MakePath(), UiPaint::Solid(Rgba8(245, 245, 250, 220)), MakeStroke());
	equivalent_builder.DrawSvg(Rectf(94, 10, 124, 40), SampleSvg());
	equivalent_builder.Restore();
	UiDisplayList equivalent;
	ok &= Check(equivalent_builder.Finish(equivalent), "equivalent vector list should finish");
	ok &= Check(equivalent.Dump() == dump, "equivalent vector values should dump identically");

	const RGBA background = MakeRgba(5, 8, 12, 255);
	ImagePainter painter(Size(132, 82));
	painter.DrawRect(Rect(0, 0, 132, 82), Color(5, 8, 12));
	SoftwareUiRenderer software;
	ok &= Check(software.Replay(list, painter), "software reference should replay vector/gradient/SVG intent");
	ok &= Check(software.GetError().IsEmpty(), "vector software replay should retain no error");
	Image output = painter.GetResult();
	ok &= Check(ImageDiffersFrom(output, background), "vector software replay should produce visible output");
	ok &= Check(list.Dump() == dump, "software vector replay must not mutate the display list");

	Image raster;
	Rectf raster_rect;
	String raster_error;
	ok &= Check(RasterizeUiVectorOp(list[2], 2.0, raster, raster_rect, raster_error),
	            "shared vector raster authority should rasterize a reflected gradient path");
	ok &= Check(!raster.IsEmpty() && raster_rect.right > raster_rect.left && raster_rect.bottom > raster_rect.top,
	            "shared vector raster should return image plus local bounds");

	UiDisplayOp repeat_op;
	repeat_op.type = UiDisplayOpType::FillPath;
	repeat_op.path = MakePath();
	repeat_op.paint = repeat_radial;
	repeat_op.fill_rule = UiFillRule::NonZero;
	Image repeat_raster;
	Rectf repeat_rect;
	ok &= Check(RasterizeUiVectorOp(repeat_op, 1.0, repeat_raster, repeat_rect, raster_error),
	            "shared Painter authority should rasterize Repeat radial gradients");
	ok &= Check(!repeat_raster.IsEmpty(), "repeated radial gradient raster should be visible");

	UiDisplayOp aa_op;
	aa_op.type = UiDisplayOpType::FillPath;
	aa_op.path = MakePath();
	aa_op.paint = UiPaint::Solid(Rgba8(255, 255, 255, 255));
	aa_op.fill_rule = UiFillRule::EvenOdd;
	Image aa_raster;
	Rectf aa_rect;
	ok &= Check(RasterizeUiVectorOp(aa_op, 1.0, aa_raster, aa_rect, raster_error),
	            "opaque curved path should rasterize through antialiased Painter mode");
	ok &= Check(HasPartialAlpha(aa_raster),
	            "opaque curved path should contain partial-alpha edge coverage proving antialiasing");

	UiPath malformed;
	malformed.LineTo(Pointf(4, 4));
	UiDisplayListBuilder malformed_builder;
	malformed_builder.FillPath(malformed, UiPaint::Solid(Rgba8(255, 255, 255)));
	UiDisplayList malformed_list;
	ok &= Check(malformed_builder.Finish(malformed_list), "malformed path should still record immutably");
	ImagePainter malformed_painter(Size(16, 16));
	ok &= Check(!software.Replay(malformed_list, malformed_painter),
	            "software replay should reject a path segment before MoveTo");

	if(ok) {
		Cout() << "RenderVectorTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
