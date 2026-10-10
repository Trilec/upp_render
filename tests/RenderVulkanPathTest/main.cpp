#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderGpu2D/RenderGpu2DPath.h>
#include <RenderVulkan/RenderVulkanRhi.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include <cmath>
#include <limits>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Upp;

static bool Check(bool condition, const char *message)
{
	if(!condition) Cout() << "FAIL: " << message << EOL;
	return condition;
}
static FARPROC WINAPI Resolver(HMODULE, LPCSTR) { return reinterpret_cast<FARPROC>(1); }

static UiPath Circle(double radius, bool reverse = false)
{
	const double k = radius * 0.5522847498307936;
	UiPath path;
	if(!reverse) {
		path.MoveTo(Pointf(radius, 0)).CubicTo(Pointf(radius, k), Pointf(k, radius), Pointf(0, radius));
		path.CubicTo(Pointf(-k, radius), Pointf(-radius, k), Pointf(-radius, 0));
		path.CubicTo(Pointf(-radius, -k), Pointf(-k, -radius), Pointf(0, -radius));
		path.CubicTo(Pointf(k, -radius), Pointf(radius, -k), Pointf(radius, 0)).Close();
	}
	else {
		path.MoveTo(Pointf(radius, 0)).CubicTo(Pointf(radius, -k), Pointf(k, -radius), Pointf(0, -radius));
		path.CubicTo(Pointf(-k, -radius), Pointf(-radius, -k), Pointf(-radius, 0));
		path.CubicTo(Pointf(-radius, k), Pointf(-k, radius), Pointf(0, radius));
		path.CubicTo(Pointf(k, radius), Pointf(radius, k), Pointf(radius, 0)).Close();
	}
	return path;
}

static bool CheckPreparation()
{
	bool ok = true;
	GpuPathMesh mesh;
	UiDisplayOp op;
	op.type = UiDisplayOpType::FillPath;
	op.path = Circle(16);
	op.paint = UiPaint::Solid(Rgba8(255, 255, 255));
	ok &= Check(PrepareGpuConvexPath(op, Transform2D(), mesh) && !mesh.triangles.IsEmpty(),
	            "closed cubic circle becomes coverage triangles");
	op.path.MoveTo(Pointf(1, 1)).LineTo(Pointf(2, 1)).LineTo(Pointf(2, 2)).Close();
	ok &= Check(!PrepareGpuConvexPath(op, Transform2D(), mesh) && mesh.triangles.IsEmpty(),
	            "compound paths retain the reference path");
	op.path.Clear();
	for(int i = 0; i < 5; ++i) {
		double angle = i * 4 * M_PI / 5;
		Pointf p(16 * cos(angle), 16 * sin(angle));
		if(i == 0) op.path.MoveTo(p); else op.path.LineTo(p);
	}
	op.path.Close();
	ok &= Check(!PrepareGpuConvexPath(op, Transform2D(), mesh),
	            "self-crossing pentagram is not misclassified as convex");
	op.path.Clear();
	op.path.MoveTo(Pointf(0, 0)).LineTo(Pointf(16, 0)).LineTo(Pointf(8, 6))
	       .LineTo(Pointf(16, 16)).LineTo(Pointf(0, 16)).Close();
	ok &= Check(!PrepareGpuConvexPath(op, Transform2D(), mesh), "concave contour retains reference");
	op.path = Circle(16);
	op.type = UiDisplayOpType::StrokePath;
	op.stroke.width = 1;
	ok &= Check(PrepareGpuConvexPath(op, Transform2D(), mesh), "one-pixel closed stroke becomes geometry");
	op.stroke.dash << 2.0 << 2.0;
	ok &= Check(!PrepareGpuConvexPath(op, Transform2D(), mesh), "dashed stroke retains reference");
	op.stroke.dash.Clear();
	Transform2D shear; shear.x.y = 0.4;
	ok &= Check(!PrepareGpuConvexPath(op, shear, mesh), "anisotropic stroke retains reference");
	op.type = UiDisplayOpType::FillPath;
	ok &= Check(PrepareGpuConvexPath(op, shear, mesh), "affine fill accepts device-space flattening");
	Transform2D bad; bad.t.x = std::numeric_limits<double>::infinity();
	ok &= Check(!PrepareGpuConvexPath(op, bad, mesh), "nonfinite transform cannot generate GPU vertices");

	op.type = UiDisplayOpType::StrokePath;
	op.path.Clear(); op.path.MoveTo(Pointf(4, 4)).LineTo(Pointf(40, 40));
	op.stroke.width = 1;
	ok &= Check(PrepareGpuConvexPath(op, Transform2D(), mesh), "butt straight stroke becomes GPU geometry");
	op.stroke.cap = UiLineCap::Square;
	ok &= Check(PrepareGpuConvexPath(op, Transform2D(), mesh), "square straight stroke becomes GPU geometry");
	op.stroke.cap = UiLineCap::Round;
	ok &= Check(!PrepareGpuConvexPath(op, Transform2D(), mesh), "round cap retains reference");
	op.stroke.cap = UiLineCap::Butt;
	op.stroke.width = 0.5;
	ok &= Check(!PrepareGpuConvexPath(op, Transform2D(), mesh), "subpixel stroke retains reference");
	op.stroke.width = 2;
	ok &= Check(!PrepareGpuConvexPath(op, shear, mesh), "anisotropic straight stroke retains reference");
	op.stroke.dash << 2.0 << 2.0;
	ok &= Check(!PrepareGpuConvexPath(op, Transform2D(), mesh), "dashed straight stroke retains reference");
	return ok;
}

static bool CheckPixels(VulkanGpuDevice& device)
{
	bool ok = true;
	const Size size(64, 64);
	const GpuFormat formats[] = { GpuFormat::RGBA8, GpuFormat::BGRA8,
	                              GpuFormat::RGBA8Srgb, GpuFormat::BGRA8Srgb };
	for(GpuFormat format : formats) {
		GpuTextureDesc desc;
		desc.size = size; desc.format = format;
		desc.usage = GpuTextureUsage_ColorAttachment | GpuTextureUsage_TransferSrc;
		GpuTextureId target;
		if(!Check(device.CreateTexture(desc, target) == GpuResult::Ok, "AA target creates")) return false;
		{
			UiRenderer2D renderer(device);
			UiRenderer2DCacheLimits limits;
			limits.vector_bytes = 0; limits.vector_entries = 0;
			limits.image_bytes = 0; limits.image_entries = 0;
			renderer.SetCacheLimits(limits);
			UiRenderer2DTarget output;
			output.color_target = target; output.size = size; output.color_format = format;
			output.clear_color.alpha = 1;
			for(int case_index = 0; case_index < 16; ++case_index) {
				bool stroke = (case_index & 7) >= 4;
				bool reverse = (case_index & 1) != 0;
				double radius = (case_index & 2) ? 3 : 16;
				double opacity = reverse ? 128.0 / 255.0 : 1;
				double stroke_width = radius == 3 ? 1 : 2;
				Pointf centre = case_index < 8 ? Pointf(31.25, 30.75) : Pointf(32, 32);
				UiDisplayListBuilder builder;
				builder.Save(); builder.ClipRect(Rectf(10, 8, 49, 51));
				builder.ConcatTransform(Transform2D::Translation(centre.x, centre.y));
				UiPaint paint = UiPaint::Solid(Rgba8(255, 0, 0, reverse ? 128 : 255));
				if(stroke) { UiStrokeStyle style; style.width = stroke_width;
					builder.StrokePath(Circle(radius, reverse), paint, style); }
				else builder.FillPath(Circle(radius, reverse), paint);
				builder.Restore();
				UiDisplayList list; builder.Finish(list);
				Vector<byte> bytes;
				bool rendered = renderer.Render(list, output);
				if(!rendered) Cout() << renderer.GetError() << EOL;
				ok &= Check(rendered && device.ReadTexturePixels(target, bytes), "direct path renders/readbacks");
				const auto stats = renderer.GetStats();
				ok &= Check(stats.gpu_path_count == 1 && stats.vector_raster_count == 0 &&
				            stats.texture_upload_count == 0 && stats.image_cache_bytes == 0 &&
				            stats.vector_cache_bytes == 0, "direct path needs no raster/image budget");
				ok &= Check(stats.gpu_path_coverage_draw_count == (case_index >= 8 ? 1 : 0),
				            "integer placement uses GPU coverage; fractional placement stays direct");
				if(case_index >= 8) {
					ok &= Check(renderer.Render(list, output) &&
					            renderer.GetStats().gpu_path_coverage_render_count == 0 &&
					            renderer.GetStats().gpu_path_coverage_draw_count == 1 &&
					            renderer.GetStats().texture_upload_count == 0,
					            "warm GPU coverage needs neither rerasterization nor pixel upload");
				}
				if(bytes.GetCount() != size.cx * size.cy * 4) { ok = false; continue; }
				bool srgb = format == GpuFormat::RGBA8Srgb || format == GpuFormat::BGRA8Srgb;
				bool bgra = format == GpuFormat::BGRA8 || format == GpuFormat::BGRA8Srgb;
				double worst = 0, total = 0;
				int fractional = 0;
				for(int y = 0; y < size.cy; ++y)
					for(int x = 0; x < size.cx; ++x) {
						int covered = 0;
						// Independent box-filtered analytic circle/ring reference.
						for(int sy = 0; sy < 16; ++sy) for(int sx = 0; sx < 16; ++sx) {
							double px = x + (sx + 0.5) / 16, py = y + (sy + 0.5) / 16;
							if(px < 10 || px >= 49 || py < 8 || py >= 51) continue;
							double distance = hypot(px - centre.x, py - centre.y);
							covered += stroke ? fabs(distance - radius) <= stroke_width / 2 : distance <= radius;
						}
						const byte *pixel = bytes.Begin() + 4 * (y * size.cx + x);
						double actual = pixel[bgra ? 2 : 0] / 255.0;
						if(srgb) actual = actual <= 0.04045 ? actual / 12.92 : pow((actual + 0.055) / 1.055, 2.4);
						double error = fabs(actual - covered * opacity / 256);
						worst = max(worst, error); total += error;
						fractional += actual > 0.01 && actual < opacity - 0.01;
						ok &= pixel[1] == 0 && pixel[bgra ? 0 : 2] == 0;
					}
				Cout() << DumpGpuFormat(format) << " case=" << case_index
				       << Format(" max_coverage_error=%.4f mean=%.6f", worst, total / (size.cx * size.cy)) << EOL;
				ok &= Check(worst <= 0.30 && total / (size.cx * size.cy) <= 0.006 && fractional > 0,
				            "AA coverage agrees with independent analytic reference envelope");
			}
			UiDisplayListBuilder order;
			order.Save(); order.ConcatTransform(Transform2D::Translation(32, 32));
			order.FillPath(Circle(16), UiPaint::Solid(Rgba8(0, 0, 255)));
			order.Restore(); order.FillRect(Rectf(20, 20, 44, 44), Rgba8(255, 0, 0));
			UiDisplayList list; order.Finish(list);
			Vector<byte> bytes;
			ok &= Check(renderer.Render(list, output) && device.ReadTexturePixels(target, bytes),
			            "ordered curve/rectangle renders");
			if(bytes.GetCount() == size.cx * size.cy * 4) {
				const byte *p = bytes.Begin() + 4 * (32 * size.cx + 32);
				bool bgra = format == GpuFormat::BGRA8 || format == GpuFormat::BGRA8Srgb;
				ok &= Check(p[bgra ? 2 : 0] == 255 && p[bgra ? 0 : 2] == 0,
				            "later solid remains above direct path in painter order");
			}


			// Translation and color change must reuse the same device-space mesh.
			UiDisplayListBuilder warm_builder;
			warm_builder.ConcatTransform(Transform2D::Translation(29.5, 31.25));
			warm_builder.FillPath(Circle(16), UiPaint::Solid(Rgba8(255, 120, 0, 128)));
			UiDisplayList warm; warm_builder.Finish(warm);
			ok &= Check(renderer.Render(warm, output) &&
			            renderer.GetStats().gpu_path_cache_miss_count == 0,
			            "warm translated/recolored curve reuses geometry");
			// Independent 16x16 area samples for butt/square straight strokes.
			// No CPU vector/image budget or coverage atlas is available to this path.
			limits.path_coverage_bytes = 0;
			renderer.SetCacheLimits(limits);
			for(int line_case = 0; line_case < 8; ++line_case) {
				const Pointf start(11.25, 12.75);
				const Pointf end = (line_case & 1) ? Pointf(49.25, 47.75) : Pointf(50.25, 12.75);
				const double width = (line_case & 2) ? 3 : 1;
				const bool square = (line_case & 4) != 0;
				const Pointf delta = end - start;
				const double length = sqrt(delta.x * delta.x + delta.y * delta.y);
				const Pointf direction = delta / length;
				const double extension = square ? width * 0.5 : 0;
				UiPath path; path.MoveTo(start).LineTo(end);
				UiStrokeStyle stroke; stroke.width = width;
				stroke.cap = square ? UiLineCap::Square : UiLineCap::Butt;
				UiDisplayListBuilder builder;
				builder.StrokePath(path, UiPaint::Solid(Rgba8(255, 0, 0)), stroke);
				UiDisplayList list; builder.Finish(list);
				Vector<byte> bytes;
				bool rendered = renderer.Render(list, output) && device.ReadTexturePixels(target, bytes);
				ok &= Check(rendered && bytes.GetCount() == 64 * 64 * 4, "straight stroke renders/readbacks");
				ok &= Check(renderer.GetStats().gpu_path_count == 1 &&
				            renderer.GetStats().vector_raster_count == 0 &&
				            renderer.GetStats().texture_upload_count == 0 &&
				            renderer.GetStats().gpu_path_coverage_bytes == 0,
				            "straight stroke uses direct GPU coverage without raster/atlas allocation");
				if(rendered && bytes.GetCount() == 64 * 64 * 4) {
					double maximum = 0, sum = 0;
					bool srgb = format == GpuFormat::RGBA8Srgb || format == GpuFormat::BGRA8Srgb;
					bool bgra = format == GpuFormat::BGRA8 || format == GpuFormat::BGRA8Srgb;
					for(int y = 0; y < 64; ++y)
						for(int x = 0; x < 64; ++x) {
							int inside = 0;
							for(int sy = 0; sy < 16; ++sy)
								for(int sx = 0; sx < 16; ++sx) {
									Pointf p(x + (sx + 0.5) / 16, y + (sy + 0.5) / 16);
									Pointf relative = p - start;
									double along = relative.x * direction.x + relative.y * direction.y;
									double across = fabs(relative.x * direction.y - relative.y * direction.x);
									inside += along >= -extension && along <= length + extension && across <= width * 0.5;
								}
							double actual = bytes[4 * (y * 64 + x) + (bgra ? 2 : 0)] / 255.0;
							if(srgb) actual = actual <= 0.04045 ? actual / 12.92 : pow((actual + 0.055) / 1.055, 2.4);
							double error = fabs(actual - inside / 256.0);
							maximum = max(maximum, error); sum += error;
						}
					Cout() << "line_case=" << line_case << " max_coverage_error=" << maximum
					       << " mean=" << sum / 4096 << EOL;
					ok &= Check(maximum <= 0.30 && sum / 4096 <= 0.006,
					            "straight stroke AA meets unchanged analytic coverage thresholds");
				}
			}

			limits.path_geometry_entries = 2; limits.path_geometry_bytes = 32768;
			renderer.SetCacheLimits(limits);
			for(int i = 0; i < 12; ++i) {
				UiDisplayListBuilder churn_builder;
				churn_builder.ConcatTransform(Transform2D::Translation(32, 32));
				churn_builder.FillPath(Circle(6 + i), UiPaint::Solid(Rgba8(255, 255, 255)));
				UiDisplayList churn; churn_builder.Finish(churn);
				ok &= Check(renderer.Render(churn, output) &&
				            renderer.GetStats().gpu_path_cache_entry_count <= 2 &&
				            renderer.GetStats().gpu_path_cache_bytes <= 32768 &&
				            renderer.GetStats().vector_raster_count == 0,
				            "geometry cache churn stays within entry/byte limits");
			}
			limits.path_geometry_entries = 0; limits.path_geometry_bytes = 0;
			renderer.SetCacheLimits(limits);
			ok &= Check(renderer.Render(warm, output) &&
			            renderer.GetStats().gpu_path_count == 1 &&
			            renderer.GetStats().gpu_path_cache_entry_count == 0 &&
			            renderer.GetStats().texture_upload_count == 0,
			            "zero geometry-cache budget still permits bounded direct drawing");
			UiDisplayListBuilder quad_builder;
			quad_builder.FillRect(Rectf(5, 5, 25, 25), Rgba8(255, 0, 0));
			UiDisplayList quad; quad_builder.Finish(quad);
			limits.geometry_bytes = 143; renderer.SetCacheLimits(limits);
			ok &= Check(!renderer.Render(quad, output) &&
			            renderer.GetError().Find("geometry") >= 0,
			            "oversized active vertex frame fails explicitly");
			limits.geometry_bytes = 144; renderer.SetCacheLimits(limits);
			ok &= Check(renderer.Render(quad, output),
			            "renderer recovers with an exact six-vertex budget");
		}
		// Implicit destruction must close the GPU coverage atlas and its child renderer.
		// Final device teardown alone can hide resources retained until that point.
		{
			UiRenderer2D implicit(device);
			UiDisplayListBuilder builder;
			builder.ConcatTransform(Transform2D::Translation(32, 32));
			builder.FillPath(Circle(10), UiPaint::Solid(Rgba8(255, 255, 255)));
			UiDisplayList list; builder.Finish(list);
			UiRenderer2DTarget output; output.color_target = target; output.size = size;
			output.color_format = format; output.clear_color.alpha = 1;
			ok &= Check(implicit.Render(list, output) && implicit.GetStats().gpu_path_coverage_bytes > 0,
			            "implicit-lifetime case creates GPU coverage resources");
		}
		ok &= Check(device.GetLiveTextureCount() == 1 && device.GetLiveBufferCount() == 0 &&
		            device.GetLivePipelineCount() == 0 && device.GetLiveShaderCount() == 0 &&
		            device.GetLiveCommandCount() == 0,
		            "renderer destructor releases coverage resources before device shutdown");
		ok &= Check(device.DestroyTexture(target) == GpuResult::Ok, "AA target destroys");
	}
	return ok;
}

CONSOLE_APP_MAIN
{
	bool ok = CheckPreparation();
	VulkanTestHooks::ClearVulkanRuntimeDeviceDiagnostics();
	HWND window = CreateWindowExW(0, L"STATIC", L"RenderVulkanPathTest", WS_POPUP,
	                              0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	ok &= Check(window != nullptr, "hidden window creates");
	VulkanSurfaceSession session;
	GpuNativeWindowDesc native; native.kind = GpuNativeWindowKind::Win32; native.handle = (uintptr_t)window;
	if(window && Check(session.Open(true, native, &Resolver), "validation session opens")) {
		{
			VulkanGpuDevice device(session);
			ok &= CheckPixels(device);
			ok &= Check(device.GetLiveTextureCount() == 0 && device.GetLiveBufferCount() == 0 &&
			            device.GetLiveCommandCount() == 0 && device.GetLivePipelineCount() == 0 &&
			            device.GetLiveShaderCount() == 0, "all adapter resources released");
		}
	}
	else ok = false;
	session.Close();
	ok &= Check(session.GetReport().validation_warning_count == 0 &&
	            session.GetReport().validation_error_count == 0, "zero validation diagnostics");
	if(window) DestroyWindow(window);
	const auto diag = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
	ok &= Check(diag.runtime_live_count == 0 && diag.instance_live_count == 0 &&
	            diag.device_live_count == 0 && diag.surface_live_count == 0 &&
	            diag.swapchain_live_count == 0, "final Vulkan ownership ZERO");
	if(ok) Cout() << "RenderVulkanPathTest passed" << EOL;
	else SetExitCode(1);
}
