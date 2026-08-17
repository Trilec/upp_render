#include <Painter/Painter.h>
#include <RenderSoftware/RenderSoftware.h>

using namespace Upp;

static bool Check(bool condition, const char *message)
{
	if(!condition)
		Cout() << "FAIL: " << message << EOL;
	return condition;
}

static RGBA MakeRgba(byte r, byte g, byte b, byte a = 255)
{
	RGBA color;
	color.r = r;
	color.g = g;
	color.b = b;
	color.a = a;
	return color;
}

static bool ImageDiffersFrom(const Image& image, RGBA reference)
{
	for(int y = 0; y < image.GetHeight(); ++y)
		for(int x = 0; x < image.GetWidth(); ++x)
			if(image[y][x] != reference)
				return true;
	return false;
}

static WString TestText()
{
	WString text;
	text.Cat('A');
	text.Cat('B');
	text.Cat('B');
	text.Cat('A');
	return text;
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	const WString text = TestText();
	const Font font = SansSerif(22).Bold();

	UiDisplayListBuilder builder;
	builder.Save();
	builder.ClipRect(Rectf(0, 0, 96, 48));
	builder.DrawText(Pointf(8, 9), text, font, Rgba8(220, 235, 250, 210));
	builder.Restore();
	UiDisplayList list;
	ok &= Check(builder.Finish(list), "text display list should finish");
	ok &= Check(list.IsValid() && list.GetCount() == 4,
	            "text display list should contain Save/Clip/DrawText/Restore");
	if(list.GetCount() == 4) {
		ok &= Check(list[2].type == UiDisplayOpType::DrawText, "third operation should be DrawText");
		ok &= Check(list[2].point == Pointf(8, 9), "recorded text position should be preserved");
		ok &= Check(list[2].text == text, "recorded WString should be preserved");
		ok &= Check(list[2].font == font, "recorded Font value should be preserved");
		ok &= Check(list[2].color == Rgba8(220, 235, 250, 210), "recorded text color should be preserved");
	}

	const String dump = list.Dump();
	ok &= Check(dump.Find("DrawText 8 9 chars=4 hash=") >= 0,
	            "display-list dump should contain deterministic text metadata");
	ok &= Check(dump.Find(" font=") >= 0 && dump.Find("rgba(220,235,250,210)") >= 0,
	            "text dump should include font value and color");
	ok &= Check(dump.Find("0x") < 0, "text dump should not contain pointer-like values");
	ok &= Check(dump == list.Dump(), "repeated text dump should be deterministic");

	UiDisplayListBuilder equivalent_builder;
	equivalent_builder.Save();
	equivalent_builder.ClipRect(Rectf(0, 0, 96, 48));
	equivalent_builder.DrawText(Pointf(8, 9), TestText(), SansSerif(22).Bold(), Rgba8(220, 235, 250, 210));
	equivalent_builder.Restore();
	UiDisplayList equivalent;
	ok &= Check(equivalent_builder.Finish(equivalent), "equivalent text display list should finish");
	ok &= Check(equivalent.Dump() == dump, "equivalent text values should produce the same deterministic dump");

	const RGBA background = MakeRgba(7, 10, 13, 255);
	ImagePainter painter(Size(104, 56));
	painter.DrawRect(Rect(0, 0, 104, 56), Color(7, 10, 13));
	SoftwareUiRenderer software;
	ok &= Check(software.Replay(list, painter), "software reference should replay DrawText through U++ Painter");
	ok &= Check(software.GetError().IsEmpty(), "software DrawText replay should retain no error");
	Image output = painter.GetResult();
	ok &= Check(ImageDiffersFrom(output, background), "software text replay should produce visible output");
	ok &= Check(list.Dump() == dump, "software text replay must not mutate the immutable display list");

	UiDisplayListBuilder empty_builder;
	empty_builder.DrawText(Pointf(4, 4), WString(), SansSerif(18), Rgba8(255, 255, 255));
	UiDisplayList empty;
	ok &= Check(empty_builder.Finish(empty), "empty text should remain a valid no-op command");
	ImagePainter empty_painter(Size(24, 24));
	empty_painter.DrawRect(Rect(0, 0, 24, 24), Color(2, 3, 4));
	ok &= Check(software.Replay(empty, empty_painter), "software replay should accept empty text");

	if(ok) {
		Cout() << "RenderTextTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
