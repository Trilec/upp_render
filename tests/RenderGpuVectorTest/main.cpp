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

static UiPath MakePath()
{
	UiPath path;
	path.MoveTo(Pointf(10, 10));
	path.LineTo(Pointf(68, 8));
	path.CubicTo(Pointf(88, 14), Pointf(88, 44), Pointf(66, 50));
	path.LineTo(Pointf(12, 48));
	path.QuadraticTo(Pointf(2, 28), Pointf(10, 10));
	path.Close();
	path.MoveTo(Pointf(28, 22));
	path.LineTo(Pointf(52, 22));
	path.LineTo(Pointf(52, 36));
	path.LineTo(Pointf(28, 36));
	path.Close();
	return path;
}

static UiPaint MakeGradient()
{
	UiPaint paint = UiPaint::Radial(Pointf(28, 20), Pointf(42, 30), 38.0,
	                                Rgba8(70, 180, 245, 235),
	                                Rgba8(235, 75, 55, 180), UiGradientSpread::Pad);
	paint.AddStop(0.55, Rgba8(125, 225, 145, 210));
	return paint;
}

static UiStrokeStyle MakeStroke()
{
	UiStrokeStyle stroke;
	stroke.width = 2.5;
	stroke.cap = UiLineCap::Round;
	stroke.join = UiLineJoin::Bevel;
	stroke.dash << 6.0 << 3.0;
	stroke.dash_offset = 1.0;
	return stroke;
}

static String SampleSvg()
{
	return "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'>"
	       "<rect x='3' y='3' width='26' height='26' rx='7' fill='#6b79db'/>"
	       "<path d='M8 17 L14 23 L25 9' fill='none' stroke='#fff' stroke-width='3'/></svg>";
}

static WString Stage5Label()
{
	WString text;
	text.Cat('V');
	text.Cat('5');
	return text;
}

static bool BuildScene(UiDisplayList& out)
{
	UiDisplayListBuilder builder;
	builder.FillRect(Rectf(3, 3, 24, 20), Rgba8(25, 65, 135, 255));
	builder.Save();
	builder.ClipRect(Rectf(6, 5, 142, 74));
	Transform2D affine;
	affine.x.x = 1.35;
	affine.x.y = 0.12;
	affine.y.x = -0.08;
	affine.y.y = 1.15;
	affine.t = Pointf(5, 2);
	builder.ConcatTransform(affine);
	builder.FillPath(MakePath(), MakeGradient(), UiFillRule::EvenOdd);
	builder.StrokePath(MakePath(), UiPaint::Solid(Rgba8(245, 245, 250, 220)), MakeStroke());
	builder.DrawSvg(Rectf(82, 12, 112, 42), SampleSvg());
	builder.DrawText(Pointf(88, 46), Stage5Label(), SansSerif(14).Bold(), Rgba8(245, 248, 255, 230));
	builder.Restore();
	builder.FillRect(Rectf(118, 52, 150, 76), Rgba8(220, 105, 40, 200));
	return builder.Finish(out);
}

static GpuTextureId CreateTarget(NullGpuDevice& device, Size size)
{
	GpuTextureDesc desc;
	desc.size = size;
	desc.format = GpuFormat::RGBA8;
	desc.usage = GpuTextureUsage_ColorAttachment;
	desc.label = "RenderGpuVectorTest target";
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
	const Size size(160, 82);
	GpuTextureId target = CreateTarget(device, size);
	ok &= Check(target.IsValid(), "vector target should create");

	UiDisplayList scene;
	ok &= Check(BuildScene(scene), "GPU vector scene should build");
	const String dump = scene.Dump();

	{
		UiRenderer2D renderer(device);
		ok &= Check(renderer.IsReady(), "UiRenderer2D should be ready for vector content on RenderNull");
		ok &= Check(renderer.Render(scene, MakeTarget(target, size)), "first vector GPU frame should render");
		const UiRenderer2DStats first = renderer.GetStats();
		ok &= Check(first.vector_op_count == 3 && first.vector_path_count == 2 && first.svg_count == 1,
		            "renderer should account for two vector paths plus one SVG");
		ok &= Check(first.gradient_count == 1,
		            "renderer should identify the radial-gradient path");
		ok &= Check(first.vector_cache_miss_count == 3 && first.vector_raster_count == 3 &&
		            first.vector_cache_entry_count == 3,
		            "first frame should rasterize exactly three unique vector assets");
		ok &= Check(first.texture_upload_count == 3,
		            "first frame should reuse DrawImage and upload exactly three vector rasters");
		ok &= Check(first.image_count == 3 && first.text_run_count == 1 && first.glyph_count == 2,
		            "materialized vector assets and original text should share one renderer pass");
		ok &= Check(first.glyph_cache_miss_count == 2 && first.glyph_atlas_upload_count == 2 &&
		            first.glyph_atlas_page_count == 1,
		            "mixed Stage-5 frame should populate the accepted glyph atlas exactly once per glyph");
		ok &= Check(first.textured_vertex_count > 0,
		            "mixed vector/text scene should emit sampled geometry");
		ok &= Check(first.batch_count == 6 && first.draw_count == 6,
		            "solid/vector/vector/SVG/text/solid order should remain six batches and draws");
		ok &= Check(first.textured_vertex_buffer_grew && first.textured_vertex_buffer_capacity > 0,
		            "first mixed Stage-5 frame should allocate the reusable textured vertex buffer");
		ok &= Check(scene.Dump() == dump, "vector GPU replay must not mutate the source display list");

		ok &= Check(renderer.Render(scene, MakeTarget(target, size)), "second vector GPU frame should render from caches");
		const UiRenderer2DStats second = renderer.GetStats();
		ok &= Check(second.vector_cache_miss_count == 0 && second.vector_raster_count == 0,
		            "second frame should perform no U++ vector raster work");
		ok &= Check(second.vector_cache_entry_count == 3,
		            "second frame should retain three cached vector rasters");
		ok &= Check(second.texture_upload_count == 0,
		            "second frame should reuse the accepted GPU image texture cache");
		ok &= Check(second.glyph_cache_miss_count == 0 && second.glyph_atlas_upload_count == 0,
		            "second mixed frame should also reuse the glyph atlas");
		ok &= Check(second.batch_count == 6 && second.draw_count == 6,
		            "cached mixed Stage-5 rendering should preserve deterministic draw order");
		ok &= Check(!second.textured_vertex_buffer_grew &&
		            second.textured_vertex_buffer_capacity == first.textured_vertex_buffer_capacity,
		            "second mixed frame should reuse its textured vertex buffer");
		ok &= Check(scene.Dump() == dump, "cached vector replay must preserve display-list immutability");

		renderer.Close();
		ok &= Check(device.GetLivePipelineCount() == 0 && device.GetLiveShaderCount() == 0 &&
		            device.GetLiveBufferCount() == 0,
		            "vector renderer Close should release pipelines, shaders and buffers");
		ok &= Check(device.GetLiveTextureCount() == 1,
		            "renderer Close should leave only the externally owned target texture");
	}

	const String log = device.DumpLog();
	ok &= Check(CountText(log, "CreateTexture id=") == 5,
	            "mixed vector test should create target, three vector textures and one glyph atlas");
	ok &= Check(CountText(log, "WriteTexture id=") == 5,
	            "first mixed frame should upload three vector rasters plus two glyphs only");
	ok &= Check(CountText(log, "SetSampledTexture list=") == 8,
	            "two frames should bind three vector textures plus one glyph atlas each");
	ok &= Check(CountText(log, "Draw list=") == 12,
	            "two six-batch mixed frames should issue twelve draws");
	ok &= Check(device.DestroyTexture(target) == GpuResult::Ok,
	            "vector target should destroy after renderer shutdown");
	ok &= Check(device.GetLiveTextureCount() == 0,
	            "RenderGpuVectorTest should finish with zero RenderNull textures");

	if(ok) {
		Cout() << "RenderGpuVectorTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
