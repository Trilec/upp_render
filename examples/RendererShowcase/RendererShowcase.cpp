#include "RendererShowcase.h"

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

static void DrawSoftwareError(Draw& w, Size size, const String& error)
{
	w.DrawRect(size, Color(248, 248, 250));
	w.DrawText(DPI(14), DPI(14), error.IsEmpty() ? String("Software preview unavailable") : error,
	           SansSerif(DPI(13)), Color(180, 50, 50));
}

} // namespace

void RendererSoftwarePreview::Paint(Draw& w)
{
	const Size size = GetSize();
	if(size.cx <= 0 || size.cy <= 0)
		return;
	if(!WhenBuildFrame) {
		DrawSoftwareError(w, size, "No display-list source attached");
		return;
	}

	UiDisplayList list;
	Rgba8 background;
	String error;
	if(!WhenBuildFrame(size, list, background, error) || !list.IsValid()) {
		if(error.IsEmpty())
			error = list.GetError();
		DrawSoftwareError(w, size, error);
		return;
	}

	ImagePainter painter(size);
	painter.DrawRect(Rect(0, 0, size.cx, size.cy),
	                 Color(background.r, background.g, background.b));
	SoftwareUiRenderer renderer;
	if(!renderer.Replay(list, painter)) {
		DrawSoftwareError(w, size, renderer.GetError());
		return;
	}
	w.DrawImage(0, 0, painter.GetResult());
}

RendererShowcase::RendererShowcase()
{
	Title("Renderer Showcase");
	Sizeable().Zoomable();
	SetRect(0, 0, DPI(1240), DPI(780));

	UiThemeContext context = UiTheme::GetContext();
	context.preset = UiThemePreset::Minimal;
	context.mode = UiThemeMode::Light;
	UiTheme::Set(context);

	demo_image = BuildDemoImage();
	BuildHeader();
	BuildInspector();

	Add(preview_panel);
	preview_panel.Add(preview_stack);
	preview_panel.Add(preview_caption);
	preview_stack.Add(gpu_preview, "gpu");
	preview_stack.Add(software_preview, "software");
	preview_caption.SetAlign(UiAlign::CENTER, UiAlign::CENTER);

	gpu_preview.SetValidation(true);
	gpu_preview.WhenBuildFrame = [=](Size size, UiDisplayList& list, Rgba8& background, String& error) {
		return BuildScene(size, list, background, error);
	};
	software_preview.WhenBuildFrame = [=](Size size, UiDisplayList& list, Rgba8& background, String& error) {
		return BuildScene(size, list, background, error);
	};

	Wire();
	ApplyProjection();
}

void RendererShowcase::BuildHeader()
{
	Add(header);
	header.SetTitle("Renderer Showcase")
	      .SetSubTitle("One neutral display list · Software reference and Vulkan preview")
	      .ShowTitleLine(true)
	      .SetContentInset(DPI(8))
	      .SetContentCell(header_actions);

	header_actions.SetGap(DPI(5)).SetInset(0).SetAlignItems(UiCrossAlign::Center);
	status.SetText("Preparing preview...");
	btn_gpu.SetText("GPU");
	btn_software.SetText("Software");
	btn_reset.SetText("Reset");
	btn_exit.SetText("Exit");
	header_actions.Add(status).Expand(1);
	header_actions.Add(btn_gpu).Fixed(DPI(74));
	header_actions.Add(btn_software).Fixed(DPI(88));
	header_actions.Add(btn_reset).Fixed(DPI(72));
	header_actions.Add(btn_exit).Fixed(DPI(64));
}

void RendererShowcase::BuildInspector()
{
	Add(inspector_panel);
	inspector_panel.Add(inspector_title);
	inspector_panel.Add(inspector_subtitle);
	inspector_panel.Add(inspector);
	inspector_title.SetText("Scene controls").SetAlign(UiAlign::LEFT, UiAlign::CENTER);
	inspector_subtitle.SetText("Small live edits against the same recorded scene")
	                  .SetAlign(UiAlign::LEFT, UiAlign::CENTER);

	model.AddChoice("renderer", "Preview", "GPU", "Renderer")
	     .AddChoice("GPU", "Vulkan GPU")
	     .AddChoice("Software", "Software reference")
	     .SetDefault("GPU");

	model.AddText("text", "Text", "GPU UI Rendering", "Content")
	     .SetDefault("GPU UI Rendering");
	model.AddNumericInt("font_size", "Text size", 24, 12, 48, 1, "Content")
	     .SetDefault(24).SetUnit("px");
	model.AddBoolean("show_image", "Show image", true, "Content").SetDefault(true);
	model.AddBoolean("show_svg", "Show SVG", true, "Content").SetDefault(true);

	model.AddColor("accent", "Accent", Color(72, 132, 238), "Appearance")
	     .SetDefault(Color(72, 132, 238));
	model.AddNumericInt("opacity", "Opacity", 205, 40, 255, 1, "Appearance")
	     .SetDefault(205);
	model.AddNumericInt("radius", "Corner radius", 24, 0, 64, 1, "Appearance")
	     .SetDefault(24).SetUnit("px");

	model.AddNumericDouble("scale", "Scale", 1.0, 0.65, 1.35, 0.05, "Geometry")
	     .SetDefault(1.0);
	model.AddNumericDouble("rotation", "Rotation", -8.0, -30.0, 30.0, 1.0, "Geometry")
	     .SetDefault(-8.0).SetUnit("deg");
	model.AddBoolean("clip", "Clip transformed group", true, "Geometry").SetDefault(true);

	model.SetGroupSubtitle("Renderer", "switch the exact same display list between reference and GPU");
	model.SetGroupSubtitle("Content", "text, sampled image and SVG content");
	model.SetGroupSubtitle("Appearance", "colour, alpha and rounded geometry");
	model.SetGroupSubtitle("Geometry", "affine transform and clipping state");
	model.StructureChanged();
	inspector.SetModel(&model);
}

void RendererShowcase::Wire()
{
	btn_gpu.WhenAction = [=] { SetPreviewMode("GPU"); };
	btn_software.WhenAction = [=] { SetPreviewMode("Software"); };
	btn_reset.WhenAction = [=] { ResetProperties(); };
	btn_exit.WhenAction = [=] { Close(); };

	auto changed = [=](String, Value) { ApplyProjection(); };
	inspector.WhenPreview = changed;
	inspector.WhenCommit = changed;
	inspector.WhenReset = [=](String id) {
		model.Reset(id);
		ApplyProjection();
	};
}

Value RendererShowcase::PropertyValue(const String& id) const
{
	const PropertyEditorItem *item = model.Find(id);
	return item ? item->value : Value();
}

void RendererShowcase::SetPreviewMode(const String& mode)
{
	model.SetValue("renderer", mode);
	ApplyProjection();
}

void RendererShowcase::ResetProperties()
{
	static const char *ids[] = {
		"renderer", "text", "font_size", "show_image", "show_svg",
		"accent", "opacity", "radius", "scale", "rotation", "clip"
	};
	for(const char *id : ids)
		model.Reset(id);
	ApplyProjection();
}

void RendererShowcase::ApplyProjection()
{
	const String mode = AsString(PropertyValue("renderer"));
	const bool gpu = mode != "Software";
	preview_stack.SetActivePage(gpu ? 0 : 1);
	status.SetText(gpu ? "Vulkan · live neutral scene" : "Software · reference replay");
	preview_caption.SetText(gpu
		? "Vulkan preview — fills · strokes · clipping · transforms · text · image · vector · SVG"
		: "Software reference — the exact same immutable display list");
	if(gpu)
		gpu_preview.RequestGpuRefresh();
	else
		software_preview.Refresh();
	preview_panel.Refresh();
}

Image RendererShowcase::BuildDemoImage() const
{
	ImageDraw draw(72, 72);
	draw.DrawRect(0, 0, 72, 72, Color(31, 39, 54));
	draw.DrawEllipse(8, 8, 56, 56, Color(249, 180, 60));
	draw.DrawEllipse(20, 20, 32, 32, Color(72, 132, 238));
	draw.DrawLine(12, 56, 60, 16, 4, Color(245, 247, 252));
	return draw;
}

bool RendererShowcase::BuildScene(Size size, UiDisplayList& list,
	                              Rgba8& background, String& error) const
{
	error.Clear();
	background = Rgba8(17, 24, 36, 255);
	UiDisplayListBuilder builder;
	if(size.cx <= 0 || size.cy <= 0)
		return builder.Finish(list);

	const double w = size.cx;
	const double h = size.cy;
	const double unit = max(1.0, min(w, h));
	const Color accent_color(PropertyValue("accent"));
	const int opacity = minmax((int)PropertyValue("opacity"), 0, 255);
	const double radius = max(0.0, (double)(int)PropertyValue("radius"));
	const double scale = (double)PropertyValue("scale");
	const double angle = (double)PropertyValue("rotation") * 3.14159265358979323846 / 180.0;
	const int font_size = max(8, (int)PropertyValue("font_size"));
	const bool show_image = (bool)PropertyValue("show_image");
	const bool show_svg = (bool)PropertyValue("show_svg");
	const bool clip = (bool)PropertyValue("clip");
	const WString text = AsString(PropertyValue("text")).ToWString();

	const Rgba8 accent = ToRgba(accent_color, opacity);
	const Rgba8 accent_soft = ToRgba(accent_color, max(50, opacity / 2));
	const Rgba8 paper(236, 241, 248, 255);
	const Rgba8 white(250, 252, 255, 245);

	builder.FillRect(Rectf(DPI(14), DPI(14), w - DPI(14), h - DPI(14)),
	                 Rgba8(27, 37, 52, 255));
	builder.StrokeRect(Rectf(DPI(20), DPI(20), w - DPI(20), h - DPI(20)),
	                   max(1.0, unit * 0.004), Rgba8(108, 128, 158, 210));

	// Left composition deliberately combines clip + affine state with ordinary
	// primitives, rounded geometry, sampled image and text.
	builder.Save();
	if(clip)
		builder.ClipRect(Rectf(w * 0.055, h * 0.18, w * 0.57, h * 0.82));
	Transform2D transform;
	transform.x.x = cos(angle) * scale;
	transform.x.y = sin(angle) * scale;
	transform.y.x = -sin(angle) * scale;
	transform.y.y = cos(angle) * scale;
	transform.t = Pointf(w * 0.31, h * 0.49);
	builder.ConcatTransform(transform);
	builder.FillRect(Rectf(-w * 0.22, -h * 0.20, w * 0.19, h * 0.17), accent_soft);
	builder.StrokeRect(Rectf(-w * 0.19, -h * 0.17, w * 0.18, h * 0.15),
	                   max(2.0, unit * 0.007), white);
	struct RoundedRect card(Rectf(-w * 0.16, -h * 0.125, w * 0.17, h * 0.12),
	                        min(radius, min(w * 0.08, h * 0.08)));
	builder.FillRoundedRect(card, accent);
	if(show_image)
		builder.DrawImage(Rectf(w * 0.055, -h * 0.085, w * 0.145, h * 0.055), demo_image);
	builder.DrawText(Pointf(-w * 0.135, -h * 0.018), text,
	                 SansSerif(DPI(font_size)).Bold(), paper);
	builder.Restore();

	// Right composition exercises the U++ Painter vector authority transported
	// through the accepted sampled-image GPU path.
	UiPath blob;
	blob.MoveTo(Pointf(w * 0.62, h * 0.23));
	blob.CubicTo(Pointf(w * 0.78, h * 0.12), Pointf(w * 0.94, h * 0.22), Pointf(w * 0.90, h * 0.42));
	blob.QuadraticTo(Pointf(w * 0.86, h * 0.58), Pointf(w * 0.68, h * 0.54));
	blob.CubicTo(Pointf(w * 0.58, h * 0.47), Pointf(w * 0.56, h * 0.32), Pointf(w * 0.62, h * 0.23));
	blob.Close();
	UiPaint gradient = UiPaint::Linear(Pointf(w * 0.60, h * 0.22), Pointf(w * 0.91, h * 0.53),
	                                   ToRgba(accent_color, 245), Rgba8(239, 91, 118, 230),
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
	if(show_svg)
		builder.DrawSvg(Rectf(w * 0.73, h * 0.61, w * 0.86, h * 0.80), SampleSvg());

	builder.DrawText(Pointf(w * 0.055, h - DPI(30)),
	                 WString("Same scene · Software / Vulkan"),
	                 SansSerif(DPI(13)), Rgba8(184, 197, 216, 255));

	if(!builder.Finish(list)) {
		error = builder.GetError();
		return false;
	}
	return true;
}

void RendererShowcase::Layout()
{
	Rect client = GetSize();
	const int pad = DPI(12);
	const int gap = DPI(10);
	const int header_h = DPI(76);
	const int right_w = min(DPI(390), max(DPI(320), client.GetWidth() * 31 / 100));

	header.SetRect(pad, pad, max(0, client.GetWidth() - 2 * pad), header_h);
	const int top = pad + header_h + gap;
	const int content_h = max(0, client.GetHeight() - top - pad);
	const int left_w = max(0, client.GetWidth() - 2 * pad - gap - right_w);
	preview_panel.SetRect(pad, top, left_w, content_h);
	inspector_panel.SetRect(pad + left_w + gap, top, right_w, content_h);

	Rect preview = preview_panel.GetSize();
	preview_stack.SetRect(DPI(8), DPI(8), max(0, preview.GetWidth() - DPI(16)),
	                      max(0, preview.GetHeight() - DPI(48)));
	preview_caption.SetRect(DPI(12), max(0, preview.GetHeight() - DPI(36)),
	                        max(0, preview.GetWidth() - DPI(24)), DPI(26));

	Rect rail = inspector_panel.GetSize();
	inspector_title.SetRect(DPI(10), DPI(8), max(0, rail.GetWidth() - DPI(20)), DPI(24));
	inspector_subtitle.SetRect(DPI(10), DPI(30), max(0, rail.GetWidth() - DPI(20)), DPI(28));
	inspector.SetRect(DPI(6), DPI(64), max(0, rail.GetWidth() - DPI(12)),
	                  max(0, rail.GetHeight() - DPI(70)));
}

} // namespace Upp
