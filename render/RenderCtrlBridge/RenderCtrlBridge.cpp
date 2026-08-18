#include "RenderCtrlBridge.h"

namespace Upp {
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

class CtrlDisplayListDraw : public Draw {
public:
	CtrlDisplayListDraw(UiDisplayListBuilder& target, Size size,
	                    CtrlDisplayListRecordReport& target_report)
		: builder(target), page_size(size), report(target_report)
	{
		state.clip = Rect(size);
	}

	bool Failed() const { return !error.IsEmpty(); }
	const String& GetError() const { return error; }

	bool Record(Ctrl& ctrl)
	{
		if(Failed())
			return false;
		Size size = ctrl.GetSize();
		if(!ctrl.IsShown() || size.cx <= 0 || size.cy <= 0)
			return true;

		// FramePaint needs the same progressively reduced outer rectangle as
		// CtrlCore. FrameLayout is applied to a local rectangle only; after frames
		// are painted, GetView() remains the authoritative resolved layout geometry.
		Rect frame_view(size);
		for(int i = 0; i < ctrl.GetFrameCount(); i++) {
			CtrlFrame& frame = ctrl.GetFrame(i);
			frame.FramePaint(*this, frame_view);
			if(Failed())
				return false;
			frame.FrameLayout(frame_view);
		}
		Rect view = ctrl.GetView();

		// Frame children live in the outer control coordinate system and paint
		// before the owner's view, matching CtrlCore's established ordering.
		for(Ctrl *child = ctrl.GetFirstChild(); child; child = child->GetNext())
			if(child->IsShown() && child->InFrame()) {
				Offset(child->GetRect().TopLeft());
				bool child_ok = Record(*child);
				End();
				if(!child_ok)
					return false;
			}

		if(!view.IsEmpty()) {
			Clipoff(view);
			ctrl.Paint(*this);
			End();
			if(Failed())
				return false;
		}

		// Ordinary children are positioned relative to the resolved view. U++
		// remains the sole layout authority; this bridge consumes those rectangles.
		if(!view.IsEmpty()) {
			bool visible = Clip(view);
			if(visible) {
				for(Ctrl *child = ctrl.GetFirstChild(); child; child = child->GetNext())
					if(child->IsShown() && child->InView()) {
						Point offset = child->GetRect().TopLeft() + view.TopLeft();
						Offset(offset);
						bool child_ok = Record(*child);
						End();
						if(!child_ok) {
							End();
							return false;
						}
					}
			}
			End();
		}
		return !Failed();
	}

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
		Fail("ExcludeClip is not representable by the current rectangular neutral clip contract");
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
		if(color == InvertColor) {
			Fail("invert rectangle drawing is not supported by the neutral compositor");
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
		if(!IsNull(color))
			resolved = CachedSetColorKeepAlpha(resolved, color);
		builder.DrawImage(Rectf(x, y, x + resolved.GetWidth(), y + resolved.GetHeight()), resolved);
		report.image_count++;
	}

	void DrawLineOp(int x1, int y1, int x2, int y2, int width, Color color) override
	{
		if(IsNull(color))
			return;
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
		if(!path.IsEmpty()) {
			builder.StrokePath(path, UiPaint::Solid(ToRgba8(color)), stroke);
			report.line_count += count_count;
			report.path_count++;
		}
	}

	void DrawPolyPolyPolygonOp(const Point *vertices, int vertex_count,
	                           const int *subpolygon_counts, int subpolygon_count_count,
	                           const int *, int,
	                           Color color, int width, Color outline,
	                           uint64 pattern, Color doxor) override
	{
		if(pattern != 0 || !IsNull(doxor)) {
			Fail("pattern/XOR polygon drawing is not supported by the neutral compositor");
			return;
		}
		if(!vertices || !subpolygon_counts || vertex_count <= 0 || subpolygon_count_count <= 0)
			return;
		UiPath path;
		int index = 0;
		for(int i = 0; i < subpolygon_count_count; i++) {
			const int n = subpolygon_counts[i];
			if(n < 0 || index + n > vertex_count) {
				Fail("invalid U++ polygon vertex counts");
				return;
			}
			if(n > 0) {
				path.MoveTo(Pointf(vertices[index].x, vertices[index].y));
				for(int j = 1; j < n; j++)
					path.LineTo(Pointf(vertices[index + j].x, vertices[index + j].y));
				path.Close();
			}
			index += n;
		}
		if(path.IsEmpty())
			return;
		if(!IsNull(color))
			builder.FillPath(path, UiPaint::Solid(ToRgba8(color)), UiFillRule::NonZero);
		UiStrokeStyle stroke;
		if(!IsNull(outline) && ConfigureStroke(width, stroke))
			builder.StrokePath(path, UiPaint::Solid(ToRgba8(outline)), stroke);
		report.path_count++;
	}

	void DrawArcOp(const Rect&, Point, Point, int, Color) override
	{
		Fail("DrawArc is not yet represented by the neutral control recorder");
	}

	void DrawEllipseOp(const Rect& r, Color color, int pen, Color pencolor) override
	{
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
		if(angle != 0) {
			Fail("rotated DrawText is not yet represented by the neutral control recorder");
			return;
		}
		if(!dx) {
			WString value;
			value.Cat(text, n);
			builder.DrawText(Pointf(x, y), value, font, ToRgba8(ink));
			report.text_count++;
			return;
		}
		int advance = 0;
		for(int i = 0; i < n; i++) {
			WString glyph;
			glyph.Cat(text + i, 1);
			builder.DrawText(Pointf(x + advance, y), glyph, font, ToRgba8(ink));
			advance += dx[i];
			report.text_count++;
		}
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

bool RecordCtrlDisplayList(Ctrl& ctrl, UiDisplayList& out, String& error,
                           CtrlDisplayListRecordReport *report)
{
	error.Clear();
	CtrlDisplayListRecordReport local_report;
	CtrlDisplayListRecordReport& result = report ? *report : local_report;
	result = CtrlDisplayListRecordReport();

	UiDisplayListBuilder builder;
	CtrlDisplayListDraw draw(builder, ctrl.GetSize(), result);
	if(!draw.Record(ctrl)) {
		error = draw.GetError();
		return false;
	}
	if(draw.GetCloffLevel() != 0) {
		error = "U++ control recording left an unbalanced Draw state stack";
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
}

}
