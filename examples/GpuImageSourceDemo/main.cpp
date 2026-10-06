#include <RenderVulkan/RenderVulkanRhi.h>
#include <cstring>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
using namespace Upp;

// Minimal source-resource caller. The embedded colour image pass is a later gate;
// this sample intentionally proves raw float upload/readback, not playback speed.
CONSOLE_APP_MAIN
{
	HWND window = CreateWindowExW(0, L"STATIC", L"GpuImageSourceDemo", WS_POPUP,
	                              0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	VulkanSurfaceSession session;
	GpuNativeWindowDesc native;
	native.kind = GpuNativeWindowKind::Win32; native.handle = (uintptr_t)window;
	bool ok = window && session.Open(false, native);
	if(ok) {
		VulkanGpuDevice device(session);
		GpuDevice& renderer = device;
		GpuTextureDesc texture;
		texture.size = Size(2, 1); texture.format = GpuFormat::RGBA32F;
		texture.usage = GpuTextureUsage_Sampled | GpuTextureUsage_TransferDst | GpuTextureUsage_TransferSrc;
		GpuTextureCapabilities caps;
		ok = renderer.GetTextureCapabilities(texture.format, texture.usage, caps) == GpuResult::Ok;
		if(ok) Cout() << "RGBA32F sampled=" << caps.sampled << " linear=" << caps.linear_filter
		              << " max=" << caps.max_size << EOL;
		GpuTextureId source;
		const float pixels[] = { -2, 8, 0.5f, 0.25f, 4, -1, 16, 1 };
		GpuTextureWriteDesc write; write.size = texture.size; write.row_pitch = 4 * sizeof(float) * 2;
		Vector<byte> returned;
		ok = ok && renderer.CreateTexture(texture, source) == GpuResult::Ok &&
		     renderer.WriteTexture(source, write, pixels, sizeof(pixels)) == GpuResult::Ok &&
		     device.ReadTexturePixels(source, returned) && returned.GetCount() == sizeof(pixels) &&
		     std::memcmp(returned.Begin(), pixels, sizeof(pixels)) == 0;
		if(source.IsValid()) ok = renderer.DestroyTexture(source) == GpuResult::Ok && ok;
		if(!ok) Cout() << renderer.GetLastError() << EOL;
	}
	session.Close();
	if(window) DestroyWindow(window);
	Cout() << (ok ? "Raw HDR source preserved; no colour conversion or premultiplication" : "Source-resource check failed") << EOL;
	if(!ok) SetExitCode(1);
}
