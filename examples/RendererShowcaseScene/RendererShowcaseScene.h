#pragma once

#include <RenderCanvas/RenderCanvas.h>

namespace Upp {

struct RendererShowcaseSettings {
	String text = "GPU UI Rendering";
	Color accent = Color(72, 132, 238);
	int opacity = 205;
	int radius = 24;
	int font_size = 24;
	double scale = 1.0;
	double rotation_degrees = -8.0;
	bool clip = true;
	bool show_image = true;
	bool show_svg = true;
};

Image BuildRendererShowcaseDemoImage();

bool BuildRendererShowcaseScene(Size size,
	                            const RendererShowcaseSettings& settings,
	                            const Image& demo_image,
	                            UiDisplayList& list,
	                            Rgba8& background,
	                            String& error);

}
