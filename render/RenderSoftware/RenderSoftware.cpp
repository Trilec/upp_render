#include "RenderSoftware.h"

#include <RenderVector/RenderVector.h>

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

namespace {
struct ShapedOutlinePainter : FontGlyphConsumer {
	Painter& painter; Pointf origin; bool used=false;
	ShapedOutlinePainter(Painter& painter,Pointf origin):painter(painter),origin(origin) {}
	void Move(Pointf p) override { painter.Move(p+origin); used=true; }
	void Line(Pointf p) override { painter.Line(p+origin); }
	void Quadratic(Pointf a,Pointf b) override { painter.Quadratic(a+origin,b+origin); }
	void Cubic(Pointf a,Pointf b,Pointf c) override { painter.Cubic(a+origin,b+origin,c+origin); }
	void Close() override { painter.Close(); }
};
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
		case UiDisplayOpType::BlurLayer:
			error="neutral blur layers require GPU composition; native software Ui uses its existing helper"; return false;
		case UiDisplayOpType::ClipPath:
			painter.EvenOdd(op.fill_rule == UiFillRule::EvenOdd);
			if(!ReplayUiPath(painter,op.path,error)) return false;
			painter.Clip(); break;
		case UiDisplayOpType::ClipRect:
			// Neutral ClipRect mutates this state; Draw::Clip would push an extra state.
			painter.Rectangle(op.rect).Clip();
			break;
		case UiDisplayOpType::ConcatTransform:
			painter.Transform(ToXform(op.transform));
			break;
		case UiDisplayOpType::FillRect:
			painter.DrawRect(ToRect(op.rect), op.color.ToColor());
			break;
		case UiDisplayOpType::InvertRect:
			painter.DrawRect(ToRect(op.rect), InvertColor());
			break;
		case UiDisplayOpType::StrokeRect:
			painter.Rectangle(op.rect);
			painter.Stroke(op.width, ToRgba(op.color));
			break;
		case UiDisplayOpType::FillRoundedRect:
			painter.RoundedRectangle(op.rounded.rect, op.rounded.radius);
			painter.Fill(ToRgba(op.color));
			break;
		case UiDisplayOpType::DrawImage:
			if(!op.image.IsEmpty() && op.image_tint.a) {
				Image resolved = Crop(op.image, op.image_source);
				if(op.image_tint != Rgba8(255, 255, 255, 255) || op.image_alpha_mask) {
					ImageBuffer pixels(resolved);
					for(RGBA& p : pixels) {
						const int a = p.a;
						p.r = ((op.image_alpha_mask ? a : p.r) * op.image_tint.r + 127) / 255;
						p.g = ((op.image_alpha_mask ? a : p.g) * op.image_tint.g + 127) / 255;
						p.b = ((op.image_alpha_mask ? a : p.b) * op.image_tint.b + 127) / 255;
						p.r = (p.r * op.image_tint.a + 127) / 255;
						p.g = (p.g * op.image_tint.a + 127) / 255;
						p.b = (p.b * op.image_tint.a + 127) / 255;
						p.a = (a * op.image_tint.a + 127) / 255;
					}
					resolved = Image(pixels);
				}
				// Match GPU clamp-to-edge sampling instead of Painter's transparent extension.
				const Rect dest = ToRect(op.rect);
				painter.RectPath(dest).Fill(resolved,
					Xform2D::Scale((double)dest.GetWidth() / resolved.GetWidth(),
					               (double)dest.GetHeight() / resolved.GetHeight()) *
					Xform2D::Translation(dest.left, dest.top), FILL_PAD);
			}
			break;
		case UiDisplayOpType::DrawShapedText:
			if(!op.shaped_text || !op.shaped_text->IsValid(&error)) return false;
			painter.EvenOdd(false);
			for(const auto& run:op.shaped_text->runs) for(const auto& glyph:run.glyphs) {
				ShapedOutlinePainter sink(painter,op.point+glyph.origin);
				if(!run.face->GetOutline(glyph.glyph,run.em_size,sink,error)) return false;
				if(sink.used) painter.Fill(op.text_ink_override || IsNull(run.ink)?ToRgba(op.color):(RGBA)run.ink);
			}
			for(const auto& r:op.shaped_text->decorations) painter.Rectangle(r.rect+op.point).Fill(op.text_ink_override || IsNull(r.ink)?ToRgba(op.color):(RGBA)r.ink);
			break;
		case UiDisplayOpType::DrawText:
			if(!op.text.IsEmpty()) {
				painter.Begin();
				painter.Translate(op.point);
				// Match the neutral control contract and GPU glyph placement.
				// Painter's default Text spacing otherwise uses fractional large-font metrics.
				Vector<int> advances;
				for(int cp : op.text) advances.Add(op.font.GetWidth(cp));
				painter.DrawText(0, 0, op.text, op.font, op.color.ToColor(), advances.Begin());
				painter.End();
			}
			break;
		case UiDisplayOpType::FillPath:
		case UiDisplayOpType::StrokePath:
		case UiDisplayOpType::DrawSvg:
			if(!ReplayUiVectorOp(painter, op, error))
				return false;
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
