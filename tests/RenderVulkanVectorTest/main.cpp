#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderVulkan/RenderVulkanRhi.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Upp;
using Upp::VulkanTestHooks::ClearVulkanRuntimeDeviceDiagnostics;
using Upp::VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics;
using Upp::VulkanTestHooks::VulkanRuntimeDeviceDiagnostics;

static FARPROC WINAPI TestResolver(HMODULE, LPCSTR)
{
	return reinterpret_cast<FARPROC>(1);
}

static bool Check(bool condition, const char *message)
{
	if(!condition)
		Cout() << "FAIL: " << message << EOL;
	return condition;
}

static UiPath MakePath()
{
	UiPath path;
	path.MoveTo(Pointf(9, 9));
	path.LineTo(Pointf(68, 8));
	path.CubicTo(Pointf(88, 15), Pointf(88, 45), Pointf(66, 51));
	path.LineTo(Pointf(11, 49));
	path.QuadraticTo(Pointf(2, 28), Pointf(9, 9));
	path.Close();
	path.MoveTo(Pointf(29, 22));
	path.LineTo(Pointf(53, 22));
	path.LineTo(Pointf(53, 37));
	path.LineTo(Pointf(29, 37));
	path.Close();
	return path;
}

static UiPaint MakeGradient()
{
	UiPaint paint = UiPaint::Linear(Pointf(8, 8), Pointf(80, 50),
	                                Rgba8(55, 135, 235, 235),
	                                Rgba8(235, 85, 50, 190), UiGradientSpread::Pad);
	paint.AddStop(0.45, Rgba8(105, 220, 155, 220));
	return paint;
}

static UiStrokeStyle MakeStroke()
{
	UiStrokeStyle stroke;
	stroke.width = 2.5;
	stroke.cap = UiLineCap::Round;
	stroke.join = UiLineJoin::Round;
	stroke.dash << 6.0 << 3.0;
	stroke.dash_offset = 1.0;
	return stroke;
}

static String SampleSvg()
{
	return "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'>"
	       "<circle cx='16' cy='16' r='13' fill='#6576d8'/>"
	       "<path d='M9 17 L14 22 L24 10' fill='none' stroke='#fff' stroke-width='3'/></svg>";
}

static WString Stage5Label()
{
	WString text;
	text.Cat('V');
	text.Cat('5');
	return text;
}

static bool MakeScene(UiDisplayList& out)
{
	UiDisplayListBuilder builder;
	builder.FillRect(Rectf(2, 2, 28, 20), Rgba8(28, 65, 135, 255));
	builder.Save();
	builder.ClipRect(Rectf(6, 5, 150, 75));
	Transform2D affine;
	affine.x.x = 1.3;
	affine.x.y = 0.10;
	affine.y.x = -0.06;
	affine.y.y = 1.12;
	affine.t = Pointf(5, 2);
	builder.ConcatTransform(affine);
	builder.FillPath(MakePath(), MakeGradient(), UiFillRule::EvenOdd);
	builder.StrokePath(MakePath(), UiPaint::Solid(Rgba8(245, 245, 250, 220)), MakeStroke());
	builder.DrawSvg(Rectf(84, 12, 114, 42), SampleSvg());
	builder.DrawText(Pointf(90, 47), Stage5Label(), SansSerif(14).Bold(), Rgba8(245, 248, 255, 230));
	builder.Restore();
	builder.FillRect(Rectf(122, 52, 156, 77), Rgba8(220, 100, 35, 185));
	return builder.Finish(out);
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	ClearVulkanRuntimeDeviceDiagnostics();
	HWND hwnd = CreateWindowExW(0, L"STATIC", L"RenderVulkanVectorTest", WS_POPUP,
	                            0, 0, 164, 82, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	ok &= Check(hwnd != nullptr, "hidden Win32 vector-test window should create");

	VulkanSurfaceSession session;
	if(hwnd) {
		GpuNativeWindowDesc native_window;
		native_window.kind = GpuNativeWindowKind::Win32;
		native_window.handle = (uintptr_t)hwnd;
		ok &= Check(session.Open(true, native_window, &TestResolver),
		            "Vulkan vector session should open with validation");
		ok &= Check(session.IsReady(), "Vulkan vector session should be ready");
	}

	if(session.IsReady()) {
		{
			VulkanGpuDevice device(session);
			ok &= Check(device.IsReady(), "VulkanGpuDevice should be ready for vector rendering");
			UiDisplayList scene;
			ok &= Check(MakeScene(scene), "vector display list should build");
			const String scene_dump = scene.Dump();

			GpuTextureDesc target_desc;
			target_desc.size = Size(164, 82);
			target_desc.format = GpuFormat::RGBA8;
			target_desc.usage = GpuTextureUsage_ColorAttachment;
			GpuTextureId target;
			ok &= Check(device.CreateTexture(target_desc, target) == GpuResult::Ok,
			            "offscreen vector target should create");

			{
				UiRenderer2D renderer(device);
				ok &= Check(renderer.IsReady(), "UiRenderer2D should be ready for real Vulkan vectors");
				UiRenderer2DTarget offscreen;
				offscreen.color_target = target;
				offscreen.size = target_desc.size;
				offscreen.color_format = target_desc.format;
				offscreen.load_op = GpuLoadOp::Clear;
				offscreen.store_op = GpuStoreOp::Store;
				offscreen.clear_color.alpha = 1.0f;

				ok &= Check(renderer.Render(scene, offscreen),
				            "real Vulkan offscreen vector/text frame should render");
				const UiRenderer2DStats first = renderer.GetStats();
				ok &= Check(first.vector_op_count == 3 && first.vector_path_count == 2 &&
				            first.svg_count == 1 && first.gradient_count == 1,
				            "first Vulkan vector frame should account for path/gradient/SVG intent");
				ok &= Check(first.vector_cache_miss_count == 3 && first.vector_raster_count == 3 &&
				            first.vector_cache_entry_count == 3,
				            "first Vulkan vector frame should rasterize three unique assets");
				ok &= Check(first.texture_upload_count == 3,
				            "first Vulkan vector frame should upload three materialized images");
				ok &= Check(first.text_run_count == 1 && first.glyph_count == 2 &&
				            first.glyph_cache_miss_count == 2 && first.glyph_atlas_upload_count == 2,
				            "vector materialization should preserve and populate the accepted text path");
				ok &= Check(first.batch_count == 6 && first.draw_count == 6,
				            "real Vulkan should preserve solid/vector/vector/SVG/text/solid ordering");
				ok &= Check(scene.Dump() == scene_dump,
				            "real Vulkan vector replay must not mutate the display list");

				ok &= Check(renderer.Render(scene, offscreen),
				            "second Vulkan vector frame should render from CPU/GPU/text caches");
				const UiRenderer2DStats second = renderer.GetStats();
				ok &= Check(second.vector_cache_miss_count == 0 && second.vector_raster_count == 0 &&
				            second.vector_cache_entry_count == 3,
				            "second Vulkan vector frame should reuse all U++ vector rasters");
				ok &= Check(second.texture_upload_count == 0,
				            "second Vulkan vector frame should reuse GPU image textures");
				ok &= Check(second.glyph_cache_miss_count == 0 && second.glyph_atlas_upload_count == 0,
				            "second Vulkan vector frame should also reuse the glyph atlas");
				ok &= Check(second.batch_count == 6 && second.draw_count == 6,
				            "cached Vulkan Stage-5 order should remain deterministic");

				GpuSurfaceDesc surface_desc;
				surface_desc.size = Size(164, 82);
				surface_desc.native_window.kind = GpuNativeWindowKind::Win32;
				surface_desc.native_window.handle = (uintptr_t)hwnd;
				GpuSurfaceId surface;
				ok &= Check(device.CreateSurface(surface_desc, surface) == GpuResult::Ok,
				            "neutral vector-test surface should bind the session window");
				GpuSwapchainDesc swapchain_desc;
				swapchain_desc.surface = surface;
				swapchain_desc.size = Size(164, 82);
				swapchain_desc.color_format = GpuFormat::RGBA8;
				swapchain_desc.image_count = 2;
				GpuSwapchainId swapchain;
				ok &= Check(device.CreateSwapchain(swapchain_desc, swapchain) == GpuResult::Ok,
				            "neutral vector-test swapchain should create through session authority");
				GpuFrameInfo frame;
				ok &= Check(device.BeginFrame(swapchain, frame) == GpuResult::Ok,
				            "vector swapchain frame should acquire");
				GpuClearColor clear;
				clear.red = 0.02f; clear.green = 0.03f; clear.blue = 0.05f; clear.alpha = 1.0f;
				ok &= Check(renderer.RenderFrame(scene, frame, clear),
				            "mixed vector/text scene should render into acquired swapchain image");
				ok &= Check(renderer.GetStats().vector_cache_miss_count == 0 &&
				            renderer.GetStats().vector_raster_count == 0 &&
				            renderer.GetStats().texture_upload_count == 0 &&
				            renderer.GetStats().glyph_cache_miss_count == 0 &&
				            renderer.GetStats().glyph_atlas_upload_count == 0,
				            "swapchain Stage-5 render should reuse vector, image and glyph caches");
				ok &= Check(device.Present(frame.frame) == GpuResult::Ok,
				            "vector swapchain frame should present through session authority");
				ok &= Check(device.DestroySwapchain(swapchain) == GpuResult::Ok,
				            "vector-test swapchain should destroy after presentation");
				ok &= Check(device.DestroySurface(surface) == GpuResult::Ok,
				            "vector-test surface should destroy after swapchain");
			}

			ok &= Check(device.DestroyTexture(target) == GpuResult::Ok,
			            "offscreen vector target should destroy after renderer shutdown");
			ok &= Check(device.GetLiveBufferCount() == 0 && device.GetLiveTextureCount() == 0 &&
			            device.GetLiveShaderCount() == 0 && device.GetLivePipelineCount() == 0 &&
			            device.GetLiveCommandCount() == 0 && device.GetLiveSurfaceCount() == 0 &&
			            device.GetLiveSwapchainCount() == 0 && device.GetLiveFrameCount() == 0,
			            "explicit Vulkan vector cleanup should leave zero adapter-owned resources");
		}
		session.Close();
		ok &= Check(session.GetReport().validation_warning_count == 0,
		            "vector Vulkan path should emit zero validation warnings");
		ok &= Check(session.GetReport().validation_error_count == 0,
		            "vector Vulkan path should emit zero validation errors");
	}
	else
		session.Close();

	if(hwnd)
		DestroyWindow(hwnd);
	const VulkanRuntimeDeviceDiagnostics diag = GetVulkanRuntimeDeviceDiagnostics();
	ok &= Check(diag.runtime_live_count == 0 && diag.instance_live_count == 0 &&
	            diag.debug_messenger_live_count == 0 && diag.surface_live_count == 0 &&
	            diag.device_live_count == 0 && diag.swapchain_live_count == 0,
	            "Vulkan vector test should finish with zero Vulkan ownership diagnostics");

	if(ok) {
		Cout() << "RenderVulkanVectorTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
