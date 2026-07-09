#include "RenderCanvas.h"

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

static String DumpTransform(const Transform2D& transform)
{
	return StableDouble(transform.x.x) + " " + StableDouble(transform.x.y) + " " +
	       StableDouble(transform.y.x) + " " + StableDouble(transform.y.y) + " " +
	       StableDouble(transform.t.x) + " " + StableDouble(transform.t.y);
}

bool UiDisplayOp::operator==(const UiDisplayOp& other) const
{
	return type == other.type && rect.left == other.rect.left && rect.top == other.rect.top &&
	       rect.right == other.rect.right && rect.bottom == other.rect.bottom &&
	       width == other.width && transform == other.transform && color == other.color &&
	       rounded == other.rounded;
}

UiDisplayList::UiDisplayList()
{
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
		case UiDisplayOpType::StrokeRect:
			sb << "StrokeRect " << DumpRect(op.rect) << ' ' << StableDouble(op.width) << ' '
			   << DumpColor(op.color);
			break;
		case UiDisplayOpType::FillRoundedRect:
			sb << "FillRoundedRect " << DumpRect(op.rounded.rect) << ' '
			   << StableDouble(op.rounded.radius) << ' ' << DumpColor(op.color);
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
