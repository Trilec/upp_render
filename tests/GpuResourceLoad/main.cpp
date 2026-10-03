#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderVulkan/RenderVulkanRhi.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include <chrono>
#include <memory>
#include <vector>

using namespace Upp;
using namespace Upp::VulkanTestHooks;

namespace {
Image Pixels(int seed)
{
	ImageBuffer buffer(32, 32);
	for(int y = 0; y < 32; ++y)
		for(int x = 0; x < 32; ++x) {
			RGBA& p = buffer[y][x];
			p.r = (byte)(seed + x * 7); p.g = (byte)(y * 9);
			p.b = (byte)(seed * 3); p.a = 255;
		}
	return Image(buffer);
}

struct Surface {
	HWND hwnd = nullptr;
	std::unique_ptr<VulkanSurfaceSession> session;
	std::unique_ptr<VulkanGpuDevice> device;
	std::unique_ptr<UiRenderer2D> renderer;
	GpuTextureId target;
	~Surface() { Close(); }
	void Close()
	{
		renderer.reset();
		if(device && target.IsValid()) device->DestroyTexture(target);
		target = GpuTextureId();
		device.reset();
		if(session) session->Close();
		session.reset();
		if(hwnd) DestroyWindow(hwnd);
		hwnd = nullptr;
	}
	bool Open(VulkanSurfaceSessionGroup& group)
	{
		hwnd = CreateWindowExW(0, L"STATIC", L"GpuResourceLoad", WS_OVERLAPPED,
		                      0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
		if(!hwnd) return false;
		session.reset(new VulkanSurfaceSession(group));
		GpuNativeWindowDesc native;
		native.kind = GpuNativeWindowKind::Win32;
		native.handle = (uintptr_t)hwnd;
		if(!session->Open(true, native)) { Cout() << session->GetError() << EOL; return false; }
		device.reset(new VulkanGpuDevice(*session));
		if(!device->IsReady()) return false;
		GpuTextureDesc desc;
		desc.size = Size(64, 64);
		desc.format = GpuFormat::RGBA8;
		desc.usage = GpuTextureUsage_ColorAttachment;
		if(device->CreateTexture(desc, target) != GpuResult::Ok) return false;
		renderer.reset(new UiRenderer2D(*device));
		return renderer->IsReady();
	}
	bool Draw(const Image& image)
	{
		UiDisplayListBuilder b;
		b.DrawImage(Rectf(0, 0, 64, 64), image);
		UiDisplayList list;
		if(!b.Finish(list)) return false;
		UiRenderer2DTarget rt;
		rt.color_target = target; rt.size = Size(64, 64); rt.color_format = GpuFormat::RGBA8;
		if(!renderer->Render(list, rt)) { Cout() << renderer->GetError() << EOL; return false; }
		return true;
	}
};

bool RunCase(int count, bool bounded, int frames = 64, bool entry_limited = false)
{
	VulkanSurfaceSessionGroup group;
	std::vector<std::unique_ptr<Surface>> surfaces;
	uint64 peak_allocated = 0;
	int64 peak_payload = 0;
	Vector<double> durations;
	int uploads = 0;
	Image shared = Pixels(1);
	for(int i = 0; i < count; ++i) {
		surfaces.emplace_back(new Surface);
		if(!surfaces.back()->Open(group)) return false;
		UiRenderer2DCacheLimits limits;
		limits.image_bytes = entry_limited ? 64 * 1024 * 1024 : bounded ? 16384 : 128 * 1024 * 1024;
		limits.image_entries = entry_limited ? 4 : 4096;
		surfaces.back()->renderer->SetCacheLimits(limits);
		if(!surfaces.back()->Draw(shared)) return false;
		uploads += surfaces.back()->renderer->GetStats().texture_upload_count;
	}
	Cout() << "cold_identical_surfaces=" << count << " uploads=" << uploads
	       << " shared_native_image_bytes=" << AsString(VulkanGpuDevice::GetSharedImmutableAllocationBytes()) << EOL;
	if(uploads != 1) return false;
	if(count > 0) {
		const auto& info = surfaces[0]->session->GetReport().selected_device;
		Cout() << "gpu=" << info.name << " vendor=" << info.vendor_id
		       << " driver_raw=" << info.driver_version << " api_raw=" << info.api_version << EOL;
	}
	for(int frame = 0; frame < frames; ++frame) {
		auto begin = std::chrono::steady_clock::now();
		for(int i = 0; i < count; ++i)
			if(!surfaces[i]->Draw(Pixels(frame * count + i + 2))) return false;
		uint64 allocated = VulkanGpuDevice::GetSharedImmutableAllocationBytes();
		int64 payload = 0;
		for(auto& surface : surfaces) {
			allocated += surface->device->GetLiveAllocationBytes();
			payload += surface->renderer->GetStats().image_cache_bytes;
			if(bounded && surface->renderer->GetStats().image_cache_bytes > 16384) return false;
		}
		peak_allocated = max(peak_allocated, allocated);
		peak_payload = max(peak_payload, payload);
		durations.Add(std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - begin).count());
	}
	Sort(durations);
	Cout() << "policy=" << (bounded ? "bounded" : "baseline-retain")
	       << " entry_limited=" << (entry_limited ? 1 : 0)
	       << " surfaces=" << count << " frames=" << frames << " image_pixel_peak=" << peak_payload
	       << " native_owned_allocation_peak=" << AsString(peak_allocated)
	       << " cpu_replay_cycle_ms_p50=" << durations[durations.GetCount() / 2]
	       << " p95=" << durations[(durations.GetCount() * 95) / 100]
	       << " p99=" << durations[(durations.GetCount() * 99) / 100] << " max=" << durations.Top() << EOL;
	for(auto& surface : surfaces)
		if(!surface->Draw(shared)) return false;
	// Destroy the first presenter while another retains the same source image.
	surfaces.front()->Close();
	for(int i = 1; i < count; ++i)
		if(!surfaces[i]->Draw(shared) || surfaces[i]->renderer->GetStats().texture_upload_count != 0) return false;
	for(auto& surface : surfaces) {
		surface->renderer.reset();
		if(surface->device) {
			surface->device->DestroyTexture(surface->target);
			surface->target = GpuTextureId();
			if(surface->device->GetLiveAllocationBytes() != 0 ||
			   surface->device->GetLiveTextureCount() != 0 ||
			   surface->device->GetLiveBufferCount() != 0) return false;
			if(surface->session->GetReport().validation_error_count ||
			   surface->session->GetReport().validation_warning_count) return false;
		}
		surface->Close();
	}
	const auto d = GetVulkanRuntimeDeviceDiagnostics();
	return VulkanGpuDevice::GetSharedImmutableAllocationBytes() == 0 && d.runtime_live_count == 0 && d.instance_live_count == 0 &&
	       d.device_live_count == 0 && d.surface_live_count == 0 && d.swapchain_live_count == 0;
}
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	bool soak = false;
	int cycles = 10;
	for(const String& arg : CommandLine()) {
		if(arg == "--soak") soak = true;
		else if(arg == "--soak-cycles=5") cycles = 5;
		else { Cout() << "Expected --soak [--soak-cycles=5]" << EOL; SetExitCode(2); return; }
	}
	if(soak) {
		for(int cycle = 0; cycle < cycles && ok; ++cycle) {
			Cout() << "lifecycle_cycle=" << cycle << EOL;
			ok = RunCase(10, true, 4096, true);
		}
	}
	else for(bool bounded : { false, true })
		for(int count : { 1, 2, 10 })
			ok = RunCase(count, bounded) && ok;
	Cout() << (ok ? "GpuResourceLoad passed; final ownership ZERO" : "GpuResourceLoad FAILED") << EOL;
	if(!ok) SetExitCode(1);
}
