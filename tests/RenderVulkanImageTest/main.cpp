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

static RGBA Pixel(byte r, byte g, byte b, byte a = 255)
{
	RGBA out;
	out.r = r;
	out.g = g;
	out.b = b;
	out.a = a;
	return out;
}

static Image MakeImage()
{
	ImageBuffer buffer(2, 2);
	buffer[0][0] = Pixel(255, 0, 0);
	buffer[0][1] = Pixel(0, 255, 0);
	buffer[1][0] = Pixel(0, 0, 255);
	buffer[1][1] = Pixel(255, 255, 255);
	return Image(buffer);
}

static bool MakeScene(const Image& image, UiDisplayList& out)
{
	UiDisplayListBuilder builder;
	builder.FillRect(Rectf(2, 2, 18, 18), Rgba8(30, 70, 140, 255));
	builder.Save();
	builder.ClipRect(Rectf(8, 6, 58, 58));
	Transform2D transform;
	transform.x.x = 0.95;
	transform.x.y = 0.12;
	transform.y.x = -0.08;
	transform.y.y = 1.0;
	transform.t = Pointf(5, 2);
	builder.ConcatTransform(transform);
	builder.DrawImage(Rectf(4, 4, 52, 50), image);
	builder.Restore();
	builder.FillRect(Rectf(44, 42, 62, 62), Rgba8(220, 100, 35, 180));
	return builder.Finish(out);
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	ClearVulkanRuntimeDeviceDiagnostics();
	HWND hwnd = CreateWindowExW(0, L"STATIC", L"RenderVulkanImageTest", WS_POPUP,
	                            0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	ok &= Check(hwnd != nullptr, "hidden Win32 image-test window should create");

	VulkanSurfaceSession session;
	if(hwnd) {
		GpuNativeWindowDesc native_window;
		native_window.kind = GpuNativeWindowKind::Win32;
		native_window.handle = (uintptr_t)hwnd;
		ok &= Check(session.Open(true, native_window, &TestResolver), "Vulkan image session should open with validation");
		ok &= Check(session.IsReady(), "Vulkan image session should be ready");
	}

	if(session.IsReady()) {
		{
			VulkanGpuDevice device(session);
			ok &= Check(device.IsReady(), "VulkanGpuDevice should be ready for image rendering");
			Image image = MakeImage();
			UiDisplayList scene;
			ok &= Check(MakeScene(image, scene), "sampled-image display list should build");
			const String scene_dump = scene.Dump();

			GpuTextureDesc target_desc;
			target_desc.size = Size(64, 64);
			target_desc.format = GpuFormat::RGBA8;
			target_desc.usage = GpuTextureUsage_ColorAttachment;
			GpuTextureId target;
			ok &= Check(device.CreateTexture(target_desc, target) == GpuResult::Ok,
			            "offscreen image target should create");

			{
				UiRenderer2D renderer(device);
				ok &= Check(renderer.IsReady(), "UiRenderer2D should be ready on real Vulkan");
				UiRenderer2DTarget offscreen;
				offscreen.color_target = target;
				offscreen.size = target_desc.size;
				offscreen.color_format = target_desc.format;
				offscreen.load_op = GpuLoadOp::Clear;
				offscreen.store_op = GpuStoreOp::Store;
				offscreen.clear_color.alpha = 1.0f;
				ok &= Check(renderer.Render(scene, offscreen), "real Vulkan offscreen DrawImage should render");
				const UiRenderer2DStats first = renderer.GetStats();
				ok &= Check(first.image_count == 1 && first.texture_upload_count == 1 && first.textured_vertex_count > 0,
				            "first Vulkan image frame should upload and draw one sampled image");
				ok &= Check(first.batch_count == 3 && first.draw_count == 3,
				            "real Vulkan should preserve solid/image/solid draw order");
				ok &= Check(scene.Dump() == scene_dump, "real Vulkan replay must not mutate the immutable image list");

				ok &= Check(renderer.Render(scene, offscreen), "second Vulkan image frame should render from cache");
				const UiRenderer2DStats second = renderer.GetStats();
				ok &= Check(second.texture_upload_count == 0 && second.batch_count == 3 && second.draw_count == 3,
				            "second Vulkan image frame should reuse cached texture and preserve ordering");

				GpuSurfaceDesc surface_desc;
				surface_desc.size = Size(64, 64);
				surface_desc.native_window.kind = GpuNativeWindowKind::Win32;
				surface_desc.native_window.handle = (uintptr_t)hwnd;
				GpuSurfaceId surface;
				ok &= Check(device.CreateSurface(surface_desc, surface) == GpuResult::Ok,
				            "neutral image-test surface should bind session window");
				GpuSwapchainDesc swapchain_desc;
				swapchain_desc.surface = surface;
				swapchain_desc.size = Size(64, 64);
				swapchain_desc.color_format = GpuFormat::RGBA8;
				swapchain_desc.image_count = 2;
				GpuSwapchainId swapchain;
				ok &= Check(device.CreateSwapchain(swapchain_desc, swapchain) == GpuResult::Ok,
				            "neutral image-test swapchain should create through session authority");
				GpuFrameInfo frame;
				ok &= Check(device.BeginFrame(swapchain, frame) == GpuResult::Ok,
				            "sampled-image swapchain frame should acquire");
				GpuClearColor clear;
				clear.red = 0.02f; clear.green = 0.03f; clear.blue = 0.05f; clear.alpha = 1.0f;
				ok &= Check(renderer.RenderFrame(scene, frame, clear),
				            "sampled-image scene should render into acquired swapchain image");
				ok &= Check(renderer.GetStats().texture_upload_count == 0,
				            "swapchain image render should reuse the already cached U++ Image texture");
				ok &= Check(device.Present(frame.frame) == GpuResult::Ok,
				            "sampled-image swapchain frame should present through session authority");
				ok &= Check(device.DestroySwapchain(swapchain) == GpuResult::Ok,
				            "image-test swapchain should destroy after presentation");
				ok &= Check(device.DestroySurface(surface) == GpuResult::Ok,
				            "image-test surface should destroy after swapchain");
			}

			ok &= Check(device.DestroyTexture(target) == GpuResult::Ok,
			            "offscreen target should destroy after renderer cache shutdown");
			ok &= Check(device.GetLiveBufferCount() == 0 && device.GetLiveTextureCount() == 0 &&
			            device.GetLiveShaderCount() == 0 && device.GetLivePipelineCount() == 0 &&
			            device.GetLiveCommandCount() == 0 && device.GetLiveSurfaceCount() == 0 &&
			            device.GetLiveSwapchainCount() == 0 && device.GetLiveFrameCount() == 0,
			            "explicit Vulkan image cleanup should leave zero adapter-owned resources");
		}
		session.Close();
		ok &= Check(session.GetReport().validation_warning_count == 0,
		            "sampled-image Vulkan path should emit zero validation warnings");
		ok &= Check(session.GetReport().validation_error_count == 0,
		            "sampled-image Vulkan path should emit zero validation errors");
	}
	else
		session.Close();

	if(hwnd)
		DestroyWindow(hwnd);
	const VulkanRuntimeDeviceDiagnostics diag = GetVulkanRuntimeDeviceDiagnostics();
	ok &= Check(diag.runtime_live_count == 0 && diag.instance_live_count == 0 &&
	            diag.debug_messenger_live_count == 0 && diag.surface_live_count == 0 &&
	            diag.device_live_count == 0 && diag.swapchain_live_count == 0,
	            "Vulkan image test should finish with zero Vulkan ownership diagnostics");

	if(ok) {
		Cout() << "RenderVulkanImageTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
