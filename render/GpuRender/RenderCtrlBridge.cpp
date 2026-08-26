#include "RenderCtrlBridge.h"

namespace Upp {

#ifdef PLATFORM_WIN32
namespace {

static Rgba8 ToRgba8(Color color)
{
	RGBA c = color;
	return Rgba8(c.r, c.g, c.b, c.a);
}

static Rect OffsetRect(const Rect& r, Point p)
{
	Rect q = r;
	q.Offset(p);
	return q;
}

static Rectf ToRectf(const Rect& r)
{
	return Rectf(r.left, r.top, r.right, r.bottom);
}

static Image CropImage(const Image& image, const Rect& source)
{
	Rect src = source & Rect(image.GetSize());
	if(src.IsEmpty())
		return Image();
	if(src == Rect(image.GetSize()))
		return image;
	ImageBuffer out(src.GetSize());
	for(int y = 0; y < src.GetHeight(); y++)
		Copy(out[y], image[src.top + y] + src.left, src.GetWidth());
	return Image(out);
}

static Transform2D Translation(Point p)
{
	return Transform2D::Translation(p.x, p.y);
}

static Transform2D TextRotation(Point origin, int angle)
{
	const double radians = angle * M_PI / 1800.0;
	const double sine = sin(radians);
	const double cosine = cos(radians);
	Transform2D transform;
	transform.x = Pointf(cosine, sine);
	transform.y = Pointf(-sine, cosine);
	transform.t = Pointf(origin.x, origin.y);
	return transform;
}

static UiPath MakeEllipsePath(const Rect& rect)
{
	UiPath path;
	if(rect.IsEmpty())
		return path;
	const double cx = (rect.left + rect.right) * 0.5;
	const double cy = (rect.top + rect.bottom) * 0.5;
	const double rx = rect.GetWidth() * 0.5;
	const double ry = rect.GetHeight() * 0.5;
	const double k = 0.5522847498307936;
	path.MoveTo(Pointf(cx + rx, cy));
	path.CubicTo(Pointf(cx + rx, cy + k * ry), Pointf(cx + k * rx, cy + ry), Pointf(cx, cy + ry));
	path.CubicTo(Pointf(cx - k * rx, cy + ry), Pointf(cx - rx, cy + k * ry), Pointf(cx - rx, cy));
	path.CubicTo(Pointf(cx - rx, cy - k * ry), Pointf(cx - k * rx, cy - ry), Pointf(cx, cy - ry));
	path.CubicTo(Pointf(cx + k * rx, cy - ry), Pointf(cx + rx, cy - k * ry), Pointf(cx + rx, cy));
	path.Close();
	return path;
}

static Pointf ArcPoint(double cx, double cy, double rx, double ry, double angle)
{
	return Pointf(cx + rx * cos(angle), cy - ry * sin(angle));
}

static Pointf ArcDerivative(double rx, double ry, double angle)
{
	return Pointf(-rx * sin(angle), -ry * cos(angle));
}

static double ArcRadialAngle(Point point, double cx, double cy, double rx, double ry)
{
	const double x = rx != 0 ? (point.x - cx) / rx : 0;
	const double y = ry != 0 ? (cy - point.y) / ry : 0;
	return atan2(y, x);
}

static UiPath MakeCounterClockwiseArcPath(const Rect& rect, Point start, Point end)
{
	UiPath path;
	if(rect.IsEmpty())
		return path;
	const double cx = (rect.left + rect.right) * 0.5;
	const double cy = (rect.top + rect.bottom) * 0.5;
	const double rx = rect.GetWidth() * 0.5;
	const double ry = rect.GetHeight() * 0.5;
	if(rx <= 0 || ry <= 0)
		return path;

	const double start_angle = ArcRadialAngle(start, cx, cy, rx, ry);
	const double end_angle = ArcRadialAngle(end, cx, cy, rx, ry);
	const double full_turn = 2.0 * M_PI;
	double sweep = end_angle - start_angle;
	while(sweep <= 0)
		sweep += full_turn;
	if(start == end)
		sweep = full_turn;

	const int segment_count = max(1, (int)ceil(sweep / (M_PI * 0.5)));
	const double segment_sweep = sweep / segment_count;
	path.MoveTo(ArcPoint(cx, cy, rx, ry, start_angle));
	for(int i = 0; i < segment_count; i++) {
		const double a0 = start_angle + i * segment_sweep;
		const double a1 = a0 + segment_sweep;
		const double k = (4.0 / 3.0) * tan((a1 - a0) * 0.25);
		const Pointf p0 = ArcPoint(cx, cy, rx, ry, a0);
		const Pointf p1 = ArcPoint(cx, cy, rx, ry, a1);
		const Pointf d0 = ArcDerivative(rx, ry, a0);
		const Pointf d1 = ArcDerivative(rx, ry, a1);
		path.CubicTo(Pointf(p0.x + k * d0.x, p0.y + k * d0.y),
		             Pointf(p1.x - k * d1.x, p1.y - k * d1.y), p1);
	}
	return path;
}

static bool ConfigureStroke(int width, UiStrokeStyle& style)
{
	if(IsNull(width) || width == PEN_NULL)
		return false;
	style.width = width > 0 ? width : 1.0;
	style.cap = UiLineCap::Butt;
	style.join = UiLineJoin::Miter;
	switch(width) {
	case PEN_DASH:
		style.dash << 4.0 << 4.0;
		break;
#ifndef PLATFORM_WINCE
	case PEN_DOT:
		style.dash << 1.0 << 3.0;
		break;
	case PEN_DASHDOT:
		style.dash << 4.0 << 3.0 << 1.0 << 3.0;
		break;
	case PEN_DASHDOTDOT:
		style.dash << 4.0 << 3.0 << 1.0 << 3.0 << 1.0 << 3.0;
		break;
#endif
	default:
		break;
	}
	return true;
}

// CtrlCore does not expose a getter for GlobalBackBuffer(). A FULLBACKPAINT
// probe lets us observe the current setting without touching private state:
// when global-backbuffer mode is enabled, DrawCtrl paints directly into the
// supplied SystemDraw; otherwise FULLBACKPAINT is rendered through BackDraw and
// copied with BitBlt, bypassing this DrawRectOp override.
static const Color DIRECT_PAINT_PROBE_COLOR = Color(17, 203, 91);

class DirectPaintProbeCtrl : public Ctrl {
public:
	DirectPaintProbeCtrl()
	{
		SetRect(0, 0, 3, 3);
		BackPaint(FULLBACKPAINT);
		NoWantFocus();
	}

	void Paint(Draw& w) override
	{
		w.DrawRect(0, 0, 1, 1, DIRECT_PAINT_PROBE_COLOR);
	}
};

class DirectPaintProbeDraw : public ImageDraw {
public:
	DirectPaintProbeDraw()
		: ImageDraw(3, 3)
	{
	}

	bool WasDirect() const { return direct; }

	void DrawRectOp(int x, int y, int cx, int cy, Color color) override
	{
		if(x == 0 && y == 0 && cx == 1 && cy == 1 && color == DIRECT_PAINT_PROBE_COLOR)
			direct = true;
		SystemDraw::DrawRectOp(x, y, cx, cy, color);
	}

private:
	bool direct = false;
};

static bool IsGlobalBackBufferEnabled()
{
	DirectPaintProbeCtrl ctrl;
	DirectPaintProbeDraw draw;
	ctrl.DrawCtrl(draw, 0, 0);
	return draw.WasDirect();
}

class ScopedDirectCtrlPainting {
public:
	ScopedDirectCtrlPainting()
	{
		if(!IsGlobalBackBufferEnabled()) {
			Ctrl::GlobalBackBuffer(true);
			enabled_by_scope = true;
		}
	}

	~ScopedDirectCtrlPainting()
	{
		if(enabled_by_scope)
			Ctrl::GlobalBackBuffer(false);
	}

private:
	bool enabled_by_scope = false;
};

class CtrlDisplayListSystemDraw : public SystemDraw {
public:
	CtrlDisplayListSystemDraw(UiDisplayListBuilder& target, Size size,
	                          CtrlDisplayListRecordReport& target_report)
		: builder(target), page_size(size), report(target_report)
	{
		state.clip = Rect(size);
	}

	bool Failed() const { return !error.IsEmpty(); }
	const String& GetError() const { return error; }

	dword GetInfo() const override { return NATIVE; }
	Size GetPageSize() const override { return page_size; }
	Size GetNativeDpi() const override { return Size(96, 96); }
	Rect GetPaintRect() const override
	{
		Rect r = state.clip;
		r.Offset(-state.offset);
		return r;
	}
	int GetCloffLevel() const override { return stack.GetCount(); }

	void BeginNative() override
	{
		Fail("native SystemDraw/GDI drawing is not supported by the neutral control recorder");
	}

	void EndNative() override {}

	void BeginOp() override
	{
		PushState();
	}

	void EndOp() override
	{
		if(stack.IsEmpty()) {
			Fail("unbalanced Draw::End");
			return;
		}
		builder.Restore();
		state = stack.Top();
		stack.Drop();
	}

	void OffsetOp(Point p) override
	{
		PushState();
		state.offset += p;
		builder.ConcatTransform(Translation(p));
		report.transform_count++;
	}

	bool ClipOp(const Rect& r) override
	{
		PushState();
		state.clip &= OffsetRect(r, state.offset);
		builder.ClipRect(ToRectf(r));
		report.clip_count++;
		return !state.clip.IsEmpty();
	}

	bool ClipoffOp(const Rect& r) override
	{
		PushState();
		state.clip &= OffsetRect(r, state.offset);
		builder.ClipRect(ToRectf(r));
		builder.ConcatTransform(Translation(r.TopLeft()));
		state.offset += r.TopLeft();
		report.clip_count++;
		report.transform_count++;
		return !state.clip.IsEmpty();
	}

	bool ExcludeClipOp(const Rect&) override
	{
		Fail("ExcludeClip (including native child-window cutouts) is not representable by the current rectangular neutral clip contract");
		return false;
	}

	bool IntersectClipOp(const Rect& r) override
	{
		state.clip &= OffsetRect(r, state.offset);
		builder.ClipRect(ToRectf(r));
		report.clip_count++;
		return !state.clip.IsEmpty();
	}

	bool IsPaintingOp(const Rect& r) const override
	{
		return state.clip.Intersects(OffsetRect(r, state.offset));
	}

	void DrawRectOp(int x, int y, int cx, int cy, Color color) override
	{
		if(cx <= 0 || cy <= 0 || IsNull(color))
			return;
		if(color == InvertColor()) {
			builder.InvertRect(Rectf(x, y, x + cx, y + cy));
			report.rect_count++;
			return;
		}
		builder.FillRect(Rectf(x, y, x + cx, y + cy), ToRgba8(color));
		report.rect_count++;
	}

	void SysDrawImageOp(int x, int y, const Image& image, const Rect& src, Color color) override
	{
		Image resolved = CropImage(image, src);
		if(resolved.IsEmpty())
			return;
		if(color == InvertColor()) {
			Fail("invert image drawing is not supported by the neutral compositor");
			return;
		}
		if(!IsNull(color))
			resolved = CachedSetColorKeepAlpha(resolved, color);
		builder.DrawImage(Rectf(x, y, x + resolved.GetWidth(), y + resolved.GetHeight()), resolved);
		report.image_count++;
	}

	void DrawLineOp(int x1, int y1, int x2, int y2, int width, Color color) override
	{
		if(IsNull(color))
			return;
		if(color == InvertColor()) {
			Fail("invert line drawing is not supported by the neutral compositor");
			return;
		}
		UiStrokeStyle stroke;
		if(!ConfigureStroke(width, stroke))
			return;
		UiPath path;
		path.MoveTo(Pointf(x1, y1)).LineTo(Pointf(x2, y2));
		builder.StrokePath(path, UiPaint::Solid(ToRgba8(color)), stroke);
		report.line_count++;
		report.path_count++;
	}

	void DrawPolyPolylineOp(const Point *vertices, int vertex_count,
	                        const int *counts, int count_count,
	                        int width, Color color, Color doxor) override
	{
		if(!IsNull(doxor)) {
			Fail("XOR polyline drawing is not supported by the neutral compositor");
			return;
		}
		if(IsNull(color) || !vertices || !counts || vertex_count <= 0 || count_count <= 0)
			return;
		if(color == InvertColor()) {
			Fail("invert polyline drawing is not supported by the neutral compositor");
			return;
		}
		UiStrokeStyle stroke;
		if(!ConfigureStroke(width, stroke))
			return;
		UiPath path;
		int index = 0;
		for(int i = 0; i < count_count; i++) {
			const int n = counts[i];
			if(n < 0 || index + n > vertex_count) {
				Fail("invalid U++ polyline vertex counts");
				return;
			}
			if(n > 0) {
				path.MoveTo(Pointf(vertices[index].x, vertices[index].y));
				for(int j = 1; j < n; j++)
					path.LineTo(Pointf(vertices[index + j].x, vertices[index + j].y));
			}
			index += n;
		}
		if(index != vertex_count) {
			Fail("U++ polyline counts do not consume all vertices");
			return;
		}
		if(!path.IsEmpty()) {
			builder.StrokePath(path, UiPaint::Solid(ToRgba8(color)), stroke);
			report.line_count += count_count;
			report.path_count++;
		}
	}

	void DrawPolyPolyPolygonOp(const Point *vertices, int vertex_count,
	                           const int *subpolygon_counts, int subpolygon_count_count,
	                           const int *disjunct_polygon_counts, int disjunct_polygon_count_count,
	                           Color color, int width, Color outline,
	                           uint64 pattern, Color doxor) override
	{
		if(pattern != 0 || !IsNull(doxor)) {
			Fail("pattern/XOR polygon drawing is not supported by the neutral compositor");
			return;
		}
		if((!IsNull(color) && color == InvertColor()) || (!IsNull(outline) && outline == InvertColor())) {
			Fail("invert polygon drawing is not supported by the neutral compositor");
			return;
		}
		if(!vertices || !subpolygon_counts || vertex_count <= 0 || subpolygon_count_count <= 0)
			return;

		int vertex_index = 0;
		int subpolygon_index = 0;
		int group_count = disjunct_polygon_counts && disjunct_polygon_count_count > 0
		                ? disjunct_polygon_count_count : 1;
		for(int group = 0; group < group_count; group++) {
			int group_vertices = disjunct_polygon_counts && disjunct_polygon_count_count > 0
			                   ? disjunct_polygon_counts[group]
			                   : vertex_count;
			if(group_vertices <= 0 || vertex_index + group_vertices > vertex_count) {
				Fail("invalid U++ disjunct polygon vertex count");
				return;
			}

			UiPath path;
			int consumed = 0;
			while(consumed < group_vertices) {
				if(subpolygon_index >= subpolygon_count_count) {
					Fail("U++ polygon grouping exhausts subpolygon counts early");
					return;
				}
				int n = subpolygon_counts[subpolygon_index++];
				if(n <= 0 || consumed + n > group_vertices || vertex_index + n > vertex_count) {
					Fail("invalid U++ subpolygon vertex count");
					return;
				}
				path.MoveTo(Pointf(vertices[vertex_index].x, vertices[vertex_index].y));
				for(int j = 1; j < n; j++)
					path.LineTo(Pointf(vertices[vertex_index + j].x, vertices[vertex_index + j].y));
				path.Close();
				vertex_index += n;
				consumed += n;
			}

			if(!IsNull(color))
				builder.FillPath(path, UiPaint::Solid(ToRgba8(color)), UiFillRule::EvenOdd);
			UiStrokeStyle stroke;
			if(!IsNull(outline) && ConfigureStroke(width, stroke))
				builder.StrokePath(path, UiPaint::Solid(ToRgba8(outline)), stroke);
			report.path_count++;
		}
		if(vertex_index != vertex_count || subpolygon_index != subpolygon_count_count)
			Fail("U++ polygon grouping does not consume all supplied geometry");
	}

	void DrawArcOp(const Rect& r, Point start, Point end, int width, Color color) override
	{
		if(IsNull(color))
			return;
		if(color == InvertColor()) {
			Fail("invert arc drawing is not supported by the neutral compositor");
			return;
		}
		UiStrokeStyle stroke;
		if(!ConfigureStroke(width, stroke))
			return;
		UiPath arc = MakeCounterClockwiseArcPath(r, start, end);
		if(arc.IsEmpty())
			return;
		builder.StrokePath(arc, UiPaint::Solid(ToRgba8(color)), stroke);
		report.path_count++;
	}

	void DrawEllipseOp(const Rect& r, Color color, int pen, Color pencolor) override
	{
		if((!IsNull(color) && color == InvertColor()) || (!IsNull(pencolor) && pencolor == InvertColor())) {
			Fail("invert ellipse drawing is not supported by the neutral compositor");
			return;
		}
		UiPath ellipse = MakeEllipsePath(r);
		if(ellipse.IsEmpty())
			return;
		if(!IsNull(color))
			builder.FillPath(ellipse, UiPaint::Solid(ToRgba8(color)), UiFillRule::NonZero);
		UiStrokeStyle stroke;
		if(!IsNull(pencolor) && ConfigureStroke(pen, stroke))
			builder.StrokePath(ellipse, UiPaint::Solid(ToRgba8(pencolor)), stroke);
		report.path_count++;
	}

	void DrawTextOp(int x, int y, int angle, const wchar *text, Font font,
	                Color ink, int n, const int *dx) override
	{
		if(!text || n <= 0 || IsNull(ink))
			return;
		if(ink == InvertColor()) {
			Fail("invert text drawing is not supported by the neutral compositor");
			return;
		}
		const bool rotated = angle != 0;
		if(rotated) {
			builder.Save();
			builder.ConcatTransform(TextRotation(Point(x, y), angle));
			report.transform_count++;
			x = 0;
			y = 0;
		}
		if(!dx) {
			WString value;
			value.Cat(text, n);
			builder.DrawText(Pointf(x, y), value, font, ToRgba8(ink));
			report.text_count++;
		}
		else {
			int advance = 0;
			for(int i = 0; i < n; i++) {
				WString glyph;
				glyph.Cat(text + i, 1);
				builder.DrawText(Pointf(x + advance, y), glyph, font, ToRgba8(ink));
				advance += dx[i];
				report.text_count++;
			}
		}
		if(rotated)
			builder.Restore();
	}

private:
	struct State : Moveable<State> {
		Point offset = Point(0, 0);
		Rect clip;
	};

	void PushState()
	{
		stack.Add(state);
		builder.Save();
	}

	void Fail(const String& message)
	{
		if(error.IsEmpty()) {
			error = message;
			report.unsupported_operation = message;
		}
	}

	UiDisplayListBuilder& builder;
	Size page_size;
	CtrlDisplayListRecordReport& report;
	State state;
	Vector<State> stack;
	String error;
};

} // namespace
#endif

bool RecordCtrlDisplayList(Ctrl& ctrl, UiDisplayList& out, String& error,
                           CtrlDisplayListRecordReport *report)
{
	error.Clear();
	CtrlDisplayListRecordReport local_report;
	CtrlDisplayListRecordReport& result = report ? *report : local_report;
	result = CtrlDisplayListRecordReport();

#ifdef PLATFORM_WIN32
	GuiLock gui_lock;
	UiDisplayListBuilder builder;
	CtrlDisplayListSystemDraw draw(builder, ctrl.GetSize(), result);
	{
		ScopedDirectCtrlPainting direct_paint;
		ctrl.DrawCtrl(draw, 0, 0);
	}
	if(draw.Failed()) {
		error = draw.GetError();
		return false;
	}
	if(draw.GetCloffLevel() != 0) {
		error = "U++ control drawing left an unbalanced Draw state stack";
		return false;
	}
	if(!builder.Finish(out)) {
		error = builder.GetError();
		return false;
	}
	if(!out.IsValid()) {
		error = out.GetError();
		return false;
	}
	return true;
#else
	(void)ctrl;
	(void)out;
	result.unsupported_operation = "control recording SystemDraw adapter is not implemented for this platform";
	error = result.unsupported_operation;
	return false;
#endif
}

}
