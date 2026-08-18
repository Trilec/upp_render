#include "RendererShowcase.h"

namespace Upp {
namespace {

static void DrawSoftwareError(Draw& w, Size size, const String& error)
{
	w.DrawRect(0, 0, size.cx, size.cy, Color(248, 248, 250));
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

	demo_image = BuildRendererShowcaseDemoImage();
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

RendererShowcaseSettings RendererShowcase::GetSettings() const
{
	RendererShowcaseSettings settings;
	settings.text = AsString(PropertyValue("text"));
	settings.font_size = max(8, (int)PropertyValue("font_size"));
	settings.show_image = (bool)PropertyValue("show_image");
	settings.show_svg = (bool)PropertyValue("show_svg");
	settings.accent = Color(PropertyValue("accent"));
	settings.opacity = minmax((int)PropertyValue("opacity"), 0, 255);
	settings.radius = max(0, (int)PropertyValue("radius"));
	settings.scale = (double)PropertyValue("scale");
	settings.rotation_degrees = (double)PropertyValue("rotation");
	settings.clip = (bool)PropertyValue("clip");
	return settings;
}

bool RendererShowcase::BuildScene(Size size, UiDisplayList& list,
	                              Rgba8& background, String& error) const
{
	return BuildRendererShowcaseScene(size, GetSettings(), demo_image, list, background, error);
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
