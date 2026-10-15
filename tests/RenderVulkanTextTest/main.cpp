#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderVulkan/RenderVulkanRhi.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include <Painter/Painter.h>
#include <cmath>
#ifdef flagCFONTS
#include <RenderFontWin32/RenderFontWin32.h>
#endif

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


static bool CheckTextPixels(VulkanGpuDevice& device)
{
	bool ok = true;
	const Size size(320, 72);
	WString text;
	for(int cp : {0x41, 0x42, 0x00e9, 0x03a9, 0x0416, 0x4e2d, 0x1f600}) text.Cat(cp);
	Font font = SansSerif(24).Bold().Italic();
	ImagePainter painter(size);
	RGBA black{0, 0, 0, 255};
	painter.Clear(black);
	// The neutral U++ control contract uses integer advances. Painter's implicit
	// Text() spacing uses a larger font for fractional advances, so provide dx.
	Vector<int> advances;
	for(int cp : text) advances.Add(font.GetWidth(cp));
	painter.DrawText(4, 4, text, font, White(), advances.Begin());
	Image reference = painter.GetResult();
	for(GpuFormat format : {GpuFormat::RGBA8, GpuFormat::BGRA8,
	                       GpuFormat::RGBA8Srgb, GpuFormat::BGRA8Srgb}) {
		GpuTextureDesc desc;
		desc.size = size; desc.format = format;
		desc.usage = GpuTextureUsage_ColorAttachment | GpuTextureUsage_TransferSrc;
		GpuTextureId target;
		if(!Check(device.CreateTexture(desc, target) == GpuResult::Ok, "Unicode pixel target creates"))
			return false;
		{
			UiRenderer2D renderer(device);
			UiRenderer2DTarget output;
			output.color_target = target; output.size = size; output.color_format = format;
			output.clear_color.alpha = 1;
			UiDisplayListBuilder builder;
			builder.DrawText(Pointf(4, 4), text, font, Rgba8(255, 255, 255, 255));
			UiDisplayList list; builder.Finish(list);
			Vector<byte> bytes;
			bool read = renderer.Render(list, output) && device.ReadTexturePixels(target, bytes);
			ok &= Check(read && bytes.GetCount() == size.cx * size.cy * 4, "Unicode pixels render/read back");
			ok &= Check(renderer.GetStats().gpu_glyph_raster_count > 0 &&
			            renderer.GetStats().cpu_glyph_raster_count == 0 && renderer.GetStats().texture_upload_count == 0,
			            "Vulkan generates cold glyph coverage on the GPU without CPU raster uploads");
			if(read && bytes.GetCount() == size.cx * size.cy * 4) {
				double max_error = 0, sum = 0;
				int covered = 0, antialiased = 0;
				const bool srgb = format == GpuFormat::RGBA8Srgb || format == GpuFormat::BGRA8Srgb;
				for(int y = 0; y < size.cy; ++y)
					for(int x = 0; x < size.cx; ++x) {
						const byte *p = bytes.Begin() + 4 * (y * size.cx + x);
						double actual = p[0] / 255.0;
						if(srgb) actual = actual <= 0.04045 ? actual / 12.92 : pow((actual + 0.055) / 1.055, 2.4);
						const double error = fabs(actual - reference[y][x].r / 255.0);
						max_error = max(max_error, error); sum += error;
						covered += p[0] > 0;
						antialiased += p[0] > 0 && p[0] < 255;
					}
				Cout() << "Unicode pixel format=" << (int)format << " max_error=" << max_error
				       << " mean_error=" << sum / (size.cx * size.cy) << " aa_pixels=" << antialiased << EOL;
				ok &= Check(covered > 100 && antialiased > 20, "visible Unicode text has antialiased edge pixels");
				ok &= Check(max_error <= 0.03 && sum / (size.cx * size.cy) <= 0.001,
				            "aligned Unicode mask pixels match the independent Painter reference");
			}
			UiDisplayListBuilder decorated;
			Font decorated_font = font;
			decorated.DrawText(Pointf(4, 4), text, decorated_font.Underline().Strikeout(), Rgba8(255, 255, 255, 255));
			UiDisplayList decorations; decorated.Finish(decorations);
			Vector<byte> decorated_bytes;
			ok &= Check(renderer.Render(decorations, output) && device.ReadTexturePixels(target, decorated_bytes),
			            "text decorations render/read back");
			ok &= Check(renderer.GetStats().glyph_cache_miss_count == 0 &&
			            renderer.GetStats().glyph_atlas_upload_count == 0,
			            "decorations reuse undecorated glyph atlas entries");
			ok &= Check(bytes != decorated_bytes, "underline and strikeout change actual GPU pixels");
		}
		ok &= Check(device.DestroyTexture(target) == GpuResult::Ok, "Unicode target cleanup");
	}
	return ok;
}



// Independent double-precision point-in-polygon raster reference for the
// few pixels where Painter's legacy area accumulator differs at crossings.
struct GlyphCoverageOracle : LinearPathConsumer {
	Vector<Pointf> starts, ends; Pointf at, first; bool begun = false;
	Rectf bounds = Rectf(1e100,1e100,-1e100,-1e100);
	void Finish() { if(begun && at != first) { starts.Add(at); ends.Add(first); } }
	void Bound(Pointf p) { bounds.left=min(bounds.left,p.x); bounds.top=min(bounds.top,p.y); bounds.right=max(bounds.right,p.x); bounds.bottom=max(bounds.bottom,p.y); }
	void Move(const Pointf& p) override { Finish(); at=first=p; begun=true; Bound(p); }
	void Line(const Pointf& p) override { if(begun && p!=at) { starts.Add(at); ends.Add(p); } at=p; Bound(p); }
	double Sample(Point pixel) const {
		if(bounds.right<=pixel.x || bounds.left>=pixel.x+1 || bounds.bottom<=pixel.y || bounds.top>=pixel.y+1) return 0;
		double coverage=0;
		for(int y=0;y<128;++y) for(int x=0;x<128;++x) {
			Pointf q(pixel.x+(x+.5)/128,pixel.y+(y+.5)/128); bool inside=false;
			for(int i=0;i<starts.GetCount();++i) {
				Pointf a=starts[i],b=ends[i];
				if((a.y>q.y)!=(b.y>q.y) && a.x+(q.y-a.y)*(b.x-a.x)/(b.y-a.y)>q.x) inside=!inside;
			}
			coverage+=inside?1.0/(128*128):0;
		}
		return coverage;
	}
};

static bool CheckGlyphBatching(VulkanGpuDevice& device)
{
	bool ok = true;
	const Size size(620, 190);
	GpuTextureDesc desc; desc.size = size; desc.format = GpuFormat::RGBA8;
	desc.usage = GpuTextureUsage_ColorAttachment | GpuTextureUsage_TransferSrc;
	GpuTextureId target;
	if(!Check(device.CreateTexture(desc, target) == GpuResult::Ok, "glyph batching target creates")) return false;
	{
		UiRenderer2D renderer(device); UiRenderer2DTarget output;
		output.color_target = target; output.size = size; output.color_format = desc.format;
		output.clear_color.alpha = 1;
		UiDisplayListBuilder builder; ImagePainter painter(size); painter.Clear(RGBA{0,0,0,255});
		Font font = SansSerif(18);
		for(int line = 0; line < 8; ++line) {
			WString text; Vector<int> advances;
			for(int i = 0; i < 28; ++i) {
				int cp = 32 + line * 28 + i; if(cp >= 127) cp += 33;
				text.Cat(cp); advances.Add(max(0, font.GetWidth(cp)));
			}
			builder.DrawText(Pointf(3, 3 + line * 23), text, font, Rgba8(255,255,255));
			int pen = 3;
			for(int i = 0; i < text.GetCount(); ++i) {
				WString one; one.Cat(text[i]);
				painter.DrawText(pen, 3 + line * 23, one, font, White());
				pen += advances[i];
			}
		}
		UiDisplayList list; builder.Finish(list);
		ok &= Check(renderer.Render(list, output), "more than one cold glyph batch renders");
		const auto first = renderer.GetStats();
		Cout() << "glyph_batch_gpu_rasters=" << first.gpu_glyph_raster_count
		       << " compute_submissions=" << first.gpu_vector_submit_count << EOL;
		ok &= Check(first.gpu_glyph_raster_count > 128 && first.cpu_glyph_raster_count == 0 &&
		            first.gpu_vector_submit_count >= 3 && first.gpu_vector_submit_count <= 6,
		            "cold glyphs use bounded multi-dispatch batches");
		Vector<byte> bytes; bool read = device.ReadTexturePixels(target, bytes);
		ok &= Check(read && bytes.GetCount() == size.cx*size.cy*4, "batched glyph pixels read back");
		if(read && bytes.GetCount() == size.cx*size.cy*4) {
			Image reference = painter.GetResult(); double maximum = 0, total = 0; Point worst(0,0);
			for(int y = 0; y < size.cy; ++y) for(int x = 0; x < size.cx; ++x) {
				double delta = abs((int)bytes[4*(y*size.cx+x)] - (int)reference[y][x].r)/255.0;
				if(delta > maximum) { maximum = delta; worst = Point(x,y); } total += delta/(size.cx*size.cy);
			}
			Cout() << "glyph_batch_max_error=" << maximum << " mean_error=" << total
			       << " worst=" << worst << " actual=" << (int)bytes[4*(worst.y*size.cx+worst.x)] << " reference=" << (int)reference[worst.y][worst.x].r << EOL;
			Array<GlyphCoverageOracle> oracle;
			for(int line=0;line<8;++line) {
				int pen=3;
				for(int i=0;i<28;++i) {
					int cp=32+line*28+i; if(cp>=127) cp+=33;
					auto& shape=oracle.Add(); ApproximateChar(shape,Point(pen,3+line*23),cp,font,.3); shape.Finish();
					pen+=max(0,font.GetWidth(cp));
				}
			}
			int outliers=0; double oracle_error=0;
			for(int y=0;y<size.cy;++y) for(int x=0;x<size.cx;++x)
				if(abs((int)bytes[4*(y*size.cx+x)]-(int)reference[y][x].r)>8) {
					double expected=0;
					for(const auto& shape:oracle) { double a=shape.Sample(Point(x,y)); expected=a+expected*(1-a); }
					oracle_error=max(oracle_error,fabs(bytes[4*(y*size.cx+x)]/255.0-expected)); ++outliers;
				}
			Cout()<<"glyph_legacy_crossing_pixels="<<outliers<<" independent_max_error="<<oracle_error<<EOL;
			ok &= Check(maximum<=.25 && total<=.001 && oracle_error<=.02,
			            "batched text preserves reference pixels and independent crossing coverage");
		}
		ok &= Check(renderer.Render(list, output) && !renderer.GetStats().gpu_vector_submit_count &&
		            !renderer.GetStats().gpu_glyph_raster_count, "warm batched text has no compute submissions");
		Vector<byte> warm_pixels; ok &= Check(device.ReadTexturePixels(target, warm_pixels) && warm_pixels == bytes,
		                                    "cold and warm text preserve whitespace advances exactly");
		UiRenderer2DCacheLimits limits = renderer.GetCacheLimits(); limits.glyph_entries = 1; renderer.SetCacheLimits(limits);
		ok &= Check(!renderer.Render(list, output) && !renderer.GetError().IsEmpty(),
		            "failed frame discards queued glyph writes explicitly");
		limits.glyph_entries = 8192; renderer.SetCacheLimits(limits);
		ok &= Check(renderer.Render(list, output) && renderer.GetStats().cpu_glyph_raster_count == 0,
		            "glyph cache recovers after an interrupted batch");
	}
	ok &= Check(device.DestroyTexture(target) == GpuResult::Ok, "glyph batching target retires");
	return ok;
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
			ok &= CheckTextPixels(device);
			ok &= CheckGlyphBatching(device);
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
				ok &= Check(first.gpu_glyph_raster_count == 2 && first.cpu_glyph_raster_count == 0,
				            "cold glyph regions use GPU coverage only");
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
				ok &= Check(second.gpu_glyph_raster_count == 0 && second.cpu_glyph_raster_count == 0,
				            "warm text performs no glyph coverage work");
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

#ifdef flagCFONTS
	const auto font_stats = GetRenderFontWin32Stats();
	ok &= Check(font_stats.legacy_gdi_font_requests == 0 && font_stats.error.IsEmpty(),
	            "DirectWrite Vulkan text does not invoke legacy GDI fonts");
	Cout() << "font_backend=DirectWrite legacy_gdi_font_requests=" << font_stats.legacy_gdi_font_requests << EOL;
#endif
	if(ok) {
		Cout() << "RenderVulkanTextTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
