#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderNull/RenderNull.h>
#include <RenderSoftware/RenderSoftware.h>

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
		pos += (int)String(needle).GetCount();
	}
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

static Image MakeTestImage()
{
	ImageBuffer buffer(2, 2);
	buffer[0][0] = MakeRgba(255, 0, 0, 255);
	buffer[0][1] = MakeRgba(0, 255, 0, 255);
	buffer[1][0] = MakeRgba(0, 0, 255, 255);
	buffer[1][1] = MakeRgba(96, 48, 24, 128);
	return Image(buffer);
}

static bool BuildMixedList(UiDisplayList& out)
{
	UiDisplayListBuilder builder;
	builder.FillRect(Rectf(8, 8, 56, 42), Rgba8(225, 70, 30, 255));
	builder.Save();
	builder.ClipRect(Rectf(18, 12, 108, 82));
	Transform2D affine;
	affine.x.x = 1.08;
	affine.x.y = 0.18;
	affine.y.x = -0.12;
	affine.y.y = 0.92;
	affine.t = Pointf(9, 4);
	builder.ConcatTransform(affine);
	builder.StrokeRect(Rectf(20, 18, 78, 56), 5.0, Rgba8(30, 190, 100, 210));
	builder.FillRoundedRect(RoundedRect(Rectf(28, 24, 92, 68), 12.0), Rgba8(40, 90, 230, 170));
	builder.Restore();
	builder.FillRect(Rectf(70, 52, 126, 94), Rgba8(245, 210, 60, 128));
	builder.FillRect(Rectf(180, 140, 220, 180), Rgba8(255, 0, 255, 255));
	return builder.Finish(out);
}

static bool BuildImageList(const Image& image, UiDisplayList& out)
{
	UiDisplayListBuilder builder;
	builder.FillRect(Rectf(4, 4, 34, 28), Rgba8(30, 70, 120, 255));
	builder.Save();
	builder.ClipRect(Rectf(24, 12, 104, 82));
	Transform2D affine;
	affine.x.x = 1.0;
	affine.x.y = 0.15;
	affine.y.x = -0.10;
	affine.y.y = 1.0;
	affine.t = Pointf(8, 2);
	builder.ConcatTransform(affine);
	builder.DrawImage(Rectf(10, 8, 78, 62), image);
	builder.Restore();
	builder.FillRect(Rectf(88, 58, 124, 92), Rgba8(210, 90, 40, 170));
	return builder.Finish(out);
}

static GpuTextureId CreateTarget(NullGpuDevice& device, GpuFormat format, Size size)
{
	GpuTextureDesc desc;
	desc.size = size;
	desc.format = format;
	desc.usage = GpuTextureUsage_ColorAttachment;
	desc.label = "RenderGpu2DTest target";
	GpuTextureId target;
	if(device.CreateTexture(desc, target) != GpuResult::Ok)
		return GpuTextureId();
	return target;
}

static UiRenderer2DTarget MakeTarget(GpuTextureId target, GpuFormat format, Size size)
{
	UiRenderer2DTarget out;
	out.color_target = target;
	out.size = size;
	out.color_format = format;
	out.load_op = GpuLoadOp::Clear;
	out.store_op = GpuStoreOp::Store;
	out.clear_color.red = 0.03f;
	out.clear_color.green = 0.04f;
	out.clear_color.blue = 0.07f;
	out.clear_color.alpha = 1.0f;
	return out;
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	NullGpuDevice device;
	const Size size(128, 96);
	GpuTextureId rgba_target = CreateTarget(device, GpuFormat::RGBA8, size);
	GpuTextureId bgra_target = CreateTarget(device, GpuFormat::BGRA8, size);
	ok &= Check(rgba_target.IsValid() && bgra_target.IsValid(), "offscreen color targets should create");

	UiDisplayList mixed;
	ok &= Check(BuildMixedList(mixed), "mixed Stage-4 display list should build");
	const String semantic_dump = mixed.Dump();

	const RGBA software_background = MakeRgba(7, 10, 18, 255);
	ImagePainter software_painter(size);
	software_painter.DrawRect(Rect(0, 0, size.cx, size.cy), Color(7, 10, 18));
	SoftwareUiRenderer software_renderer;
	ok &= Check(software_renderer.Replay(mixed, software_painter),
	            "Stage-4 display list should replay through the software reference");
	ok &= Check(software_renderer.GetError().IsEmpty(), "software reference replay should retain no error");
	Image software_image = software_painter.GetResult();
	ok &= Check(ImageDiffersFrom(software_image, software_background),
	            "software reference replay should produce visible primitive output");
	ok &= Check(mixed.Dump() == semantic_dump,
	            "software reference replay must not mutate the immutable display list");

	{
		UiRenderer2D renderer(device);
		ok &= Check(renderer.IsReady(), "UiRenderer2D should accept RenderNull capabilities");
		ok &= Check(renderer.Render(mixed, MakeTarget(rgba_target, GpuFormat::RGBA8, size)), "first mixed render should succeed");
		const UiRenderer2DStats first = renderer.GetStats();
		ok &= Check(first.display_op_count == mixed.GetCount(), "renderer should account for every display operation");
		ok &= Check(first.primitive_count == 5, "renderer should account for five logical draw primitives");
		ok &= Check(first.emitted_primitive_count >= 4, "visible primitives should tessellate after clipping");
		ok &= Check(first.clipped_primitive_count >= 1, "fully/outward clipped geometry should be reported");
		ok &= Check(first.triangle_count > 0 && first.vertex_count == first.triangle_count * 3,
		            "renderer should emit triangle-list geometry");
		ok &= Check(first.translucent_vertex_count > 0, "Rgba8 alpha should survive tessellation into vertex data");
		ok &= Check(first.draw_count == 1 && first.batch_count == 1,
		            "all solid primitives should batch into one draw");
		ok &= Check(first.uploaded_bytes == (int64)first.vertex_count * 6 * (int)sizeof(float),
		            "uploaded byte count should match Position2Color4F layout");
		ok &= Check(first.vertex_buffer_grew && first.vertex_buffer_capacity >= first.uploaded_bytes,
		            "first render should allocate a reusable vertex buffer");
		ok &= Check(mixed.Dump() == semantic_dump,
		            "GPU-contract replay must consume the same immutable display list without mutation");

		ok &= Check(renderer.Render(mixed, MakeTarget(rgba_target, GpuFormat::RGBA8, size)), "second mixed render should succeed");
		const UiRenderer2DStats second = renderer.GetStats();
		ok &= Check(second.vertex_count == first.vertex_count, "deterministic replay should emit stable vertex count");
		ok &= Check(!second.vertex_buffer_grew && second.vertex_buffer_capacity == first.vertex_buffer_capacity,
		            "same-sized second frame should reuse the persistent vertex buffer");

		ok &= Check(renderer.Render(mixed, MakeTarget(bgra_target, GpuFormat::BGRA8, size)), "second color format should render");
		ok &= Check(renderer.GetStats().draw_count == 1, "second format should still use one batched draw");
		GpuPipelineDesc captured;
		GpuPipelineId rgba_pipeline; rgba_pipeline.value = 1;
		GpuPipelineId bgra_pipeline; bgra_pipeline.value = 2;
		ok &= Check(device.GetPipelineDesc(rgba_pipeline, captured) && captured.blend_mode == GpuBlendMode::SourceOver,
		            "RGBA UiRenderer2D pipeline should request explicit SourceOver blending");
		ok &= Check(device.GetPipelineDesc(bgra_pipeline, captured) && captured.blend_mode == GpuBlendMode::SourceOver,
		            "BGRA UiRenderer2D pipeline should request explicit SourceOver blending");

		UiDisplayListBuilder empty_builder;
		UiDisplayList empty;
		ok &= Check(empty_builder.Finish(empty), "empty display list should be valid");
		ok &= Check(renderer.Render(empty, MakeTarget(rgba_target, GpuFormat::RGBA8, size)), "clear-only frame should render");
		ok &= Check(renderer.GetStats().vertex_count == 0 && renderer.GetStats().draw_count == 0 && renderer.GetStats().batch_count == 0,
		            "clear-only frame should not issue geometry draw work");

		UiRenderer2DTarget bad_target = MakeTarget(rgba_target, GpuFormat::Unknown, size);
		ok &= Check(!renderer.Render(mixed, bad_target), "unknown target format should be rejected before command recording");
		ok &= Check(!renderer.GetError().IsEmpty(), "renderer failure should retain a diagnostic");
	}

	String log = device.DumpLog();
	ok &= Check(CountText(log, "CreateShader id=") == 2, "solid renderer should keep one persistent shader pair");
	ok &= Check(CountText(log, "CreatePipeline id=") == 2, "solid renderer should cache one pipeline per target format");
	ok &= Check(CountText(log, "CreateBuffer id=") == 1, "solid renderer should reuse one persistent vertex buffer at stable capacity");
	ok &= Check(CountText(log, "WriteBuffer id=") == 3, "three solid geometry frames should upload vertices");
	ok &= Check(CountText(log, "Draw list=") == 3, "three solid geometry frames should issue one draw each");
	ok &= Check(CountText(log, "DestroyPipeline id=") == 2, "solid renderer shutdown should release cached pipelines");
	ok &= Check(CountText(log, "DestroyShader id=") == 2, "solid renderer shutdown should release shaders");
	ok &= Check(CountText(log, "DestroyBuffer id=") == 1, "solid renderer shutdown should release the persistent vertex buffer");

	ok &= Check(device.DestroyTexture(rgba_target) == GpuResult::Ok, "RGBA target should destroy");
	ok &= Check(device.DestroyTexture(bgra_target) == GpuResult::Ok, "BGRA target should destroy");

	{
		NullGpuDevice image_device;
		GpuTextureId image_target = CreateTarget(image_device, GpuFormat::RGBA8, size);
		Image image = MakeTestImage();
		UiDisplayList image_list;
		ok &= Check(BuildImageList(image, image_list), "Stage-5 image display list should build");
		const String image_dump = image_list.Dump();
		UiRenderer2D renderer(image_device);
		ok &= Check(renderer.IsReady(), "image renderer should accept RenderNull capabilities");
		ok &= Check(renderer.Render(image_list, MakeTarget(image_target, GpuFormat::RGBA8, size)),
		            "ordered solid/image/solid replay should succeed");
		const UiRenderer2DStats first = renderer.GetStats();
		ok &= Check(first.image_count == 1 && first.texture_upload_count == 1,
		            "first image frame should upload exactly one immutable image texture");
		ok &= Check(first.textured_vertex_count > 0 && first.textured_vertex_count % 3 == 0,
		            "DrawImage should emit triangle-list UV geometry");
		ok &= Check(first.batch_count == 3 && first.draw_count == 3,
		            "solid/image/solid ordering should be preserved as three draw batches");
		ok &= Check(first.clipped_primitive_count >= 1,
		            "clipped transformed image should preserve clipping evidence");
		ok &= Check(first.textured_vertex_buffer_grew && first.textured_vertex_buffer_capacity > 0,
		            "first image frame should allocate a reusable textured vertex buffer");
		ok &= Check(image_list.Dump() == image_dump,
		            "GPU image replay must not mutate the immutable display list");

		ok &= Check(renderer.Render(image_list, MakeTarget(image_target, GpuFormat::RGBA8, size)),
		            "second image frame should reuse cached resources");
		const UiRenderer2DStats second = renderer.GetStats();
		ok &= Check(second.texture_upload_count == 0,
		            "second image frame should reuse the immutable-image texture cache");
		ok &= Check(!second.textured_vertex_buffer_grew && second.textured_vertex_buffer_capacity == first.textured_vertex_buffer_capacity,
		            "second image frame should reuse the textured vertex buffer");
		ok &= Check(second.batch_count == 3 && second.draw_count == 3,
		            "ordered image replay should remain deterministic across frames");
		GpuPipelineDesc sampled_pipeline;
		bool found_sampled_pipeline = false;
		for(int id = 1; id <= 4; ++id) {
			GpuPipelineId candidate; candidate.value = id;
			if(image_device.GetPipelineDesc(candidate, sampled_pipeline) && sampled_pipeline.sampled_texture_count == 1) {
				found_sampled_pipeline = sampled_pipeline.vertex_layout == GpuVertexLayout::Position2Uv2Color4F &&
				                         sampled_pipeline.blend_mode == GpuBlendMode::SourceOver &&
				                         sampled_pipeline.sampler_filter == GpuSamplerFilter::Linear &&
				                         sampled_pipeline.sampler_address == GpuSamplerAddressMode::ClampToEdge;
				break;
			}
		}
		ok &= Check(found_sampled_pipeline,
		            "image renderer should request the bounded UV + SourceOver + linear-clamp sampled pipeline");
		renderer.Close();
		String image_log = image_device.DumpLog();
		ok &= Check(CountText(image_log, "WriteTexture id=") == 1,
		            "immutable image cache should upload pixel data once across repeated frames");
		ok &= Check(CountText(image_log, "SetSampledTexture list=") == 2,
		            "each image frame should bind its cached sampled texture once");
		ok &= Check(CountText(image_log, "Draw list=") == 6,
		            "two ordered solid/image/solid frames should issue six draws");
		ok &= Check(image_device.DestroyTexture(image_target) == GpuResult::Ok,
		            "image target should destroy after renderer-owned cache shutdown");
	}

	if(ok) {
		Cout() << "RenderGpu2DTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
