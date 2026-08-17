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

static WString TestText()
{
	WString text;
	text.Cat('A');
	text.Cat('B');
	text.Cat('B');
	text.Cat('A');
	return text;
}

static bool MakeScene(UiDisplayList& out)
{
	UiDisplayListBuilder builder;
	builder.FillRect(Rectf(2, 2, 30, 22), Rgba8(30, 70, 140, 255));
	builder.Save();
	builder.ClipRect(Rectf(8, 5, 150, 70));
	Transform2D transform;
	transform.x.x = 0.98;
	transform.x.y = 0.04;
	transform.y.x = -0.03;
	transform.y.y = 1.0;
	transform.t = Pointf(3, 1);
	builder.ConcatTransform(transform);
	builder.DrawText(Pointf(14, 16), TestText(), SansSerif(22).Bold(), Rgba8(230, 242, 255, 220));
	builder.Restore();
	builder.FillRect(Rectf(118, 50, 154, 74), Rgba8(220, 100, 35, 180));
	return builder.Finish(out);
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	ClearVulkanRuntimeDeviceDiagnostics();
	HWND hwnd = CreateWindowExW(0, L"STATIC", L"RenderVulkanTextTest", WS_POPUP,
	                            0, 0, 160, 80, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	ok &= Check(hwnd != nullptr, "hidden Win32 text-test window should create");

	VulkanSurfaceSession session;
	if(hwnd) {
		GpuNativeWindowDesc native_window;
		native_window.kind = GpuNativeWindowKind::Win32;
		native_window.handle = (uintptr_t)hwnd;
		ok &= Check(session.Open(true, native_window, &TestResolver), "Vulkan text session should open with validation");
		ok &= Check(session.IsReady(), "Vulkan text session should be ready");
	}

	if(session.IsReady()) {
		{
			VulkanGpuDevice device(session);
			ok &= Check(device.IsReady(), "VulkanGpuDevice should be ready for glyph-atlas rendering");
			UiDisplayList scene;
			ok &= Check(MakeScene(scene), "text display list should build");
			const String scene_dump = scene.Dump();

			GpuTextureDesc target_desc;
			target_desc.size = Size(160, 80);
			target_desc.format = GpuFormat::RGBA8;
			target_desc.usage = GpuTextureUsage_ColorAttachment;
			GpuTextureId target;
			ok &= Check(device.CreateTexture(target_desc, target) == GpuResult::Ok,
			            "offscreen text target should create");

			{
				UiRenderer2D renderer(device);
				ok &= Check(renderer.IsReady(), "UiRenderer2D should be ready for real Vulkan text");
				UiRenderer2DTarget offscreen;
				offscreen.color_target = target;
				offscreen.size = target_desc.size;
				offscreen.color_format = target_desc.format;
				offscreen.load_op = GpuLoadOp::Clear;
				offscreen.store_op = GpuStoreOp::Store;
				offscreen.clear_color.alpha = 1.0f;

				ok &= Check(renderer.Render(scene, offscreen), "real Vulkan offscreen glyph-atlas frame should render");
				const UiRenderer2DStats first = renderer.GetStats();
				ok &= Check(first.text_run_count == 1 && first.glyph_count == 4,
				            "first Vulkan text frame should place four glyphs");
				ok &= Check(first.glyph_cache_miss_count == 2 && first.glyph_atlas_page_count == 1,
				            "first Vulkan text frame should cache two distinct glyphs in one atlas page");
				ok &= Check(first.glyph_atlas_upload_count == 2,
				            "first Vulkan text frame should upload only two padded glyph regions");
				ok &= Check(first.batch_count == 3 && first.draw_count == 3,
				            "real Vulkan should preserve solid/text/solid draw order");
				ok &= Check(first.textured_vertex_count > 0,
				            "real Vulkan text should emit sampled glyph geometry");
				ok &= Check(scene.Dump() == scene_dump,
				            "real Vulkan text replay must not mutate the immutable display list");

				ok &= Check(renderer.Render(scene, offscreen), "second Vulkan text frame should render from atlas cache");
				const UiRenderer2DStats second = renderer.GetStats();
				ok &= Check(second.glyph_cache_miss_count == 0 && second.glyph_atlas_upload_count == 0,
				            "second Vulkan text frame should perform no glyph raster/upload work");
				ok &= Check(second.glyph_atlas_page_count == 1 && second.batch_count == 3 && second.draw_count == 3,
				            "cached Vulkan text should retain one page and deterministic ordering");

				GpuSurfaceDesc surface_desc;
				surface_desc.size = Size(160, 80);
				surface_desc.native_window.kind = GpuNativeWindowKind::Win32;
				surface_desc.native_window.handle = (uintptr_t)hwnd;
				GpuSurfaceId surface;
				ok &= Check(device.CreateSurface(surface_desc, surface) == GpuResult::Ok,
				            "neutral text-test surface should bind the session window");
				GpuSwapchainDesc swapchain_desc;
				swapchain_desc.surface = surface;
				swapchain_desc.size = Size(160, 80);
				swapchain_desc.color_format = GpuFormat::RGBA8;
				swapchain_desc.image_count = 2;
				GpuSwapchainId swapchain;
				ok &= Check(device.CreateSwapchain(swapchain_desc, swapchain) == GpuResult::Ok,
				            "neutral text-test swapchain should create through session authority");
				GpuFrameInfo frame;
				ok &= Check(device.BeginFrame(swapchain, frame) == GpuResult::Ok,
				            "glyph-atlas swapchain frame should acquire");
				GpuClearColor clear;
				clear.red = 0.02f; clear.green = 0.03f; clear.blue = 0.05f; clear.alpha = 1.0f;
				ok &= Check(renderer.RenderFrame(scene, frame, clear),
				            "glyph-atlas scene should render into acquired swapchain image");
				ok &= Check(renderer.GetStats().glyph_cache_miss_count == 0 &&
				            renderer.GetStats().glyph_atlas_upload_count == 0,
				            "swapchain text render should reuse the existing atlas cache");
				ok &= Check(device.Present(frame.frame) == GpuResult::Ok,
				            "glyph-atlas swapchain frame should present through session authority");
				ok &= Check(device.DestroySwapchain(swapchain) == GpuResult::Ok,
				            "text-test swapchain should destroy after presentation");
				ok &= Check(device.DestroySurface(surface) == GpuResult::Ok,
				            "text-test surface should destroy after swapchain");
			}

			ok &= Check(device.DestroyTexture(target) == GpuResult::Ok,
			            "offscreen text target should destroy after renderer atlas shutdown");
			ok &= Check(device.GetLiveBufferCount() == 0 && device.GetLiveTextureCount() == 0 &&
			            device.GetLiveShaderCount() == 0 && device.GetLivePipelineCount() == 0 &&
			            device.GetLiveCommandCount() == 0 && device.GetLiveSurfaceCount() == 0 &&
			            device.GetLiveSwapchainCount() == 0 && device.GetLiveFrameCount() == 0,
			            "explicit Vulkan text cleanup should leave zero adapter-owned resources");
		}
		session.Close();
		ok &= Check(session.GetReport().validation_warning_count == 0,
		            "glyph-atlas Vulkan path should emit zero validation warnings");
		ok &= Check(session.GetReport().validation_error_count == 0,
		            "glyph-atlas Vulkan path should emit zero validation errors");
	}
	else
		session.Close();

	if(hwnd)
		DestroyWindow(hwnd);
	const VulkanRuntimeDeviceDiagnostics diag = GetVulkanRuntimeDeviceDiagnostics();
	ok &= Check(diag.runtime_live_count == 0 && diag.instance_live_count == 0 &&
	            diag.debug_messenger_live_count == 0 && diag.surface_live_count == 0 &&
	            diag.device_live_count == 0 && diag.swapchain_live_count == 0,
	            "Vulkan text test should finish with zero Vulkan ownership diagnostics");

	if(ok) {
		Cout() << "RenderVulkanTextTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
