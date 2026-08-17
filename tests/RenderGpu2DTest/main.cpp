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
		pos += (int)String(needle).GetCount();
	}
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

		ok &= Check(renderer.Render(mixed, MakeTarget(rgba_target, GpuFormat::RGBA8, size)), "second mixed render should succeed");
		const UiRenderer2DStats second = renderer.GetStats();
		ok &= Check(second.vertex_count == first.vertex_count, "deterministic replay should emit stable vertex count");
		ok &= Check(!second.vertex_buffer_grew && second.vertex_buffer_capacity == first.vertex_buffer_capacity,
		            "same-sized second frame should reuse the persistent vertex buffer");

		ok &= Check(renderer.Render(mixed, MakeTarget(bgra_target, GpuFormat::BGRA8, size)), "second color format should render");
		ok &= Check(renderer.GetStats().draw_count == 1, "second format should still use one batched draw");

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
	ok &= Check(CountText(log, "CreateShader id=") == 2, "renderer should keep one persistent shader pair");
	ok &= Check(CountText(log, "CreatePipeline id=") == 2, "renderer should cache one pipeline per target format");
	ok &= Check(CountText(log, "CreateBuffer id=") == 1, "renderer should reuse one persistent vertex buffer at stable capacity");
	ok &= Check(CountText(log, "WriteBuffer id=") == 3, "three geometry frames should upload vertices");
	ok &= Check(CountText(log, "Draw list=") == 3, "three geometry frames should issue one draw each");
	ok &= Check(CountText(log, "DestroyPipeline id=") == 2, "renderer shutdown should release cached pipelines");
	ok &= Check(CountText(log, "DestroyShader id=") == 2, "renderer shutdown should release shaders");
	ok &= Check(CountText(log, "DestroyBuffer id=") == 1, "renderer shutdown should release the persistent vertex buffer");

	ok &= Check(device.DestroyTexture(rgba_target) == GpuResult::Ok, "RGBA target should destroy");
	ok &= Check(device.DestroyTexture(bgra_target) == GpuResult::Ok, "BGRA target should destroy");

	if(ok) {
		Cout() << "RenderGpu2DTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
