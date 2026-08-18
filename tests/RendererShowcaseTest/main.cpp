#include <RendererShowcaseScene/RendererShowcaseScene.h>
#include <RenderSoftware/RenderSoftware.h>

using namespace Upp;

static bool Check(bool condition, const char *message)
{
	if(!condition)
		Cout() << "FAIL: " << message << EOL;
	return condition;
}

static int CountOps(const UiDisplayList& list, UiDisplayOpType type)
{
	int count = 0;
	for(int i = 0; i < list.GetCount(); ++i)
		if(list[i].type == type)
			++count;
	return count;
}

static bool ImageDiffersFrom(const Image& image, Rgba8 reference)
{
	for(int y = 0; y < image.GetHeight(); ++y)
		for(int x = 0; x < image.GetWidth(); ++x) {
			const RGBA& p = image[y][x];
			if(p.r != reference.r || p.g != reference.g || p.b != reference.b || p.a != reference.a)
				return true;
		}
	return false;
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	RendererShowcaseSettings settings;
	Image demo_image = BuildRendererShowcaseDemoImage();
	UiDisplayList list;
	Rgba8 background;
	String error;

	ok &= Check(BuildRendererShowcaseScene(Size(900, 540), settings, demo_image,
	                                      list, background, error),
	            "default showcase scene should build");
	ok &= Check(list.IsValid(), "default showcase display list should be valid");
	ok &= Check(error.IsEmpty(), "default showcase scene should not report an error");
	ok &= Check(CountOps(list, UiDisplayOpType::FillRect) >= 3,
	            "showcase should contain multiple filled rectangles");
	ok &= Check(CountOps(list, UiDisplayOpType::StrokeRect) >= 2,
	            "showcase should contain rectangle strokes");
	ok &= Check(CountOps(list, UiDisplayOpType::Save) == 1 &&
	            CountOps(list, UiDisplayOpType::Restore) == 1,
	            "showcase should exercise Save/Restore state");
	ok &= Check(CountOps(list, UiDisplayOpType::ClipRect) == 1,
	            "showcase should exercise clipping");
	ok &= Check(CountOps(list, UiDisplayOpType::ConcatTransform) == 1,
	            "showcase should exercise affine transforms");
	ok &= Check(CountOps(list, UiDisplayOpType::FillRoundedRect) == 1,
	            "showcase should contain rounded geometry");
	ok &= Check(CountOps(list, UiDisplayOpType::DrawImage) == 1,
	            "showcase should contain sampled image content");
	ok &= Check(CountOps(list, UiDisplayOpType::DrawText) >= 2,
	            "showcase should contain text content");
	ok &= Check(CountOps(list, UiDisplayOpType::FillPath) == 1 &&
	            CountOps(list, UiDisplayOpType::StrokePath) == 1,
	            "showcase should contain gradient vector fill and stroked path");
	ok &= Check(CountOps(list, UiDisplayOpType::DrawSvg) == 1,
	            "showcase should contain SVG content");

	const String dump = list.Dump();
	ok &= Check(dump == list.Dump(), "showcase display-list dump should be deterministic");
	ok &= Check(dump.Find("Linear") >= 0 && dump.Find("spread=Reflect") >= 0,
	            "showcase dump should preserve gradient evidence");
	ok &= Check(dump.Find("DrawText") >= 0 && dump.Find("DrawSvg") >= 0,
	            "showcase dump should expose text and SVG evidence");

	ImagePainter painter(Size(900, 540));
	painter.DrawRect(Rect(0, 0, 900, 540), Color(background.r, background.g, background.b));
	SoftwareUiRenderer software;
	ok &= Check(software.Replay(list, painter),
	            "software reference should replay the complete showcase scene");
	ok &= Check(software.GetError().IsEmpty(), "showcase software replay should remain error-free");
	Image output = painter.GetResult();
	ok &= Check(ImageDiffersFrom(output, background),
	            "showcase software replay should produce visible output");
	ok &= Check(list.Dump() == dump, "software replay must not mutate the showcase display list");

	RendererShowcaseSettings alternate = settings;
	alternate.text = "Interactive renderer";
	alternate.accent = Color(224, 86, 72);
	alternate.opacity = 128;
	alternate.radius = 42;
	alternate.scale = 1.2;
	alternate.rotation_degrees = 18;
	alternate.clip = false;
	alternate.show_image = false;
	alternate.show_svg = false;
	UiDisplayList alternate_list;
	Rgba8 alternate_background;
	ok &= Check(BuildRendererShowcaseScene(Size(900, 540), alternate, demo_image,
	                                      alternate_list, alternate_background, error),
	            "interactive showcase variant should build");
	ok &= Check(CountOps(alternate_list, UiDisplayOpType::ClipRect) == 0,
	            "clip toggle should remove the clip operation");
	ok &= Check(CountOps(alternate_list, UiDisplayOpType::DrawImage) == 0,
	            "image toggle should remove sampled image content");
	ok &= Check(CountOps(alternate_list, UiDisplayOpType::DrawSvg) == 0,
	            "SVG toggle should remove SVG content");
	ok &= Check(alternate_list.Dump() != dump,
	            "live property changes should materially change recorded scene intent");

	if(ok) {
		Cout() << "RendererShowcaseTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
