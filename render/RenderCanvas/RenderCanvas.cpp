#include "RenderCanvas.h"

#include <cmath>

namespace Upp {

static String StableDouble(double value)
{
	if(fabs(value) < 0.0000005)
		return "0";

	String s = FormatDoubleFix(value, 6);
	int dot = s.Find('.');
	if(dot >= 0) {
		while(s.GetCount() > dot + 1 && s[s.GetCount() - 1] == '0')
			s.Trim(s.GetCount() - 1);
		if(s.GetCount() > 0 && s[s.GetCount() - 1] == '.')
			s.Trim(s.GetCount() - 1);
	}
	return s;
}

static bool IsFinitePoint(const Pointf& p)
{
	return std::isfinite(p.x) && std::isfinite(p.y);
}

static String DumpColor(Rgba8 color)
{
	return "rgba(" + AsString((int)color.r) + "," + AsString((int)color.g) + "," +
	       AsString((int)color.b) + "," + AsString((int)color.a) + ")";
}

static String DumpRect(const Rectf& rect)
{
	return StableDouble(rect.left) + " " + StableDouble(rect.top) + " " +
	       StableDouble(rect.right) + " " + StableDouble(rect.bottom);
}

static String DumpPoint(const Pointf& point)
{
	return StableDouble(point.x) + " " + StableDouble(point.y);
}

static String DumpTransform(const Transform2D& transform)
{
	return StableDouble(transform.x.x) + " " + StableDouble(transform.x.y) + " " +
	       StableDouble(transform.y.x) + " " + StableDouble(transform.y.y) + " " +
	       StableDouble(transform.t.x) + " " + StableDouble(transform.t.y);
}

static int64 StableBytesHash(const byte *data, int count)
{
	uint64 hash = 1469598103934665603ULL;
	for(int i = 0; i < count; ++i) {
		hash ^= data[i];
		hash *= 1099511628211ULL;
	}
	return (int64)hash;
}

static int64 StableImageHash(const Image& image)
{
	uint64 hash = 1469598103934665603ULL;
	auto mix = [&](byte value) {
		hash ^= value;
		hash *= 1099511628211ULL;
	};
	const Size size = image.GetSize();
	for(int shift = 0; shift < 32; shift += 8) {
		mix((byte)((uint32)size.cx >> shift));
		mix((byte)((uint32)size.cy >> shift));
	}
	const RGBA *pixels = image.Begin();
	for(size_t i = 0; pixels && i < image.GetLength(); ++i) {
		mix(pixels[i].r);
		mix(pixels[i].g);
		mix(pixels[i].b);
		mix(pixels[i].a);
	}
	return (int64)hash;
}

static int64 StableTextHash(const WString& text)
{
	uint64 hash = 1469598103934665603ULL;
	auto mix = [&](byte value) {
		hash ^= value;
		hash *= 1099511628211ULL;
	};
	for(int i = 0; i < text.GetCount(); ++i) {
		uint32 value = (uint32)text[i];
		mix((byte)(value));
		mix((byte)(value >> 8));
		mix((byte)(value >> 16));
		mix((byte)(value >> 24));
	}
	return (int64)hash;
}

static const char *DumpFillRule(UiFillRule rule)
{
	return rule == UiFillRule::EvenOdd ? "EvenOdd" : "NonZero";
}

static const char *DumpSpread(UiGradientSpread spread)
{
	switch(spread) {
	case UiGradientSpread::Pad: return "Pad";
	case UiGradientSpread::Repeat: return "Repeat";
	case UiGradientSpread::Reflect: return "Reflect";
	}
	return "Pad";
}

static const char *DumpLineCap(UiLineCap cap)
{
	switch(cap) {
	case UiLineCap::Butt: return "Butt";
	case UiLineCap::Square: return "Square";
	case UiLineCap::Round: return "Round";
	}
	return "Butt";
}

static const char *DumpLineJoin(UiLineJoin join)
{
	switch(join) {
	case UiLineJoin::Miter: return "Miter";
	case UiLineJoin::Round: return "Round";
	case UiLineJoin::Bevel: return "Bevel";
	}
	return "Miter";
}

bool UiPathCommand::operator==(const UiPathCommand& other) const
{
	return verb == other.verb && p1 == other.p1 && p2 == other.p2 && p3 == other.p3;
}

UiPath& UiPath::MoveTo(Pointf p)
{
	UiPathCommand& command = commands.Add();
	command.verb = UiPathVerb::MoveTo;
	command.p1 = p;
	return *this;
}

UiPath& UiPath::LineTo(Pointf p)
{
	UiPathCommand& command = commands.Add();
	command.verb = UiPathVerb::LineTo;
	command.p1 = p;
	return *this;
}

UiPath& UiPath::QuadraticTo(Pointf control, Pointf end)
{
	UiPathCommand& command = commands.Add();
	command.verb = UiPathVerb::QuadraticTo;
	command.p1 = control;
	command.p2 = end;
	return *this;
}

UiPath& UiPath::CubicTo(Pointf control1, Pointf control2, Pointf end)
{
	UiPathCommand& command = commands.Add();
	command.verb = UiPathVerb::CubicTo;
	command.p1 = control1;
	command.p2 = control2;
	command.p3 = end;
	return *this;
}

UiPath& UiPath::Close()
{
	UiPathCommand& command = commands.Add();
	command.verb = UiPathVerb::Close;
	return *this;
}

Rectf UiPath::GetControlBounds() const
{
	bool have = false;
	double left = 0, top = 0, right = 0, bottom = 0;
	auto add = [&](const Pointf& p) {
		if(!have) {
			left = right = p.x;
			top = bottom = p.y;
			have = true;
		}
		else {
			left = min(left, p.x);
			top = min(top, p.y);
			right = max(right, p.x);
			bottom = max(bottom, p.y);
		}
	};
	for(const UiPathCommand& command : commands) {
		switch(command.verb) {
		case UiPathVerb::MoveTo:
		case UiPathVerb::LineTo:
			add(command.p1);
			break;
		case UiPathVerb::QuadraticTo:
			add(command.p1);
			add(command.p2);
			break;
		case UiPathVerb::CubicTo:
			add(command.p1);
			add(command.p2);
			add(command.p3);
			break;
		case UiPathVerb::Close:
			break;
		}
	}
	return have ? Rectf(left, top, right, bottom) : Rectf(0, 0, 0, 0);
}

String UiPath::Dump() const
{
	String out;
	for(int i = 0; i < commands.GetCount(); ++i) {
		if(i)
			out << ' ';
		const UiPathCommand& command = commands[i];
		switch(command.verb) {
		case UiPathVerb::MoveTo:
			out << "M " << DumpPoint(command.p1);
			break;
		case UiPathVerb::LineTo:
			out << "L " << DumpPoint(command.p1);
			break;
		case UiPathVerb::QuadraticTo:
			out << "Q " << DumpPoint(command.p1) << ' ' << DumpPoint(command.p2);
			break;
		case UiPathVerb::CubicTo:
			out << "C " << DumpPoint(command.p1) << ' ' << DumpPoint(command.p2) << ' '
			    << DumpPoint(command.p3);
			break;
		case UiPathVerb::Close:
			out << 'Z';
			break;
		}
	}
	return out;
}

bool UiPath::operator==(const UiPath& other) const
{
	return commands == other.commands;
}

bool UiGradientStop::operator==(const UiGradientStop& other) const
{
	return position == other.position && color == other.color;
}

UiPaint UiPaint::Solid(Rgba8 c)
{
	UiPaint paint;
	paint.kind = UiPaintKind::Solid;
	paint.color = c;
	return paint;
}

UiPaint UiPaint::Linear(Pointf start, Pointf end, Rgba8 start_color, Rgba8 end_color,
                        UiGradientSpread spread_mode)
{
	UiPaint paint;
	paint.kind = UiPaintKind::LinearGradient;
	paint.p0 = start;
	paint.p1 = end;
	paint.spread = spread_mode;
	paint.stops.Add(UiGradientStop(0.0, start_color));
	paint.stops.Add(UiGradientStop(1.0, end_color));
	return paint;
}

UiPaint UiPaint::Radial(Pointf focal, Pointf centre, double r,
                        Rgba8 inner_color, Rgba8 outer_color,
                        UiGradientSpread spread_mode)
{
	UiPaint paint;
	paint.kind = UiPaintKind::RadialGradient;
	paint.p0 = focal;
	paint.p1 = centre;
	paint.radius = r;
	paint.spread = spread_mode;
	paint.stops.Add(UiGradientStop(0.0, inner_color));
	paint.stops.Add(UiGradientStop(1.0, outer_color));
	return paint;
}

UiPaint& UiPaint::AddStop(double position, Rgba8 stop_color)
{
	UiGradientStop stop(position, stop_color);
	int insert = stops.GetCount();
	for(int i = 0; i < stops.GetCount(); ++i)
		if(position < stops[i].position) {
			insert = i;
			break;
		}
	stops.Insert(insert, stop);
	return *this;
}

bool UiPaint::IsValid(String *reason) const
{
	auto fail = [&](const char *message) {
		if(reason)
			*reason = message;
		return false;
	};
	if(kind == UiPaintKind::Solid)
		return true;
	if(!IsFinitePoint(p0) || !IsFinitePoint(p1))
		return fail("gradient points must be finite");
	if(kind == UiPaintKind::LinearGradient && p0 == p1)
		return fail("linear gradient endpoints must differ");
	if(kind == UiPaintKind::RadialGradient && (!std::isfinite(radius) || radius <= 0.0))
		return fail("radial gradient radius must be positive");
	if(stops.GetCount() < 2)
		return fail("gradient requires at least two stops");
	for(int i = 0; i < stops.GetCount(); ++i) {
		if(!std::isfinite(stops[i].position) || stops[i].position < 0.0 || stops[i].position > 1.0)
			return fail("gradient stop is outside 0..1");
		if(i && stops[i].position < stops[i - 1].position)
			return fail("gradient stops must be ordered");
	}
	if(fabs(stops[0].position) > 0.0000005 || fabs(stops.Top().position - 1.0) > 0.0000005)
		return fail("gradient endpoints must include stops at 0 and 1");
	return true;
}

String UiPaint::Dump() const
{
	if(kind == UiPaintKind::Solid)
		return String("Solid ") + DumpColor(color);
	String out;
	out << (kind == UiPaintKind::LinearGradient ? "Linear " : "Radial ")
	    << DumpPoint(p0) << ' ' << DumpPoint(p1);
	if(kind == UiPaintKind::RadialGradient)
		out << " r=" << StableDouble(radius);
	out << " spread=" << DumpSpread(spread) << " stops=";
	for(int i = 0; i < stops.GetCount(); ++i) {
		if(i)
			out << ',';
		out << StableDouble(stops[i].position) << ':' << DumpColor(stops[i].color);
	}
	return out;
}

bool UiPaint::operator==(const UiPaint& other) const
{
	return kind == other.kind && color == other.color && p0 == other.p0 && p1 == other.p1 &&
	       radius == other.radius && spread == other.spread && stops == other.stops;
}

bool UiStrokeStyle::IsValid(String *reason) const
{
	auto fail = [&](const char *message) {
		if(reason)
			*reason = message;
		return false;
	};
	if(!std::isfinite(width) || width <= 0.0)
		return fail("stroke width must be positive");
	if(!std::isfinite(miter_limit) || miter_limit <= 0.0)
		return fail("stroke miter limit must be positive");
	if(!std::isfinite(dash_offset))
		return fail("stroke dash offset must be finite");
	for(double value : dash)
		if(!std::isfinite(value) || value <= 0.0)
			return fail("stroke dash values must be positive");
	return true;
}

String UiStrokeStyle::Dump() const
{
	String out;
	out << "width=" << StableDouble(width) << " cap=" << DumpLineCap(cap)
	    << " join=" << DumpLineJoin(join) << " miter=" << StableDouble(miter_limit);
	if(!dash.IsEmpty()) {
		out << " dash=";
		for(int i = 0; i < dash.GetCount(); ++i) {
			if(i)
				out << ',';
			out << StableDouble(dash[i]);
		}
		out << " offset=" << StableDouble(dash_offset);
	}
	return out;
}

bool UiStrokeStyle::operator==(const UiStrokeStyle& other) const
{
	return width == other.width && cap == other.cap && join == other.join &&
	       miter_limit == other.miter_limit && dash == other.dash && dash_offset == other.dash_offset;
}

bool UiDisplayOp::operator==(const UiDisplayOp& other) const
{
	return type == other.type && rect.left == other.rect.left && rect.top == other.rect.top &&
	       rect.right == other.rect.right && rect.bottom == other.rect.bottom &&
	       point == other.point && width == other.width && transform == other.transform &&
	       color == other.color && rounded == other.rounded && image == other.image &&
	       text == other.text && font == other.font && path == other.path && paint == other.paint &&
	       fill_rule == other.fill_rule && stroke == other.stroke && svg == other.svg;
}

UiDisplayList::UiDisplayList()
{
	valid = false;
	error = "display list not built";
}

void UiDisplayList::SetValid(Vector<UiDisplayOp>&& source)
{
	ops = pick(source);
	valid = true;
	error.Clear();
}

void UiDisplayList::SetInvalid(String message, Vector<UiDisplayOp>&& source)
{
	ops = pick(source);
	valid = false;
	error = pick(message);
}

String UiDisplayList::Dump() const
{
	String sb;
	for(int i = 0; i < ops.GetCount(); ++i) {
		const UiDisplayOp& op = ops[i];
		sb << i << ' ';
		switch(op.type) {
		case UiDisplayOpType::Save:
			sb << "Save";
			break;
		case UiDisplayOpType::Restore:
			sb << "Restore";
			break;
		case UiDisplayOpType::ClipRect:
			sb << "ClipRect " << DumpRect(op.rect);
			break;
		case UiDisplayOpType::ConcatTransform:
			sb << "ConcatTransform " << DumpTransform(op.transform);
			break;
		case UiDisplayOpType::FillRect:
			sb << "FillRect " << DumpRect(op.rect) << ' ' << DumpColor(op.color);
			break;
		case UiDisplayOpType::InvertRect:
			sb << "InvertRect " << DumpRect(op.rect);
			break;
		case UiDisplayOpType::StrokeRect:
			sb << "StrokeRect " << DumpRect(op.rect) << ' ' << StableDouble(op.width) << ' '
			   << DumpColor(op.color);
			break;
		case UiDisplayOpType::FillRoundedRect:
			sb << "FillRoundedRect " << DumpRect(op.rounded.rect) << ' '
			   << StableDouble(op.rounded.radius) << ' ' << DumpColor(op.color);
			break;
		case UiDisplayOpType::DrawImage:
			sb << "DrawImage " << DumpRect(op.rect) << " image=" << op.image.GetWidth() << 'x'
			   << op.image.GetHeight() << " hash=" << StableImageHash(op.image);
			break;
		case UiDisplayOpType::DrawText:
			sb << "DrawText " << DumpPoint(op.point) << " chars=" << op.text.GetCount()
			   << " hash=" << StableTextHash(op.text) << " font=" << op.font.AsInt64() << ' '
			   << DumpColor(op.color);
			break;
		case UiDisplayOpType::FillPath:
			sb << "FillPath rule=" << DumpFillRule(op.fill_rule) << " path={" << op.path.Dump()
			   << "} paint={" << op.paint.Dump() << '}';
			break;
		case UiDisplayOpType::StrokePath:
			sb << "StrokePath path={" << op.path.Dump() << "} paint={" << op.paint.Dump()
			   << "} stroke={" << op.stroke.Dump() << '}';
			break;
		case UiDisplayOpType::DrawSvg:
			sb << "DrawSvg " << DumpRect(op.rect) << " bytes=" << op.svg.GetCount()
			   << " hash=" << StableBytesHash((const byte *)op.svg.Begin(), op.svg.GetCount());
			break;
		}
		if(i + 1 < ops.GetCount())
			sb << '\n';
	}
	if(!valid) {
		if(ops.GetCount())
			sb << '\n';
		sb << "INVALID " << error;
	}
	return sb;
}

UiDisplayListBuilder::UiDisplayListBuilder()
{
}

void UiDisplayListBuilder::Fail(const String& message)
{
	if(error.IsEmpty())
		error = message;
	finished = true;
}

bool UiDisplayListBuilder::CanRecord()
{
	if(finished) {
		if(error.IsEmpty())
			error = "builder already finished";
		return false;
	}
	return error.IsEmpty();
}

void UiDisplayListBuilder::Append(const UiDisplayOp& op)
{
	if(!CanRecord())
		return;
	ops.Add(op);
}

void UiDisplayListBuilder::Save()
{
	if(!CanRecord())
		return;
	UiDisplayOp op;
	op.type = UiDisplayOpType::Save;
	Append(op);
	++save_depth;
}

void UiDisplayListBuilder::Restore()
{
	if(!CanRecord())
		return;
	if(save_depth <= 0) {
		Fail("restore without matching save");
		return;
	}
	UiDisplayOp op;
	op.type = UiDisplayOpType::Restore;
	Append(op);
	--save_depth;
}

void UiDisplayListBuilder::ClipRect(const Rectf& rect)
{
	if(!CanRecord())
		return;
	UiDisplayOp op;
	op.type = UiDisplayOpType::ClipRect;
	op.rect = rect;
	Append(op);
}

void UiDisplayListBuilder::ConcatTransform(const Transform2D& transform)
{
	if(!CanRecord())
		return;
	UiDisplayOp op;
	op.type = UiDisplayOpType::ConcatTransform;
	op.transform = transform;
	Append(op);
}

void UiDisplayListBuilder::FillRect(const Rectf& rect, Rgba8 color)
{
	if(!CanRecord())
		return;
	UiDisplayOp op;
	op.type = UiDisplayOpType::FillRect;
	op.rect = rect;
	op.color = color;
	Append(op);
}

void UiDisplayListBuilder::InvertRect(const Rectf& rect)
{
	if(!CanRecord())
		return;
	UiDisplayOp op;
	op.type = UiDisplayOpType::InvertRect;
	op.rect = rect;
	Append(op);
}

void UiDisplayListBuilder::StrokeRect(const Rectf& rect, double width, Rgba8 color)
{
	if(!CanRecord())
		return;
	UiDisplayOp op;
	op.type = UiDisplayOpType::StrokeRect;
	op.rect = rect;
	op.width = width;
	op.color = color;
	Append(op);
}

void UiDisplayListBuilder::FillRoundedRect(const RoundedRect& rect, Rgba8 color)
{
	if(!CanRecord())
		return;
	UiDisplayOp op;
	op.type = UiDisplayOpType::FillRoundedRect;
	op.rounded = rect;
	op.color = color;
	Append(op);
}

void UiDisplayListBuilder::DrawImage(const Rectf& rect, const Image& image)
{
	if(!CanRecord())
		return;
	UiDisplayOp op;
	op.type = UiDisplayOpType::DrawImage;
	op.rect = rect;
	op.image = image;
	Append(op);
}

void UiDisplayListBuilder::DrawText(const Pointf& point, const WString& text, Font font, Rgba8 color)
{
	if(!CanRecord())
		return;
	UiDisplayOp op;
	op.type = UiDisplayOpType::DrawText;
	op.point = point;
	op.text = text;
	op.font = font;
	op.color = color;
	Append(op);
}

void UiDisplayListBuilder::FillPath(const UiPath& path, const UiPaint& paint, UiFillRule rule)
{
	if(!CanRecord())
		return;
	UiDisplayOp op;
	op.type = UiDisplayOpType::FillPath;
	op.path = path;
	op.paint = paint;
	op.fill_rule = rule;
	Append(op);
}

void UiDisplayListBuilder::StrokePath(const UiPath& path, const UiPaint& paint,
                                      const UiStrokeStyle& style)
{
	if(!CanRecord())
		return;
	UiDisplayOp op;
	op.type = UiDisplayOpType::StrokePath;
	op.path = path;
	op.paint = paint;
	op.stroke = style;
	Append(op);
}

void UiDisplayListBuilder::DrawSvg(const Rectf& rect, const String& svg)
{
	if(!CanRecord())
		return;
	UiDisplayOp op;
	op.type = UiDisplayOpType::DrawSvg;
	op.rect = rect;
	op.svg = svg;
	Append(op);
}

bool UiDisplayListBuilder::Finish(UiDisplayList& out)
{
	if(finished && error.IsEmpty() && save_depth == 0) {
		out.SetInvalid("builder already finished", Vector<UiDisplayOp>());
		return false;
	}
	if(save_depth != 0 && error.IsEmpty())
		Fail("unbalanced save depth at finish");
	finished = true;
	if(error.IsEmpty()) {
		out.SetValid(pick(ops));
		return true;
	}
	out.SetInvalid(error, pick(ops));
	return false;
}

}
