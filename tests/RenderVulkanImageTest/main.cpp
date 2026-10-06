#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderVulkan/RenderVulkanRhi.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include <cmath>
#include <RenderSoftware/RenderSoftware.h>

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


static bool CheckPixelReadback(VulkanGpuDevice& device)
{
	bool ok = true;
	ImageBuffer buffer(3, 1);
	buffer[0][0] = Pixel(255, 0, 0);
	buffer[0][1] = Pixel(0, 128, 0, 128); // U++ images store premultiplied channels.
	buffer[0][2] = Pixel(0, 0, 255);
	Image image(buffer);
	const GpuFormat formats[] = { GpuFormat::RGBA8, GpuFormat::BGRA8,
	                              GpuFormat::RGBA8Srgb, GpuFormat::BGRA8Srgb };
	for(GpuFormat format : formats) {
		GpuTextureDesc desc;
		desc.size = Size(32, 16);
		desc.format = format;
		desc.usage = GpuTextureUsage_ColorAttachment | GpuTextureUsage_TransferSrc;
		GpuTextureId target;
		if(!Check(device.CreateTexture(desc, target) == GpuResult::Ok, "readback target creates")) {
			ok = false;
			continue;
		}
		Vector<byte> bytes;
		ok &= Check(!device.ReadTexturePixels(target, bytes) && bytes.IsEmpty(),
		            "uninitialized readback rejected without allocating output");
		{
			UiRenderer2D renderer(device);
			UiDisplayListBuilder builder;
			builder.DrawImage(Rectf(0, 0, 8, 16), image, Rect(1, 0, 2, 1));
			builder.DrawImage(Rectf(8, 0, 16, 16), image, Rect(1, 0, 2, 1),
			                  Rgba8(80, 160, 240, 128), true);
			builder.DrawImage(Rectf(16, 0, 24, 16), image, Rect(0, 0, 1, 1),
			                  Rgba8(128, 255, 255, 128));
			UiDisplayList list;
			ok &= Check(builder.Finish(list), "transparent crop/tint scene builds");
			UiRenderer2DTarget output;
			output.color_target = target; output.size = desc.size; output.color_format = format;
			output.load_op = GpuLoadOp::Clear; output.store_op = GpuStoreOp::Store;
			output.clear_color.alpha = 1;
			ok &= Check(renderer.Render(list, output) && device.ReadTexturePixels(target, bytes),
			            "all four UNORM/sRGB formats render and read back");
			const bool srgb = format == GpuFormat::RGBA8Srgb || format == GpuFormat::BGRA8Srgb;
			const bool bgra = format == GpuFormat::BGRA8 || format == GpuFormat::BGRA8Srgb;
			auto expected = [&](int channel, double alpha) {
				double c = channel / 255.0;
				if(srgb) c = c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
				c *= alpha;
				if(srgb) c = c <= 0.0031308 ? c * 12.92 : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
				return (int)std::round(c * 255);
			};
			int mismatch_reports = 0;
			auto matches = [&](int x, int y, int red, int green, int blue) {
				if(bytes.GetCount() != 32 * 16 * 4) return false;
				const byte *p = bytes.Begin() + 4 * (y * 32 + x);
				const bool match = abs((int)p[bgra ? 2 : 0] - red) <= 2 &&
				                   abs((int)p[1] - green) <= 2 &&
				                   abs((int)p[bgra ? 0 : 2] - blue) <= 2 && p[3] == 255;
				if(!match && mismatch_reports++ < 8) Cout() << "pixel format=" << (int)format << " xy=" << x << "," << y
				                 << " actual=" << (int)p[0] << "," << (int)p[1] << "," << (int)p[2]
				                 << " expected RGB=" << red << "," << green << "," << blue << EOL;
				return match;
			};
			for(int y : { 0, 7, 15 }) {
				ok &= Check(matches(0, y, 0, expected(255, 128.0 / 255), 0) &&
				            matches(7, y, 0, expected(255, 128.0 / 255), 0),
				            "transparent source crop clamps both edges without outside colour bleed");
				const double alpha = (128.0 / 255) * (128.0 / 255);
				ok &= Check(matches(8, y, expected(80, alpha), expected(160, alpha), expected(240, alpha)) &&
				            matches(15, y, expected(80, alpha), expected(160, alpha), expected(240, alpha)),
				            "mask source alpha and tint opacity compose in target working colour space");
				ok &= Check(matches(16, y, expected(128, 128.0 / 255), 0, 0) &&
				            matches(23, y, expected(128, 128.0 / 255), 0, 0),
				            "ordinary RGB tint and opacity compose in target working colour space");
				ok &= Check(matches(24, y, 0, 0, 0), "clear colour outside geometry survives");
			}

			if(!srgb && bytes.GetCount() == 32 * 16 * 4) {
				ImagePainter painter(Size(32, 16));
				painter.DrawRect(0, 0, 32, 16, Black());
				SoftwareUiRenderer software;
				ok &= Check(software.Replay(list, painter), "software reference scene replays");
				Image reference = painter.GetResult();
				bool parity = true;
				for(int y = 0; y < 16; ++y)
					for(int x = 0; x < 32; ++x) {
						const RGBA p = reference[y][x];
						parity &= matches(x, y, p.r, p.g, p.b);
					}
				ok &= Check(parity, "all UNORM pixels match the software reference within two code values");
			}
			ok &= Check(renderer.Render(list, output) && renderer.GetStats().texture_upload_count == 0 &&
			            device.ReadTexturePixels(target, bytes), "warm replay survives restored readback layout");

			UiPath solid;
			solid.MoveTo(Pointf(1, 1)).LineTo(Pointf(7, 1)).LineTo(Pointf(7, 15))
			     .LineTo(Pointf(1, 15)).Close();
			UiDisplayListBuilder masks;
			masks.FillPath(solid, UiPaint::Solid(Rgba8(255, 0, 0, 128)));
			masks.Save(); masks.ConcatTransform(Transform2D::Translation(8, 0));
			masks.FillPath(solid, UiPaint::Solid(Rgba8(0, 0, 255, 64)));
			masks.Restore();
			UiDisplayList mask_list;
			ok &= Check(masks.Finish(mask_list) && renderer.Render(mask_list, output) &&
			            device.ReadTexturePixels(target, bytes), "solid path colours render from shared coverage");
			ok &= Check(renderer.GetStats().vector_raster_count == 1 &&
			            renderer.GetStats().texture_upload_count == 1,
			            "different solid path colours and opacities share one raster/upload");
			ok &= Check(matches(3, 7, expected(255, 128.0 / 255), 0, 0) &&
			            matches(11, 7, 0, 0, expected(255, 64.0 / 255)),
			            "shared path mask preserves independent colour/opacity in every target format");

			ImageBuffer edge_buffer(2, 1);
			edge_buffer[0][0] = Pixel(255, 0, 0);
			edge_buffer[0][1] = Pixel(0, 0, 0, 0);
			UiDisplayListBuilder edge_builder;
			edge_builder.DrawImage(Rectf(24, 0, 32, 16), Image(edge_buffer));
			UiDisplayList edge_list;
			ok &= Check(edge_builder.Finish(edge_list) && renderer.Render(edge_list, output) &&
			            device.ReadTexturePixels(target, bytes), "transparent filtering scene renders");
			ok &= Check(matches(27, 8, expected(255, 0.625), 0, 0) &&
			            matches(28, 8, expected(255, 0.375), 0, 0),
			            "bilinear interpolation must filter premultiplied colour without a dark halo");

		}
		ok &= Check(device.DestroyTexture(target) == GpuResult::Ok, "readback target cleans up");
	}
	return ok;
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
			ok &= CheckPixelReadback(device);
			Image image = MakeImage();
			UiDisplayList scene;
			ok &= Check(MakeScene(image, scene), "sampled-image display list should build");
			const String scene_dump = scene.Dump();

			GpuTextureDesc target_desc;
			target_desc.size = Size(64, 64);
			target_desc.format = GpuFormat::RGBA8;
			target_desc.usage = GpuTextureUsage_ColorAttachment | GpuTextureUsage_TransferSrc;
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

				UiDisplayListBuilder crop_builder;
				crop_builder.DrawImage(Rectf(0, 0, 24, 24), image, Rect(1, 0, 2, 2));
				crop_builder.DrawImage(Rectf(24, 0, 48, 24), image, Rect(0, 0, 1, 1), Rgba8(40, 140, 220, 128), true);
				UiDisplayList crop_list; ok &= Check(crop_builder.Finish(crop_list), "Vulkan cropped/masked list");
				ok &= Check(renderer.Render(crop_list, offscreen) && renderer.GetStats().texture_upload_count == 0,
				            "Vulkan crops and mask colours must reuse the existing image texture");
				ok &= Check(renderer.GetStats().batch_count == 2, "Vulkan mask and image pipelines preserve order");
				ok &= Check(renderer.Render(crop_list, offscreen) && renderer.GetStats().texture_upload_count == 0,
				            "Vulkan warm cropped/masked replay must reuse both pipelines and texture");

				Vector<byte> pixels;
				ok &= Check(device.ReadTexturePixels(target, pixels) && pixels.GetCount() == 64 * 64 * 4,
				            "Vulkan crop/mask pixels should read back");
				auto pixel_matches = [&](int x, int y, int r, int g, int b) {
					if(pixels.GetCount() != 64 * 64 * 4) return false;
					const byte *p = pixels.Begin() + 4 * (y * 64 + x);
					return abs((int)p[0] - r) <= 2 && abs((int)p[1] - g) <= 2 &&
					       abs((int)p[2] - b) <= 2 && p[3] == 255;
				};
				ok &= Check(pixel_matches(0, 0, 0, 255, 0) && pixel_matches(23, 0, 0, 255, 0) &&
				            pixel_matches(0, 23, 255, 255, 255),
				            "magnified crop edges must exclude neighbouring red/blue texels");
				ok &= Check(pixel_matches(24, 0, 20, 70, 110) && pixel_matches(47, 23, 20, 70, 110),
				            "alpha mask tint and opacity must produce expected UNORM pixels");
				ok &= Check(pixel_matches(48, 0, 0, 0, 0), "pixels outside image geometry remain clear");
				ok &= Check(renderer.Render(crop_list, offscreen) && device.ReadTexturePixels(target, pixels),
				            "readback must restore the layout for subsequent rendering");

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
				const bool srgb_frame = frame.color_format == GpuFormat::RGBA8Srgb || frame.color_format == GpuFormat::BGRA8Srgb;
				ok &= Check(renderer.GetStats().texture_upload_count == (srgb_frame ? 1 : 0),
				            "swapchain replay must select an image texture matching its actual sampling colour space");
				ok &= Check(device.Present(frame.frame) == GpuResult::Ok,
				            "sampled-image swapchain frame should present through session authority");
				ok &= Check(device.BeginFrame(swapchain, frame) == GpuResult::Ok &&
				            renderer.RenderFrame(scene, frame, clear) && renderer.GetStats().texture_upload_count == 0,
				            "warm swapchain replay should reuse its colour-space-correct image texture");
				ok &= Check(device.Present(frame.frame) == GpuResult::Ok, "warm sampled image frame should present");
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
