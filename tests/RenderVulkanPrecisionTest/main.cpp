#include <RenderVulkan/RenderVulkanRhi.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include <RenderNull/RenderNull.h>
#include <cstring>
#include <limits>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Upp;

static FARPROC WINAPI Resolver(HMODULE, LPCSTR) { return reinterpret_cast<FARPROC>(1); }
static bool Check(bool condition, const char *message)
{
	if(!condition) Cout() << "FAIL: " << message << EOL;
	return condition;
}

static void FillSource(GpuFormat format, Vector<byte>& bytes)
{
	const uint16 half[] = { 0xc000, 0x0001, 0x7bff, 0x3400, 0x8000, 0x7c00, 0xfc00, 0x7e01 };
	const uint16 unorm[] = { 1, 0x1234, 0xffff, 0x4000, 0x8001, 0, 0x2345, 0xfffe };
	const uint32 floats[] = { 0xc0800000, 0x41800000, 0x3f000000, 0x3e800000,
	                          0x80000000, 0x7f800000, 0xff800000, 0x7fc01234 };
	if(format == GpuFormat::RGBA16 || format == GpuFormat::RGBA16F || format == GpuFormat::R16F) {
		const uint16 *values = format == GpuFormat::RGBA16 ? unorm : half;
		for(int i = 0; i < bytes.GetCount() / 2; ++i) std::memcpy(bytes.Begin() + i * 2, &values[i % 8], 2);
	}
	else if(format == GpuFormat::RGBA32F || format == GpuFormat::R32F) {
		for(int i = 0; i < bytes.GetCount() / 4; ++i) std::memcpy(bytes.Begin() + i * 4, &floats[i % 8], 4);
	}
	else
		for(int i = 0; i < bytes.GetCount(); ++i) bytes[i] = (byte)(i * 37 + 3);
}

static bool TestFormat(VulkanGpuDevice& device, GpuFormat format)
{
	const int bpp = GpuFormatBytesPerPixel(format);
	GpuTextureDesc desc;
	desc.size = Size(3, 2); desc.format = format;
	desc.usage = GpuTextureUsage_Sampled | GpuTextureUsage_TransferDst | GpuTextureUsage_TransferSrc;
	GpuTextureCapabilities caps;
	if(!Check(device.GetTextureCapabilities(format, desc.usage, caps) == GpuResult::Ok,
	          "required precision format supports sampled upload/readback")) return false;
	bool ok = Check(caps.format == format && caps.bytes_per_pixel == bpp &&
	                caps.sampled && caps.transfer_src && caps.transfer_dst &&
	                caps.max_size.cx >= 3 && caps.max_resource_bytes > 0, "capability fields describe real image support");
	Cout() << DumpGpuFormat(format) << " bytes=" << bpp << " linear=" << caps.linear_filter
	       << " blend=" << caps.color_blend << " max=" << caps.max_size << EOL;
	GpuTextureId texture;
	if(!Check(device.CreateTexture(desc, texture) == GpuResult::Ok, "precision texture creates")) return false;
	Vector<byte> tight;
	tight.SetCount(6 * bpp); FillSource(format, tight);
	const int row = 3 * bpp, pitch = row + 5;
	Vector<byte> padded;
	padded.SetCount(pitch + row, 0xee);
	std::memcpy(padded.Begin(), tight.Begin(), row);
	std::memcpy(padded.Begin() + pitch, tight.Begin() + row, row);
	GpuTextureWriteDesc write;
	write.size = desc.size; write.row_pitch = pitch;
	Vector<byte> read;
	ok &= Check(!device.ReadTexturePixels(texture, read) && read.IsEmpty(), "uninitialized precision content rejected");
	ok &= Check(device.WriteTexture(texture, write, padded.Begin(), padded.GetCount()) == GpuResult::Ok &&
	            device.ReadTexturePixels(texture, read) && read.GetCount() == tight.GetCount() &&
	            std::memcmp(read.Begin(), tight.Begin(), tight.GetCount()) == 0,
	            "padded source preserves native bits, including HDR, signed zero, alpha and NaN/Inf");
	GpuTextureWriteDesc bad = write;
	bad.row_pitch = row - 1;
	ok &= Check(device.WriteTexture(texture, bad, padded.Begin(), padded.GetCount()) == GpuResult::InvalidArgument,
	            "short row pitch rejected");
	bad = write; bad.origin.x = INT_MAX;
	ok &= Check(device.WriteTexture(texture, bad, padded.Begin(), padded.GetCount()) == GpuResult::InvalidArgument,
	            "out-of-bounds origin rejected before pointer access");
	bad = write; bad.row_pitch = std::numeric_limits<int64>::max();
	ok &= Check(device.WriteTexture(texture, bad, padded.Begin(), padded.GetCount()) == GpuResult::InvalidArgument,
	            "overflowing row span rejected");
	ok &= Check(device.WriteTexture(texture, write, padded.Begin(), padded.GetCount() - 1) == GpuResult::InvalidArgument,
	            "short final row rejected");
	ok &= Check(device.ReadTexturePixels(texture, read) &&
	            std::memcmp(read.Begin(), tight.Begin(), tight.GetCount()) == 0, "failed writes retain original content");

	Vector<byte> patch;
	patch.SetCount(2 * bpp); FillSource(format, patch);
	write.origin = Point(2, 0); write.size = Size(1, 2); write.row_pitch = bpp;
	std::memcpy(tight.Begin() + 2 * bpp, patch.Begin(), bpp);
	std::memcpy(tight.Begin() + 5 * bpp, patch.Begin() + bpp, bpp);
	ok &= Check(device.WriteTexture(texture, write, patch.Begin(), patch.GetCount()) == GpuResult::Ok &&
	            device.ReadTexturePixels(texture, read) &&
	            std::memcmp(read.Begin(), tight.Begin(), tight.GetCount()) == 0,
	            "origin/subregion update preserves other texels and restores readable layout");
	ok &= Check(device.DestroyTexture(texture) == GpuResult::Ok, "mutable precision texture released");

	desc.usage = GpuTextureUsage_Sampled | GpuTextureUsage_TransferDst;
	GpuTextureId first, second; bool uploaded = false;
	ok &= Check(device.AcquireImmutableTexture(77, desc, tight.Begin(), tight.GetCount(), first, uploaded) == GpuResult::Ok &&
	            uploaded, "first immutable native-format source uploads");
	const uint64 shared = VulkanGpuDevice::GetSharedImmutableAllocationBytes();
	ok &= Check(device.AcquireImmutableTexture(77, desc, tight.Begin(), tight.GetCount(), second, uploaded) == GpuResult::Ok &&
	            !uploaded && first != second && VulkanGpuDevice::GetSharedImmutableAllocationBytes() == shared,
	            "immutable precision identity shares one allocation and separate release handles");
	write.origin = Point(0, 0); write.size = desc.size; write.row_pitch = row;
	ok &= Check(device.WriteTexture(first, write, tight.Begin(), tight.GetCount()) == GpuResult::InvalidState,
	            "immutable source cannot be mutated");
	ok &= Check(device.DestroyTexture(first) == GpuResult::Ok &&
	            VulkanGpuDevice::GetSharedImmutableAllocationBytes() == shared,
	            "second handle pins immutable allocation");
	ok &= Check(device.DestroyTexture(second) == GpuResult::Ok &&
	            VulkanGpuDevice::GetSharedImmutableAllocationBytes() == 0,
	            "last handle releases immutable allocation");
	return ok;
}

CONSOLE_APP_MAIN
{
	VulkanTestHooks::ClearVulkanRuntimeDeviceDiagnostics();
	bool ok = true;
	HWND window = CreateWindowExW(0, L"STATIC", L"RenderVulkanPrecisionTest", WS_POPUP,
	                              0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	VulkanSurfaceSession session;
	GpuNativeWindowDesc native;
	native.kind = GpuNativeWindowKind::Win32; native.handle = (uintptr_t)window;
	ok &= Check(window && session.Open(true, native, &Resolver), "precision test opens real Vulkan with validation");
	if(session.IsReady()) {
		VulkanGpuDevice device(session);
		GpuTextureCapabilities rejected;
		rejected.max_size = Size(7, 7);
		ok &= Check(device.GetTextureCapabilities(GpuFormat::Unknown, 0, rejected) == GpuResult::Unsupported &&
		            rejected.max_size == Size(0, 0), "unsupported query clears prior output");
		ok &= Check(device.GetTextureCapabilities(GpuFormat::RGBA32F, 1 << 20, rejected) == GpuResult::InvalidArgument,
		            "unknown usage bits rejected");
		for(GpuFormat format : { GpuFormat::RGBA8, GpuFormat::BGRA8, GpuFormat::RGBA8Srgb,
		                        GpuFormat::BGRA8Srgb, GpuFormat::RGBA16, GpuFormat::RGBA16F,
		                        GpuFormat::RGBA32F, GpuFormat::R16F, GpuFormat::R32F })
			ok &= TestFormat(device, format);
		GpuTextureDesc excessive;
		excessive.size = Size(INT_MAX, INT_MAX); excessive.format = GpuFormat::RGBA32F;
		excessive.usage = GpuTextureUsage_Sampled;
		GpuTextureId id;
		ok &= Check(device.CreateTexture(excessive, id) == GpuResult::Unsupported && !id.IsValid(),
		            "unsupported dimensions rejected before allocation");
		ok &= Check(device.GetLiveTextureCount() == 0 && device.GetLiveAllocationBytes() == 0,
		            "precision resources fully released");
	}
	session.Close();
	ok &= Check(session.GetReport().validation_warning_count == 0 &&
	            session.GetReport().validation_error_count == 0, "precision transfer emits zero validation diagnostics");
	if(window) DestroyWindow(window);
	const auto d = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
	ok &= Check(d.runtime_live_count == 0 && d.instance_live_count == 0 && d.device_live_count == 0 &&
	            d.surface_live_count == 0 && d.swapchain_live_count == 0, "final precision ownership ZERO");
	if(ok) Cout() << "RenderVulkanPrecisionTest passed" << EOL;
	else SetExitCode(1);
}
