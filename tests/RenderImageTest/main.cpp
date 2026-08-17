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

static Image MakeTestImage()
{
	ImageBuffer buffer(2, 2);
	buffer[0][0] = MakeRgba(255, 0, 0);
	buffer[0][1] = MakeRgba(0, 255, 0);
	buffer[1][0] = MakeRgba(0, 0, 255);
	buffer[1][1] = MakeRgba(255, 255, 255, 128);
	return Image(buffer);
}

static bool ImageDiffersFrom(const Image& image, RGBA reference)
{
	for(int y = 0; y < image.GetHeight(); ++y)
		for(int x = 0; x < image.GetWidth(); ++x)
			if(image[y][x] != reference)
				return true;
	return false;
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	Image image = MakeTestImage();
	ok &= Check(image.GetSize() == Size(2, 2), "test image should be 2x2");

	UiDisplayListBuilder builder;
	builder.Save();
	builder.ClipRect(Rectf(0, 0, 24, 24));
	builder.DrawImage(Rectf(4, 5, 20, 21), image);
	builder.Restore();
	UiDisplayList list;
	ok &= Check(builder.Finish(list), "image display list should finish");
	ok &= Check(list.IsValid() && list.GetCount() == 4, "image display list should contain Save/Clip/DrawImage/Restore");
	if(list.GetCount() == 4) {
		ok &= Check(list[2].type == UiDisplayOpType::DrawImage, "third operation should be DrawImage");
		ok &= Check(list[2].image == image, "recorded image value should preserve pixel content");
		ok &= Check(list[2].rect == Rectf(4, 5, 20, 21), "recorded image destination should be preserved");
	}

	String dump = list.Dump();
	ok &= Check(dump.Find("DrawImage 4 5 20 21 image=2x2 hash=") >= 0,
	            "display-list dump should contain deterministic image metadata");
	ok &= Check(dump.Find("0x") < 0, "image dump should not contain pointer-like values");
	ok &= Check(dump == list.Dump(), "repeated image dump should be deterministic");

	UiDisplayListBuilder equivalent_builder;
	equivalent_builder.Save();
	equivalent_builder.ClipRect(Rectf(0, 0, 24, 24));
	equivalent_builder.DrawImage(Rectf(4, 5, 20, 21), MakeTestImage());
	equivalent_builder.Restore();
	UiDisplayList equivalent;
	ok &= Check(equivalent_builder.Finish(equivalent), "equivalent image display list should finish");
	ok &= Check(equivalent.Dump() == dump, "equivalent image pixels should produce the same deterministic dump");

	ImagePainter painter(Size(32, 32));
	painter.DrawRect(Rect(0, 0, 32, 32), Color(9, 11, 13));
	SoftwareUiRenderer software;
	ok &= Check(software.Replay(list, painter), "software reference should replay DrawImage");
	ok &= Check(software.GetError().IsEmpty(), "software DrawImage replay should retain no error");
	Image output = painter.GetResult();
	ok &= Check(ImageDiffersFrom(output, MakeRgba(9, 11, 13)), "software image replay should produce visible output");
	ok &= Check(list.Dump() == dump, "software replay must not mutate the immutable image display list");

	UiDisplayListBuilder empty_image_builder;
	empty_image_builder.DrawImage(Rectf(0, 0, 10, 10), Image());
	UiDisplayList empty_image_list;
	ok &= Check(empty_image_builder.Finish(empty_image_list), "empty image recording should remain a valid no-op command");
	ImagePainter empty_painter(Size(12, 12));
	empty_painter.DrawRect(Rect(0, 0, 12, 12), Color(3, 4, 5));
	ok &= Check(software.Replay(empty_image_list, empty_painter), "software replay should accept an empty image as a no-op");

	if(ok) {
		Cout() << "RenderImageTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
