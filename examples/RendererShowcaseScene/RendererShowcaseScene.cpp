#include "RendererShowcaseScene.h"

#include <cmath>

namespace Upp {
namespace {

static Rgba8 ToRgba(Color color, int alpha = 255)
{
	return Rgba8((byte)color.GetR(), (byte)color.GetG(), (byte)color.GetB(),
	             (byte)minmax(alpha, 0, 255));
}

static String SampleSvg()
{
	return "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'>"
	       "<path d='M8 48 L28 10 L56 48 Z' fill='#56a8e8'/>"
	       "<circle cx='32' cy='39' r='8' fill='#ffffff'/>"
	       "</svg>";
}

} // namespace

Image BuildRendererShowcaseDemoImage()
{
	ImageDraw draw(72, 72);
	draw.DrawRect(0, 0, 72, 72, Color(31, 39, 54));
	draw.DrawEllipse(8, 8, 56, 56, Color(249, 180, 60));
	draw.DrawEllipse(20, 20, 32, 32, Color(72, 132, 238));
	draw.DrawLine(12, 56, 60, 16, 4, Color(245, 247, 252));
	return draw;
}

bool BuildRendererShowcaseScene(Size size,
	                            const RendererShowcaseSettings& settings,
	                            const Image& demo_image,
	                            UiDisplayList& list,
	                            Rgba8& background,
	                            String& error)
{
	error.Clear();
	background = Rgba8(17, 24, 36, 255);
	UiDisplayListBuilder builder;
	if(size.cx <= 0 || size.cy <= 0)
		return builder.Finish(list);

	const double w = size.cx;
	const double h = size.cy;
	const double unit = max(1.0, min(w, h));
	const int opacity = minmax(settings.opacity, 0, 255);
	const double radius = max(0.0, (double)settings.radius);
	const double scale = minmax(settings.scale, 0.1, 8.0);
	const double angle = settings.rotation_degrees * 3.14159265358979323846 / 180.0;
	const int font_size = max(8, settings.font_size);
	const WString text = settings.text.ToWString();

	const Rgba8 accent = ToRgba(settings.accent, opacity);
	const Rgba8 accent_soft = ToRgba(settings.accent, max(50, opacity / 2));
	const Rgba8 paper(236, 241, 248, 255);
	const Rgba8 white(250, 252, 255, 245);

	builder.FillRect(Rectf(14, 14, w - 14, h - 14), Rgba8(27, 37, 52, 255));
	builder.StrokeRect(Rectf(20, 20, w - 20, h - 20),
	                   max(1.0, unit * 0.004), Rgba8(108, 128, 158, 210));

	// Left composition intentionally combines clip + affine state with ordinary
	// primitives, rounded geometry, sampled image and text.
	builder.Save();
	if(settings.clip)
		builder.ClipRect(Rectf(w * 0.055, h * 0.18, w * 0.57, h * 0.82));
	Transform2D transform;
	transform.x.x = std::cos(angle) * scale;
	transform.x.y = std::sin(angle) * scale;
	transform.y.x = -std::sin(angle) * scale;
	transform.y.y = std::cos(angle) * scale;
	transform.t = Pointf(w * 0.31, h * 0.49);
	builder.ConcatTransform(transform);
	builder.FillRect(Rectf(-w * 0.22, -h * 0.20, w * 0.19, h * 0.17), accent_soft);
	builder.StrokeRect(Rectf(-w * 0.19, -h * 0.17, w * 0.18, h * 0.15),
	                   max(2.0, unit * 0.007), white);
	struct RoundedRect card(Rectf(-w * 0.16, -h * 0.125, w * 0.17, h * 0.12),
	                        min(radius, min(w * 0.08, h * 0.08)));
	builder.FillRoundedRect(card, accent);
	if(settings.show_image && !demo_image.IsEmpty())
		builder.DrawImage(Rectf(w * 0.055, -h * 0.085, w * 0.145, h * 0.055), demo_image);
	builder.DrawText(Pointf(-w * 0.135, -h * 0.018), text,
	                 SansSerif(font_size).Bold(), paper);
	builder.Restore();

	// Right composition exercises U++ Painter vector semantics transported by
	// the accepted sampled-image GPU path.
	UiPath blob;
	blob.MoveTo(Pointf(w * 0.62, h * 0.23));
	blob.CubicTo(Pointf(w * 0.78, h * 0.12), Pointf(w * 0.94, h * 0.22), Pointf(w * 0.90, h * 0.42));
	blob.QuadraticTo(Pointf(w * 0.86, h * 0.58), Pointf(w * 0.68, h * 0.54));
	blob.CubicTo(Pointf(w * 0.58, h * 0.47), Pointf(w * 0.56, h * 0.32), Pointf(w * 0.62, h * 0.23));
	blob.Close();
	UiPaint gradient = UiPaint::Linear(Pointf(w * 0.60, h * 0.22), Pointf(w * 0.91, h * 0.53),
	                                   ToRgba(settings.accent, 245), Rgba8(239, 91, 118, 230),
	                                   UiGradientSpread::Reflect);
	gradient.AddStop(0.52, Rgba8(104, 218, 176, 235));
	builder.FillPath(blob, gradient, UiFillRule::NonZero);
	UiStrokeStyle stroke;
	stroke.width = max(2.0, unit * 0.006);
	stroke.cap = UiLineCap::Round;
	stroke.join = UiLineJoin::Round;
	stroke.dash << max(4.0, unit * 0.018) << max(3.0, unit * 0.010);
	builder.StrokePath(blob, UiPaint::Solid(white), stroke);

	builder.FillRect(Rectf(w * 0.63, h * 0.58, w * 0.93, h * 0.84), accent_soft);
	if(settings.show_svg)
		builder.DrawSvg(Rectf(w * 0.73, h * 0.61, w * 0.86, h * 0.80), SampleSvg());

	builder.DrawText(Pointf(w * 0.055, h - 30),
	                 WString("Same scene · Software / Vulkan"),
	                 SansSerif(13), Rgba8(184, 197, 216, 255));

	if(!builder.Finish(list)) {
		error = builder.GetError();
		return false;
	}
	return true;
}

} // namespace Upp
