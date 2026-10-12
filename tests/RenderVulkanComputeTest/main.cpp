#include <RenderVulkan/RenderVulkanRhi.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include <RenderNull/RenderNull.h>
#include "../../shaders/Quantize16Spirv.h"
#include <cmath>
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
static uint32_t Reference(uint32_t source, int index, int width, bool dither)
{
	const double linear = min<uint32_t>(65535, source) / 65535.0 * 255.0;
	const int pixel = index / 4, x = pixel % width, y = pixel / width;
	auto bayer2 = [](int x, int y) { return 2 * (x ^ y) + y; };
	const int rank = 4 * bayer2(x & 1, y & 1) + bayer2((x >> 1) & 1, (y >> 1) & 1);
	const double threshold = dither && (index & 3) != 3 ? (rank + 0.5) / 16.0 : 0.5;
	return min(255, (int)floor(linear + threshold));
}
struct Parameters { uint32_t count, width, dither, reserved; };
static bool Compare(const Vector<byte>& bytes, const Vector<uint32_t>& source, int prefix,
                    int width, bool dither, bool twice)
{
	if(bytes.GetCount() != (prefix + source.GetCount() + 16) * 4) return Check(false, "readback size");
	int mismatches = 0;
	for(int i = 0; i < bytes.GetCount() / 4; ++i) {
		uint32_t actual;
		std::memcpy(&actual, bytes.Begin() + i * 4, 4);
		uint32_t expected = 0xdeadbeef;
		if(i >= prefix && i < prefix + source.GetCount()) {
			int sample = i - prefix;
			expected = Reference(source[sample], sample, width, dither);
			if(twice) expected = Reference(expected, sample, width, false);
		}
		if(actual != expected) {
			if(mismatches < 4) Cout() << "sample=" << i << " expected=" << expected << " actual=" << actual << EOL;
			++mismatches;
		}
	}
	Cout() << "cpu_reference_mismatches=" << mismatches << " samples=" << source.GetCount()
	       << " dither=" << dither << " chained=" << twice << EOL;
	return Check(mismatches == 0, "GPU output matches independent floating reference and guard regions");
}
static bool Exercise(VulkanGpuDevice& device)
{
	GpuComputeCapabilities caps;
	if(!Check(device.GetComputeCapabilities(caps) == GpuResult::Ok, "real device compute support")) return false;
	Cout() << "adapter=" << device.GetAdapterInfo().name << " storage_slots=" << caps.max_storage_buffers
	       << " max_parameters=" << caps.max_parameter_bytes << " max_dispatches=" << caps.max_dispatches_per_command << EOL;
	bool ok = Check(caps.max_storage_buffers >= 2 && caps.max_parameter_bytes >= 16, "required compute bindings supported");
	GpuShaderDesc sd;
	sd.stage = GpuShaderStage::Compute; sd.format = GpuShaderFormat::SpirV;
	sd.code = String(reinterpret_cast<const char *>(QUANTIZE16_SPIRV), sizeof(QUANTIZE16_SPIRV));
	GpuShaderId shader;
	if(!Check(device.CreateShader(sd, shader) == GpuResult::Ok, "precompiled compute shader creates")) return false;
	GpuComputePipelineDesc pd;
	pd.shader = shader; pd.storage_buffer_count = 2; pd.parameter_bytes = sizeof(Parameters);
	GpuPipelineId pipeline, rejected;
	auto bad = pd; bad.storage_buffer_count = 5;
	rejected.value = 99;
	ok &= Check(device.CreateComputePipeline(bad, rejected) == GpuResult::InvalidArgument && !rejected.IsValid(), "binding limit clears output");
	bad = pd; bad.parameter_bytes = 129;
	ok &= Check(device.CreateComputePipeline(bad, rejected) == GpuResult::InvalidArgument, "parameter alignment/size rejected");
	if(!Check(device.CreateComputePipeline(pd, pipeline) == GpuResult::Ok, "compute pipeline creates")) return false;
	ok &= Check(device.DestroyShader(shader) == GpuResult::InvalidState, "pipeline pins shader lifetime");

	const int width = 193, count = 65536 * 4 + 3; // Exhaustive UINT16 ramp, odd row and dispatch tail.
	const int prefix = (int)max<uint64>(4, caps.storage_offset_alignment) / 4;
	if(!Check(prefix > 0 && prefix <= 4096, "bounded test alignment")) return false;
	Vector<uint32_t> source;
	source.SetCount(count);
	for(int i = 0; i < count; ++i) source[i] = i / 4;
	source[count - 3] = 0; source[count - 2] = 65535; source[count - 1] = 0xffffffffu;
	Vector<uint32_t> initial;
	initial.SetCount(prefix + count + 16, 0xdeadbeef);
	GpuBufferDesc bd;
	bd.size = initial.GetCount() * 4; bd.usage = GpuBufferUsage_Storage | GpuBufferUsage_TransferSrc;
	GpuBufferId input, output, second;
	if(!Check(device.CreateBuffer(bd, input) == GpuResult::Ok &&
	          device.CreateBuffer(bd, output) == GpuResult::Ok &&
	          device.CreateBuffer(bd, second) == GpuResult::Ok, "bounded buffers create")) return false;
	for(auto buffer : {output, second}) ok &= Check(device.WriteBuffer(buffer, 0, initial.Begin(), bd.size) == GpuResult::Ok, "guards initialize");
	std::memcpy(initial.Begin() + prefix, source.Begin(), count * 4);
	ok &= Check(device.WriteBuffer(input, 0, initial.Begin(), bd.size) == GpuResult::Ok, "source uploads once");
	GpuTextureDesc td; td.size = Size(193, 257); td.format = GpuFormat::RGBA8;
	td.usage = GpuTextureUsage_Sampled | GpuTextureUsage_TransferDst | GpuTextureUsage_TransferSrc;
	GpuTextureId image;
	ok &= Check(device.CreateTexture(td, image) == GpuResult::Ok, "compute output texture creates");
	GpuTextureWriteDesc region; region.size = td.size; region.row_pitch = (td.size.cx + 3) * 4;
	Vector<byte> read;
	ok &= Check(device.ReadBuffer(input, -1, 4, read) == GpuResult::InvalidArgument && read.IsEmpty(), "negative read range rejected");
	ok &= Check(device.ReadBuffer(input, 0, std::numeric_limits<int64>::max(), read) == GpuResult::InvalidArgument, "oversized read range rejected");

	for(bool dither : {false, true}) {
		GpuCommandListId command;
		ok &= Check(device.BeginCommands(command) == GpuResult::Ok, "compute commands begin");
		ok &= Check(device.Dispatch(command, 1) == GpuResult::InvalidState, "dispatch without pipeline rejected");
		ok &= Check(device.SetComputePipeline(command, pipeline) == GpuResult::Ok, "compute pipeline binds");
		Parameters params{(uint32_t)count, (uint32_t)width, (uint32_t)dither, 0};
		ok &= Check(device.SetComputeParameters(command, &params, sizeof(params) - 4) == GpuResult::InvalidArgument, "partial parameter payload rejected");
		ok &= Check(device.SetComputeParameters(command, &params, sizeof(params)) == GpuResult::Ok, "complete parameters bind");
		ok &= Check(device.Dispatch(command, 1) == GpuResult::InvalidState, "unbound storage slots rejected");
		ok &= Check(device.SetStorageBuffer(command, 2, input) == GpuResult::InvalidArgument, "undeclared storage slot rejected");
		if(caps.storage_offset_alignment > 1)
			ok &= Check(device.SetStorageBuffer(command, 0, input, 1, 4) == GpuResult::InvalidArgument, "misaligned storage offset rejected");
		ok &= Check(device.SetStorageBuffer(command, 0, input, prefix * 4, count * 4) == GpuResult::Ok &&
		            device.SetStorageBuffer(command, 1, output, prefix * 4, count * 4) == GpuResult::Ok, "aligned source/output ranges bind");
		ok &= Check(device.Dispatch(command, 0) == GpuResult::InvalidArgument, "zero workgroup count rejected");
		ok &= Check(device.Dispatch(command, caps.max_group_count[0] + 1) == GpuResult::InvalidArgument, "device workgroup limit enforced");
		ok &= Check(device.ReadBuffer(output, 0, bd.size, read) == GpuResult::InvalidState && read.IsEmpty(), "readback cannot race live commands");
		ok &= Check(device.DestroyBuffer(input) == GpuResult::InvalidState &&
		            device.DestroyPipeline(pipeline) == GpuResult::InvalidState, "recorded resource lifetime protected");
		ok &= Check(device.Dispatch(command, (count + 63) / 64) == GpuResult::Ok, "quantization dispatch records");
		params.dither = 0;
		ok &= Check(device.SetStorageBuffer(command, 0, output, prefix * 4, count * 4) == GpuResult::Ok &&
		            device.SetStorageBuffer(command, 1, second, prefix * 4, count * 4) == GpuResult::Ok &&
		            device.SetComputeParameters(command, &params, sizeof(params)) == GpuResult::Ok &&
		            device.Dispatch(command, (count + 63) / 64) == GpuResult::Ok, "chained dispatch snapshots bindings and barriers");
		GpuTextureWriteDesc partial; partial.size = Size(1, 1); partial.row_pitch = 4;
		if(!dither) ok &= Check(device.CopyBufferToTexture(command, output, prefix * 4, image, partial) == GpuResult::InvalidState,
		                       "uninitialized texture rejects partial first copy");
		ok &= Check(device.CopyBufferToTexture(command, output, prefix * 4 + 1, image, region) == GpuResult::InvalidArgument,
		            "GPU copy rejects misaligned offset");
		ok &= Check(device.CopyBufferToTexture(command, output, prefix * 4, image, region) == GpuResult::Ok &&
		            device.CopyBufferToTexture(command, input, (prefix + count - 1) * 4, image, partial) == GpuResult::Ok,
		            "GPU-resident output copy and same-command partial initialization");
		ok &= Check(device.EndCommands(command) == GpuResult::Ok, "compute commands end");
		ok &= Check(device.Dispatch(command, 1) == GpuResult::InvalidState, "ended dispatch rejected");
		if(!Check(device.Submit(command) == GpuResult::Ok, "compute submission completes")) return false;
		ok &= Check(device.GetLiveCommandCount() == 0, "submitted commands retire");
		Vector<byte> pixels;
		ok &= Check(device.ReadTexturePixels(image, pixels), "GPU image-copy pixels read back for qualification");
		if(pixels.GetCount() == td.size.cx * td.size.cy * 4) {
			int mismatches = 0;
			for(int y = 0; y < td.size.cy; ++y) for(int x = 0; x < td.size.cx; ++x) {
				uint32_t actual; std::memcpy(&actual, pixels.Begin() + (y * td.size.cx + x) * 4, 4);
				uint32_t expected = x == 0 && y == 0 ? 0xffffffffu : Reference(source[y * (td.size.cx + 3) + x], y * (td.size.cx + 3) + x, width, dither);
				if(actual != expected) ++mismatches;
			}
			ok &= Check(mismatches == 0, "padded-row compute-to-image copy preserves every native byte");
		} else ok &= Check(false, "GPU image-copy exact readback length");
		if(Check(device.ReadBuffer(output, 0, bd.size, read) == GpuResult::Ok, "native output readback"))
			ok &= Compare(read, source, prefix, width, dither, false);
		else ok = false;
		if(Check(device.ReadBuffer(second, 0, bd.size, read) == GpuResult::Ok, "chained output readback"))
			ok &= Compare(read, source, prefix, width, dither, true);
		else ok = false;
		ok &= Check(device.ReadBuffer(input, 0, bd.size, read) == GpuResult::Ok &&
		            !std::memcmp(read.Begin(), initial.Begin(), bd.size), "source samples unchanged");
	}

	GpuCommandListId command;
	Parameters params{1, 1, 0, 0};
	ok &= Check(device.BeginCommands(command) == GpuResult::Ok &&
	            device.SetComputePipeline(command, pipeline) == GpuResult::Ok &&
	            device.SetStorageBuffer(command, 0, input, prefix * 4, 4) == GpuResult::Ok &&
	            device.SetStorageBuffer(command, 1, output, prefix * 4, 4) == GpuResult::Ok &&
	            device.SetComputeParameters(command, &params, sizeof(params)) == GpuResult::Ok, "dispatch budget setup");
	for(uint32 i = 0; i < caps.max_dispatches_per_command; ++i)
		ok &= Check(device.Dispatch(command, 1) == GpuResult::Ok, "descriptor pages within dispatch budget");
	ok &= Check(device.Dispatch(command, 1) == GpuResult::InvalidState, "dispatch budget rejects before growth");
	ok &= Check(device.EndCommands(command) == GpuResult::Ok && device.Submit(command) == GpuResult::Ok, "budgeted dispatch command completes");

	// Device-local output never maps through the host. Qualify bytes via GPU copy into a texture.
	GpuBufferDesc local_desc; local_desc.size = 4; local_desc.usage = bd.usage;
	local_desc.memory = GpuBufferMemory::DeviceLocal;
	GpuBufferId local;
	ok &= Check(device.CreateBuffer(local_desc, local) == GpuResult::Ok, "device-local output creates");
	ok &= Check(device.WriteBuffer(local, 0, &params, 4) == GpuResult::InvalidState &&
	            device.ReadBuffer(local, 0, 4, read) == GpuResult::InvalidState && read.IsEmpty(),
	            "device-local buffers forbid host mapping on all memory architectures");
	GpuTextureDesc pixel_desc = td; pixel_desc.size = Size(1,1);
	GpuTextureId pixel; ok &= Check(device.CreateTexture(pixel_desc, pixel) == GpuResult::Ok, "device-local qualification image");
	GpuTextureWriteDesc pixel_region; pixel_region.size = Size(1,1); pixel_region.row_pitch = 4;
	for(int frame = 0; frame < 32; ++frame) {
		ok &= Check(device.BeginCommands(command) == GpuResult::Ok &&
		            device.SetComputePipeline(command, pipeline) == GpuResult::Ok &&
		            device.SetStorageBuffer(command, 0, input, prefix * 4, 4) == GpuResult::Ok &&
		            device.SetStorageBuffer(command, 1, local) == GpuResult::Ok &&
		            device.SetComputeParameters(command, &params, sizeof(params)) == GpuResult::Ok &&
		            device.Dispatch(command, 1) == GpuResult::Ok &&
		            device.CopyBufferToTexture(command, local, 0, pixel, pixel_region) == GpuResult::Ok &&
		            device.EndCommands(command) == GpuResult::Ok && device.Submit(command) == GpuResult::Ok,
		            "repeated device-local dispatch/copy completes");
	}
	Vector<byte> pixel_bytes;
	ok &= Check(device.ReadTexturePixels(pixel, pixel_bytes) && pixel_bytes.GetCount() == 4 &&
	            pixel_bytes[0] == 0 && pixel_bytes[1] == 0 && pixel_bytes[2] == 0 && pixel_bytes[3] == 0,
	            "device-local compute result reaches the sampled image intact");
	GpuComputeStats stats;
	ok &= Check(device.GetComputeStats(stats) == GpuResult::Ok && stats.live_commands == 0 &&
	            stats.descriptor_pools_created == 4 && stats.descriptor_pools_reused >= 32 &&
	            stats.retained_descriptor_pools == 4, "descriptor pools plateau and recycle after completion");
	Cout() << "descriptor_pools_created=" << stats.descriptor_pools_created << " reused=" << stats.descriptor_pools_reused
	       << " retained=" << stats.retained_descriptor_pools << EOL;
	ok &= Check(device.DestroyBuffer(local) == GpuResult::Ok && device.DestroyTexture(pixel) == GpuResult::Ok,
	            "device-local resources release");
	ok &= Check(device.DestroyTexture(image) == GpuResult::Ok, "compute output image releases");
	for(auto buffer : {input, output, second}) ok &= Check(device.DestroyBuffer(buffer) == GpuResult::Ok, "compute buffers release");
	ok &= Check(device.DestroyPipeline(pipeline) == GpuResult::Ok && device.DestroyShader(shader) == GpuResult::Ok, "pipeline/shader release in ownership order");
	ok &= Check(device.GetLiveBufferCount() == 0 && device.GetLivePipelineCount() == 0 &&
	            device.GetLiveShaderCount() == 0 && device.GetLiveTextureCount() == 0 && device.GetLiveCommandCount() == 0 &&
	            device.GetLiveAllocationBytes() == 0, "all adapter compute resources released");
	return ok;
}
CONSOLE_APP_MAIN
{
	bool ok = true;
	NullGpuDevice null;
	GpuComputeCapabilities absent; absent.max_storage_buffers = 99;
	GpuPipelineId unavailable; unavailable.value = 42;
	ok &= Check(null.GetComputeCapabilities(absent) == GpuResult::Unsupported && absent.max_storage_buffers == 0 &&
	            null.CreateComputePipeline(GpuComputePipelineDesc(), unavailable) == GpuResult::Unsupported && !unavailable.IsValid(),
	            "absent backend clears optional compute capabilities and outputs");
	VulkanTestHooks::ClearVulkanRuntimeDeviceDiagnostics();
	HWND window = CreateWindowExW(0, L"STATIC", L"RenderVulkanComputeTest", WS_POPUP, 0, 0, 64, 64,
	                              nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	VulkanSurfaceSession session;
	GpuNativeWindowDesc native; native.kind = GpuNativeWindowKind::Win32; native.handle = (uintptr_t)window;
	ok &= Check(window && session.Open(true, native, &Resolver), "real Vulkan session opens with validation");
	if(session.IsReady()) { VulkanGpuDevice device(session); ok &= Exercise(device); }
	session.Close();
	const auto& report = session.GetReport();
	Cout() << "validation_warnings=" << report.validation_warning_count << " errors=" << report.validation_error_count << EOL;
	ok &= Check(report.validation_warning_count == 0 && report.validation_error_count == 0, "zero Vulkan validation diagnostics");
	if(window) DestroyWindow(window);
	const auto d = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
	ok &= Check(d.runtime_live_count == 0 && d.instance_live_count == 0 && d.device_live_count == 0 &&
	            d.surface_live_count == 0 && d.swapchain_live_count == 0, "final native ownership ZERO");
	if(ok) Cout() << "RenderVulkanComputeTest passed" << EOL; else SetExitCode(1);
}
