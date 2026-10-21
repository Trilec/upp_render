#include "RenderVector.h"

#include <cmath>

namespace Upp {

namespace {

static RGBA ToRgba(const Rgba8& c)
{
	RGBA rgba;
	rgba.r = ((int)c.r * c.a + 127) / 255;
	rgba.g = ((int)c.g * c.a + 127) / 255;
	rgba.b = ((int)c.b * c.a + 127) / 255;
	rgba.a = c.a;
	return rgba;
}

static bool IsFinitePoint(const Pointf& p)
{
	return std::isfinite(p.x) && std::isfinite(p.y);
}

static bool IsFiniteRect(const Rectf& r)
{
	return std::isfinite(r.left) && std::isfinite(r.top) &&
	       std::isfinite(r.right) && std::isfinite(r.bottom);
}

static int PainterSpread(UiGradientSpread spread)
{
	switch(spread) {
	case UiGradientSpread::Pad: return GRADIENT_PAD;
	case UiGradientSpread::Repeat: return GRADIENT_REPEAT;
	case UiGradientSpread::Reflect: return GRADIENT_REFLECT;
	}
	return GRADIENT_PAD;
}

static int PainterCap(UiLineCap cap)
{
	switch(cap) {
	case UiLineCap::Butt: return LINECAP_BUTT;
	case UiLineCap::Square: return LINECAP_SQUARE;
	case UiLineCap::Round: return LINECAP_ROUND;
	}
	return LINECAP_BUTT;
}

static int PainterJoin(UiLineJoin join)
{
	switch(join) {
	case UiLineJoin::Miter: return LINEJOIN_MITER;
	case UiLineJoin::Round: return LINEJOIN_ROUND;
	case UiLineJoin::Bevel: return LINEJOIN_BEVEL;
	}
	return LINEJOIN_MITER;
}

static Pointf MapPoint(Pointf p, double scale, Pointf offset)
{
	return Pointf(p.x * scale + offset.x, p.y * scale + offset.y);
}

static bool ReplayPath(Painter& painter, const UiPath& path, double scale, Pointf offset,
                       String& error)
{
	if(path.IsEmpty()) {
		error = "vector path is empty";
		return false;
	}
	bool have_current = false;
	for(int i = 0; i < path.GetCount(); ++i) {
		const UiPathCommand& command = path[i];
		switch(command.verb) {
		case UiPathVerb::MoveTo:
			if(!IsFinitePoint(command.p1)) { error = "path MoveTo is non-finite"; return false; }
			painter.Move(MapPoint(command.p1, scale, offset));
			have_current = true;
			break;
		case UiPathVerb::LineTo:
			if(!have_current) { error = "path LineTo before MoveTo"; return false; }
			if(!IsFinitePoint(command.p1)) { error = "path LineTo is non-finite"; return false; }
			painter.Line(MapPoint(command.p1, scale, offset));
			break;
		case UiPathVerb::QuadraticTo:
			if(!have_current) { error = "path QuadraticTo before MoveTo"; return false; }
			if(!IsFinitePoint(command.p1) || !IsFinitePoint(command.p2)) {
				error = "path QuadraticTo is non-finite";
				return false;
			}
			painter.Quadratic(MapPoint(command.p1, scale, offset),
			                  MapPoint(command.p2, scale, offset));
			break;
		case UiPathVerb::CubicTo:
			if(!have_current) { error = "path CubicTo before MoveTo"; return false; }
			if(!IsFinitePoint(command.p1) || !IsFinitePoint(command.p2) || !IsFinitePoint(command.p3)) {
				error = "path CubicTo is non-finite";
				return false;
			}
			painter.Cubic(MapPoint(command.p1, scale, offset),
			              MapPoint(command.p2, scale, offset),
			              MapPoint(command.p3, scale, offset));
			break;
		case UiPathVerb::Close:
			if(!have_current) { error = "path Close before MoveTo"; return false; }
			painter.Close();
			break;
		}
	}
	return true;
}

static void AddIntermediateStops(Painter& painter, const UiPaint& paint)
{
	painter.ClearStops();
	for(int i = 1; i + 1 < paint.stops.GetCount(); ++i)
		painter.ColorStop(paint.stops[i].position, ToRgba(paint.stops[i].color));
}

static bool FillPaint(Painter& painter, const UiPaint& paint, double scale, Pointf offset,
                      String& error)
{
	String reason;
	if(!paint.IsValid(&reason)) {
		error = "invalid vector fill paint: " + reason;
		return false;
	}
	if(paint.kind == UiPaintKind::Solid) {
		painter.Fill(ToRgba(paint.color));
		return true;
	}
	AddIntermediateStops(painter, paint);
	const UiGradientStop& first = paint.stops[0];
	const UiGradientStop& last = paint.stops.Top();
	Xform2D matrix;
	if(paint.kind == UiPaintKind::LinearGradient) {
		Pointf d = paint.p1 - paint.p0;
		matrix.x = Pointf(d.x, -d.y); matrix.y = Pointf(d.y, d.x); matrix.t = paint.p0;
	}
	else { matrix = Xform2D::Scale(paint.radius); matrix.t = paint.p1; }
	Xform2D local; local.x = paint.transform.x; local.y = paint.transform.y; local.t = paint.transform.t;
	matrix = matrix * local * Xform2D::Scale(scale) * Xform2D::Translation(offset.x, offset.y);
	if(paint.kind == UiPaintKind::LinearGradient)
		painter.Fill(ToRgba(first.color), ToRgba(last.color), matrix, PainterSpread(paint.spread));
	else
		painter.Fill((paint.p0 - paint.p1) / paint.radius, ToRgba(first.color),
		             ToRgba(last.color), matrix, PainterSpread(paint.spread));
	painter.ClearStops();
	return true;
}

static bool StrokePaint(Painter& painter, const UiPaint& paint, double width,
                        double scale, Pointf offset, String& error)
{
	String reason;
	if(!paint.IsValid(&reason)) {
		error = "invalid vector stroke paint: " + reason;
		return false;
	}
	if(paint.kind == UiPaintKind::Solid) {
		painter.Stroke(width * scale, ToRgba(paint.color));
		return true;
	}
	AddIntermediateStops(painter, paint);
	const UiGradientStop& first = paint.stops[0];
	const UiGradientStop& last = paint.stops.Top();
	Xform2D matrix;
	if(paint.kind == UiPaintKind::LinearGradient) {
		Pointf d = paint.p1 - paint.p0;
		matrix.x = Pointf(d.x, -d.y); matrix.y = Pointf(d.y, d.x); matrix.t = paint.p0;
	}
	else { matrix = Xform2D::Scale(paint.radius); matrix.t = paint.p1; }
	Xform2D local; local.x = paint.transform.x; local.y = paint.transform.y; local.t = paint.transform.t;
	matrix = matrix * local * Xform2D::Scale(scale) * Xform2D::Translation(offset.x, offset.y);
	if(paint.kind == UiPaintKind::LinearGradient)
		painter.Stroke(width * scale, ToRgba(first.color), ToRgba(last.color), matrix, PainterSpread(paint.spread));
	else
		painter.Stroke(width * scale, (paint.p0 - paint.p1) / paint.radius, ToRgba(first.color),
		             ToRgba(last.color), matrix, PainterSpread(paint.spread));
	painter.ClearStops();
	return true;
}

static bool SafeRasterSize(const Rectf& rect, double scale, Size& out)
{
	if(!IsFiniteRect(rect) || rect.right <= rect.left || rect.bottom <= rect.top ||
	   !std::isfinite(scale) || scale <= 0.0)
		return false;
	const double w = ceil((rect.right - rect.left) * scale);
	const double h = ceil((rect.bottom - rect.top) * scale);
	if(w < 1.0 || h < 1.0 || w > 4096.0 || h > 4096.0)
		return false;
	out = Size((int)w, (int)h);
	return true;
}

}

static bool ReplayAngularPaint(Painter& painter, const UiDisplayOp& op, double scale,
                               Pointf offset, String& error)
{
	const UiPaint& paint=op.paint;
	Rectf bounds=op.path.GetControlBounds();
	double pad=2/scale;
	if(op.type==UiDisplayOpType::StrokePath) pad+=op.stroke.width*max(1.0,op.stroke.miter_limit);
	bounds.Inflate(pad,pad);
	Size size;
	if(!paint.IsValid() || !SafeRasterSize(bounds,scale,size)) { error="invalid angular paint bounds"; return false; }
	ImageBuffer image(size); const auto& m=paint.transform;
	double det=m.x.x*m.y.y-m.x.y*m.y.x, tau=2*M_PI, sweep=min(fabs(paint.p1.y),tau);
	for(int y=0;y<size.cy;++y) for(int x=0;x<size.cx;++x) {
		Pointf d(bounds.left+(x+.5)/scale-m.t.x,bounds.top+(y+.5)/scale-m.t.y);
		Pointf q((m.y.y*d.x-m.x.y*d.y)/det,(-m.y.x*d.x+m.x.x*d.y)/det); q-=paint.p0;
		double phase=fmod((paint.p1.y<0?-1:1)*(atan2(q.y,q.x)-paint.p1.x),tau);
		if(phase<0) phase+=tau;
		double t=phase/sweep;
		if(sweep<tau-1e-6 && phase>sweep) {
			double delta=fabs(phase-sweep);
			t=min(phase,tau-phase)<=min(delta,tau-delta)?0:1;
		}
		t=minmax(t,0.0,1.0);
		int stop=1; while(stop+1<paint.stops.GetCount() && t>paint.stops[stop].position) ++stop;
		const auto& a=paint.stops[stop-1]; const auto& b=paint.stops[stop];
		double u=b.position>a.position?(t-a.position)/(b.position-a.position):1;
		auto mix=[&](int aa,int bb)->byte { return (byte)minmax((int)round(aa*(1-u)+bb*u),0,255); };
		image[y][x]=ToRgba(Rgba8(mix(a.color.r,b.color.r),mix(a.color.g,b.color.g),
		                         mix(a.color.b,b.color.b),mix(a.color.a,b.color.a)));
	}
	Xform2D placement=Xform2D::Translation(bounds.left*scale+offset.x,bounds.top*scale+offset.y);
	if(op.type==UiDisplayOpType::FillPath) painter.Fill(Image(image),placement);
	else painter.Stroke(op.stroke.width*scale,Image(image),placement);
	return true;
}

bool ReplayUiPath(Painter& painter, const UiPath& path, String& error)
{
	return ReplayPath(painter,path,1.0,Pointf(0,0),error);
}

bool ReplayUiVectorOp(Painter& painter, const UiDisplayOp& op, String& error,
                      double scale, Pointf offset)
{
	error.Clear();
	if(!std::isfinite(scale) || scale <= 0.0 || !IsFinitePoint(offset)) {
		error = "vector replay scale/offset is invalid";
		return false;
	}

	if(op.type == UiDisplayOpType::DrawSvg) {
		if(op.svg.IsEmpty())
			return true;
		if(!IsFiniteRect(op.rect) || op.rect.right <= op.rect.left || op.rect.bottom <= op.rect.top) {
			error = "SVG target rectangle is invalid";
			return false;
		}
		Size raster;
		if(!SafeRasterSize(op.rect, scale, raster)) {
			error = "SVG raster size is invalid or exceeds 4096 pixels";
			return false;
		}
		Image image = RenderSVGImage(raster, op.svg);
		if(image.IsEmpty()) {
			error = "U++ RenderSVGImage failed";
			return false;
		}
		Rect target((int)floor(op.rect.left * scale + offset.x),
		            (int)floor(op.rect.top * scale + offset.y),
		            (int)ceil(op.rect.right * scale + offset.x),
		            (int)ceil(op.rect.bottom * scale + offset.y));
		painter.DrawImage(target, image);
		return true;
	}

	if(op.type != UiDisplayOpType::FillPath && op.type != UiDisplayOpType::StrokePath) {
		error = "operation is not vector content";
		return false;
	}

	painter.Begin();
	bool ok = true;
	if(op.type == UiDisplayOpType::FillPath) {
		painter.EvenOdd(op.fill_rule == UiFillRule::EvenOdd);
		ok = ReplayPath(painter, op.path, scale, offset, error);
		if(ok)
			ok = op.paint.kind==UiPaintKind::AngularGradient ? ReplayAngularPaint(painter,op,scale,offset,error) :
			     FillPaint(painter, op.paint, scale, offset, error);
	}
	else {
		String reason;
		if(!op.stroke.IsValid(&reason)) {
			error = "invalid vector stroke style: " + reason;
			ok = false;
		}
		if(ok) {
			painter.LineCap(PainterCap(op.stroke.cap));
			painter.LineJoin(PainterJoin(op.stroke.join));
			painter.MiterLimit(op.stroke.miter_limit);
			if(!op.stroke.dash.IsEmpty()) {
				Vector<double> scaled;
				scaled.Reserve(op.stroke.dash.GetCount());
				for(double value : op.stroke.dash)
					scaled.Add(value * scale);
				painter.Dash(scaled, op.stroke.dash_offset * scale);
			}
			ok = ReplayPath(painter, op.path, scale, offset, error);
		}
		if(ok)
			ok = op.paint.kind==UiPaintKind::AngularGradient ? ReplayAngularPaint(painter,op,scale,offset,error) :
			     StrokePaint(painter, op.paint, op.stroke.width, scale, offset, error);
	}
	painter.End();
	return ok;
}

bool RasterizeUiVectorOp(const UiDisplayOp& op, double scale, Image& out,
                         Rectf& local_rect, String& error)
{
	out = Image();
	local_rect = Rectf(0, 0, 0, 0);
	error.Clear();
	if(!std::isfinite(scale) || scale <= 0.0) {
		error = "vector raster scale must be positive";
		return false;
	}
	scale = minmax(scale, 1.0, 8.0);

	if(op.type == UiDisplayOpType::DrawSvg) {
		if(op.svg.IsEmpty())
			return true;
		local_rect = op.rect;
		Size raster;
		if(!SafeRasterSize(local_rect, scale, raster)) {
			error = "SVG raster size is invalid or exceeds 4096 pixels";
			return false;
		}
		out = RenderSVGImage(raster, op.svg);
		if(out.IsEmpty()) {
			error = "U++ RenderSVGImage failed";
			return false;
		}
		return true;
	}

	if(op.type != UiDisplayOpType::FillPath && op.type != UiDisplayOpType::StrokePath) {
		error = "operation is not rasterizable vector content";
		return false;
	}
	if(op.path.IsEmpty()) {
		error = "vector path is empty";
		return false;
	}
	String reason;
	if(!op.paint.IsValid(&reason)) {
		error = "invalid vector paint: " + reason;
		return false;
	}
	if(op.type == UiDisplayOpType::StrokePath && !op.stroke.IsValid(&reason)) {
		error = "invalid vector stroke style: " + reason;
		return false;
	}

	Rectf bounds = op.path.GetControlBounds();
	if(!IsFiniteRect(bounds)) {
		error = "vector path bounds are non-finite";
		return false;
	}
	double extent = 0.0;
	if(op.type == UiDisplayOpType::StrokePath) {
		extent = op.stroke.width * 0.5;
		if(op.stroke.join == UiLineJoin::Miter)
			extent *= max(1.0, op.stroke.miter_limit);
	}
	const double aa_padding = 2.0 / scale;
	const double padding = extent + aa_padding;
	local_rect = Rectf(bounds.left - padding, bounds.top - padding,
	                   bounds.right + padding, bounds.bottom + padding);
	if(local_rect.right <= local_rect.left)
		local_rect.right = local_rect.left + max(1.0 / scale, aa_padding);
	if(local_rect.bottom <= local_rect.top)
		local_rect.bottom = local_rect.top + max(1.0 / scale, aa_padding);

	Size raster;
	if(!SafeRasterSize(local_rect, scale, raster)) {
		error = "vector raster size is invalid or exceeds 4096 pixels";
		return false;
	}

	ImagePainter painter(raster, MODE_ANTIALIASED);
	painter.Clear(RGBAZero());
	Pointf offset(-local_rect.left * scale, -local_rect.top * scale);
	if(!ReplayUiVectorOp(painter, op, error, scale, offset))
		return false;
	out = painter.GetResult();
	return !out.IsEmpty();
}

}
