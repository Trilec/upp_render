#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderNull/RenderNull.h>

using namespace Upp;

static bool Check(bool condition, const char *message)
{
	if(!condition)
		Cout() << "FAIL: " << message << EOL;
	return condition;
}

static int CountText(const String& text, const char *needle)
{
	int count = 0;
	int pos = 0;
	for(;;) {
		pos = text.Find(needle, pos);
		if(pos < 0)
			return count;
		++count;
		pos += String(needle).GetCount();
	}
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

static bool BuildScene(UiDisplayList& out)
{
	UiDisplayListBuilder builder;
	builder.FillRect(Rectf(4, 4, 30, 24), Rgba8(35, 80, 150, 255));
	builder.Save();
	builder.ClipRect(Rectf(12, 6, 118, 60));
	Transform2D affine;
	affine.x.x = 1.0;
	affine.x.y = 0.08;
	affine.y.x = -0.05;
	affine.y.y = 1.0;
	affine.t = Pointf(4, 1);
	builder.ConcatTransform(affine);
	builder.DrawText(Pointf(16, 14), TestText(), SansSerif(22).Bold(), Rgba8(225, 240, 255, 210));
	builder.Restore();
	builder.FillRect(Rectf(92, 48, 126, 72), Rgba8(220, 105, 40, 190));
	return builder.Finish(out);
}

static GpuTextureId CreateTarget(NullGpuDevice& device, Size size)
{
	GpuTextureDesc desc;
	desc.size = size;
	desc.format = GpuFormat::RGBA8;
	desc.usage = GpuTextureUsage_ColorAttachment;
	desc.label = "RenderGpuTextTest target";
	GpuTextureId target;
	if(device.CreateTexture(desc, target) != GpuResult::Ok)
		return GpuTextureId();
	return target;
}

static UiRenderer2DTarget MakeTarget(GpuTextureId target, Size size)
{
	UiRenderer2DTarget out;
	out.color_target = target;
	out.size = size;
	out.color_format = GpuFormat::RGBA8;
	out.load_op = GpuLoadOp::Clear;
	out.store_op = GpuStoreOp::Store;
	out.clear_color.alpha = 1.0f;
	return out;
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	NullGpuDevice device;
	const Size size(128, 76);
	GpuTextureId target = CreateTarget(device, size);
	ok &= Check(target.IsValid(), "text target should create");

	UiDisplayList scene;
	ok &= Check(BuildScene(scene), "text GPU scene should build");
	const String dump = scene.Dump();

	{
		UiRenderer2D renderer(device);
		ok &= Check(renderer.IsReady(), "UiRenderer2D should be ready for text on RenderNull");
		ok &= Check(renderer.Render(scene, MakeTarget(target, size)), "first text GPU frame should render");
		const UiRenderer2DStats first = renderer.GetStats();
		ok &= Check(first.text_run_count == 1 && first.glyph_count == 4,
		            "renderer should account for one four-character text run");
		ok &= Check(first.glyph_cache_miss_count == 2,
		            "ABBA should rasterize only two distinct glyphs on first use");
		ok &= Check(first.glyph_atlas_page_count == 1,
		            "small text run should fit in one persistent atlas page");
		ok &= Check(first.glyph_atlas_upload_count == 3,
		            "first text frame should initialize one atlas page and upload two glyph regions");
		ok &= Check(first.textured_vertex_count > 0 && first.textured_vertex_count % 3 == 0,
		            "glyphs should emit sampled triangle geometry");
		ok &= Check(first.translucent_vertex_count > 0,
		            "text color alpha should survive into textured vertices");
		ok &= Check(first.batch_count == 3 && first.draw_count == 3,
		            "solid/text/solid order should be preserved as three batches");
		ok &= Check(first.textured_vertex_buffer_grew && first.textured_vertex_buffer_capacity > 0,
		            "first text frame should allocate the reusable textured vertex buffer");
		ok &= Check(scene.Dump() == dump, "GPU text replay must not mutate the immutable display list");

		ok &= Check(renderer.Render(scene, MakeTarget(target, size)), "second text GPU frame should render");
		const UiRenderer2DStats second = renderer.GetStats();
		ok &= Check(second.glyph_cache_miss_count == 0 && second.glyph_atlas_upload_count == 0,
		            "second text frame should reuse the persistent glyph atlas without raster/upload work");
		ok &= Check(second.glyph_atlas_page_count == 1 && second.glyph_count == 4,
		            "second frame should retain one atlas page and four placed glyphs");
		ok &= Check(second.batch_count == 3 && second.draw_count == 3,
		            "text batching should remain deterministic across frames");
		ok &= Check(!second.textured_vertex_buffer_grew &&
		            second.textured_vertex_buffer_capacity == first.textured_vertex_buffer_capacity,
		            "second text frame should reuse the textured vertex buffer");
		ok &= Check(scene.Dump() == dump, "repeated GPU text replay must preserve display-list immutability");

		renderer.Close();
		ok &= Check(device.GetLivePipelineCount() == 0 && device.GetLiveShaderCount() == 0 &&
		            device.GetLiveBufferCount() == 0,
		            "text renderer Close should release pipelines, shaders and buffers");
	}

	String log = device.DumpLog();
	ok &= Check(CountText(log, "CreateTexture id=") == 2,
	            "text test should create only target plus one atlas page texture");
	ok &= Check(CountText(log, "WriteTexture id=") == 3,
	            "glyph atlas should have one initialization and two partial glyph uploads");
	ok &= Check(CountText(log, "SetSampledTexture list=") == 2,
	            "each text frame should bind the atlas once");
	ok &= Check(CountText(log, "Draw list=") == 6,
	            "two solid/text/solid frames should issue six ordered draws");
	ok &= Check(device.DestroyTexture(target) == GpuResult::Ok,
	            "text target should destroy after renderer-owned atlas shutdown");
	ok &= Check(device.GetLiveTextureCount() == 0,
	            "text test should finish with zero RenderNull textures");

	if(ok) {
		Cout() << "RenderGpuTextTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
