#include "RenderSoftware.h"

namespace Upp {

static Rect ToRect(const Rectf& r)
{
	return Rect((int)r.left, (int)r.top, (int)r.right, (int)r.bottom);
}

static RGBA ToRgba(const Rgba8& c)
{
	RGBA rgba;
	rgba.r = c.r;
	rgba.g = c.g;
	rgba.b = c.b;
	rgba.a = c.a;
	return rgba;
}

static Xform2D ToXform(const Transform2D& m)
{
	Xform2D x;
	x.x = m.x;
	x.y = m.y;
	x.t = m.t;
	return x;
}

bool SoftwareUiRenderer::Replay(const UiDisplayList& list, Painter& painter)
{
	error.Clear();
	if(!list.IsValid()) {
		error = list.GetError();
		return false;
	}

	struct StateGuard {
		Painter& painter;
		int& depth;
		bool active = true;
		StateGuard(Painter& p, int& d) : painter(p), depth(d) {
			painter.Begin();
			++depth;
		}
		~StateGuard() {
			if(active) {
				while(depth > 0) {
					painter.End();
					--depth;
				}
			}
		}
	};

	int depth = 0;
	Vector<int> save_targets;
	StateGuard guard(painter, depth);

	for(int i = 0; i < list.GetCount(); ++i) {
		const UiDisplayOp& op = list[i];
		switch(op.type) {
		case UiDisplayOpType::Save:
			save_targets.Add(depth);
			painter.Begin();
			++depth;
			break;
		case UiDisplayOpType::Restore:
			if(save_targets.IsEmpty()) {
				error = "restore without matching save during replay";
				return false;
			}
			{
				int target = save_targets.Pop();
				while(depth > target + 1) {
					painter.End();
					--depth;
				}
				painter.End();
				--depth;
			}
			break;
		case UiDisplayOpType::ClipRect:
			painter.Draw::Clip(ToRect(op.rect));
			break;
		case UiDisplayOpType::ConcatTransform:
			painter.Transform(ToXform(op.transform));
			break;
		case UiDisplayOpType::FillRect:
			painter.DrawRect(ToRect(op.rect), op.color.ToColor());
			break;
		case UiDisplayOpType::StrokeRect:
			painter.Rectangle(op.rect);
			painter.Stroke(op.width, ToRgba(op.color));
			break;
		case UiDisplayOpType::FillRoundedRect:
			painter.RoundedRectangle(op.rounded.rect, op.rounded.radius);
			painter.Fill(ToRgba(op.color));
			break;
		}
	}

	if(depth != 1) {
		error = "replay ended with unbalanced painter state";
		return false;
	}
	painter.End();
	--depth;

	guard.active = false;
	return true;
}

}
