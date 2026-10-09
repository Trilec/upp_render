// ABI and offscreen execution proof for the pinned wgpu-native release.
// This is intentionally an isolated native spike, not a second public RHI.
#include <Core/Core.h>
#include <wgpu.h>
#include <atomic>
#include <cstring>
#include <chrono>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Upp;

#define WGPU_STANDARD_PROCS(X) \
	X(CreateInstance) \
	X(InstanceRelease) \
	X(AdapterGetInfo) \
	X(AdapterInfoFreeMembers) \
	X(AdapterGetLimits) \
	X(AdapterRelease) \
	X(AdapterRequestDevice) \
	X(DeviceGetQueue) \
	X(DeviceGetLimits) \
	X(DeviceRelease) \
	X(DeviceDestroy) \
	X(QueueRelease) \
	X(DeviceCreateTexture) \
	X(TextureCreateView) \
	X(TextureViewRelease) \
	X(TextureRelease) \
	X(DeviceCreateBuffer) \
	X(BufferRelease) \
	X(BufferMapAsync) \
	X(BufferGetConstMappedRange) \
	X(BufferUnmap) \
	X(DeviceCreateCommandEncoder) \
	X(CommandEncoderBeginRenderPass) \
	X(RenderPassEncoderEnd) \
	X(RenderPassEncoderRelease) \
	X(CommandEncoderCopyTextureToBuffer) \
	X(CommandEncoderFinish) \
	X(CommandEncoderRelease) \
	X(CommandBufferRelease) \
	X(QueueSubmit) \
	X(InstanceCreateSurface) \
	X(SurfaceGetCapabilities) \
	X(SurfaceCapabilitiesFreeMembers) \
	X(SurfaceConfigure) \
	X(SurfaceUnconfigure) \
	X(SurfaceGetCurrentTexture) \
	X(SurfacePresent) \
	X(SurfaceRelease) \
	X(DeviceCreateShaderModule) \
	X(ShaderModuleRelease) \
	X(DeviceCreateRenderPipeline) \
	X(RenderPipelineRelease) \
	X(RenderPassEncoderSetPipeline) \
	X(RenderPassEncoderDraw)

struct NativeApi {
	HMODULE module = nullptr;
#define DECLARE(name) WGPUProc##name name = nullptr;
	WGPU_STANDARD_PROCS(DECLARE)
#undef DECLARE
	decltype(&wgpuGetVersion) GetVersion = nullptr;
	decltype(&wgpuInstanceEnumerateAdapters) EnumerateAdapters = nullptr;
	decltype(&wgpuDevicePoll) DevicePoll = nullptr;
	decltype(&wgpuGenerateReport) GenerateReport = nullptr;

	bool Open(const String& path) {
		// Absolute selection avoids loading an unrelated DLL from the current directory.
		if(!IsFullPath(path)) {
			Cout() << "FAIL: --dll requires an absolute path" << EOL;
			return false;
		}
		int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ~path, path.GetCount(), nullptr, 0);
		if(count <= 0) return false;
		Vector<wchar_t> wide;
		wide.SetCount(count + 1, 0);
		if(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ~path, path.GetCount(), wide.Begin(), count) != count)
			return false;
		module = LoadLibraryExW(wide.Begin(), nullptr,
		                       LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		if(!module) {
			Cout() << "UNAVAILABLE: native DLL load failed, Win32=" << (int)::GetLastError() << EOL;
			return false;
		}
#define RESOLVE(name) name = reinterpret_cast<decltype(name)>(GetProcAddress(module, "wgpu" #name)); \
		if(!name) { Cout() << "FAIL: missing wgpu" #name << EOL; return false; }
		WGPU_STANDARD_PROCS(RESOLVE)
#undef RESOLVE
#define NATIVE(member, symbol) member = reinterpret_cast<decltype(member)>(GetProcAddress(module, symbol)); \
		if(!member) { Cout() << "FAIL: missing " symbol << EOL; return false; }
		NATIVE(GetVersion, "wgpuGetVersion")
		NATIVE(EnumerateAdapters, "wgpuInstanceEnumerateAdapters")
		NATIVE(DevicePoll, "wgpuDevicePoll")
		NATIVE(GenerateReport, "wgpuGenerateReport")
#undef NATIVE
		uint32 version = GetVersion();
		Cout() << Format("native_version=0x%08x", (int)version) << EOL;
		if(version != 0x1d000101) {
			Cout() << "FAIL: expected exactly v29.0.1.1 before passing ABI structs" << EOL;
			return false;
		}
		return true;
	}
	~NativeApi() { if(module) FreeLibrary(module); }
};

struct Callbacks {
	std::atomic<int> errors { 0 }, unexpected_loss { 0 };
	std::atomic<int> map_status { -1 };
	WGPUDevice requested = nullptr;
	bool request_done = false;
};
// Process-lifetime callback storage also remains valid during failure cleanup.
static Callbacks callbacks;

static void Requested(WGPURequestDeviceStatus status, WGPUDevice device,
                      WGPUStringView, void *, void *)
{
	callbacks.requested = status == WGPURequestDeviceStatus_Success ? device : nullptr;
	callbacks.request_done = true;
}
static void Lost(WGPUDevice const *, WGPUDeviceLostReason reason, WGPUStringView, void *, void *)
{
	if(reason != WGPUDeviceLostReason_Destroyed && reason != WGPUDeviceLostReason_CallbackCancelled)
		++callbacks.unexpected_loss;
}
static void Error(WGPUDevice const *, WGPUErrorType, WGPUStringView, void *, void *)
{
	++callbacks.errors;
}
static void Mapped(WGPUMapAsyncStatus status, WGPUStringView, void *, void *)
{
	callbacks.map_status.store((int)status, std::memory_order_release);
}
static String Text(WGPUStringView text)
{
	if(!text.data) return String();
	size_t size = text.length == WGPU_STRLEN ? std::strlen(text.data) : text.length;
	return String(text.data, (int)min<size_t>(size, 4096));
}

struct Probe {
	NativeApi& a;
	WGPUInstance instance = nullptr;
	WGPUAdapter adapter = nullptr;
	WGPUDevice device = nullptr;
	WGPUQueue queue = nullptr;
	WGPUTexture texture = nullptr;
	WGPUTextureView view = nullptr;
	WGPUBuffer buffer = nullptr;
	WGPUCommandEncoder encoder = nullptr;
	WGPURenderPassEncoder pass = nullptr;
	WGPUCommandBuffer command = nullptr;
	bool mapped = false;

	explicit Probe(NativeApi& api) : a(api) {}
	void ReleaseWork() {
		if(mapped) { a.BufferUnmap(buffer); mapped = false; }
		if(pass) { a.RenderPassEncoderRelease(pass); pass = nullptr; }
		if(command) { a.CommandBufferRelease(command); command = nullptr; }
		if(encoder) { a.CommandEncoderRelease(encoder); encoder = nullptr; }
		if(view) { a.TextureViewRelease(view); view = nullptr; }
		if(texture) { a.TextureRelease(texture); texture = nullptr; }
		if(buffer) { a.BufferRelease(buffer); buffer = nullptr; }
	}
	void ReleaseDevice() {
		ReleaseWork();
		// Teardown drain is deliberate. Steady-state streaming has its own later gate.
		if(device) a.DevicePoll(device, WGPU_TRUE, nullptr);
		if(queue) { a.QueueRelease(queue); queue = nullptr; }
		if(device) {
			a.DeviceDestroy(device);
			a.DeviceRelease(device);
			device = nullptr;
		}
		if(adapter) { a.AdapterRelease(adapter); adapter = nullptr; }
	}
	~Probe() {
		ReleaseDevice();
		if(instance) a.InstanceRelease(instance);
	}

	bool Open() {
		WGPUInstanceExtras extras {};
		extras.chain.sType = (WGPUSType)WGPUSType_InstanceExtras;
		extras.backends = WGPUInstanceBackend_Vulkan;
		WGPUInstanceDescriptor desc = WGPU_INSTANCE_DESCRIPTOR_INIT;
		desc.nextInChain = &extras.chain;
		instance = a.CreateInstance(&desc);
		if(!instance) return false;
		WGPUInstanceEnumerateAdapterOptions options {};
		options.backends = WGPUInstanceBackend_Vulkan;
		size_t count = a.EnumerateAdapters(instance, &options, nullptr);
		if(count == 0 || count > 64) return false;
		Vector<WGPUAdapter> adapters;
		adapters.SetCount((int)count, nullptr);
		size_t actual = a.EnumerateAdapters(instance, &options, adapters.Begin());
		if(actual != count) {
			for(WGPUAdapter candidate : adapters) if(candidate) a.AdapterRelease(candidate);
			return false;
		}
		// Prefer discrete hardware, with an explicit Vulkan-only instance.
		int selected = -1;
		for(int i = 0; i < adapters.GetCount(); ++i) {
			WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
			if(a.AdapterGetInfo(adapters[i], &info) != WGPUStatus_Success) continue;
			if(info.backendType == WGPUBackendType_Vulkan && info.adapterType != WGPUAdapterType_CPU &&
			   (selected < 0 || info.adapterType == WGPUAdapterType_DiscreteGPU))
				selected = i;
			a.AdapterInfoFreeMembers(info);
		}
		for(int i = 0; i < adapters.GetCount(); ++i)
			if(i != selected) a.AdapterRelease(adapters[i]);
		if(selected < 0) return false;
		adapter = adapters[selected];
		WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
		if(a.AdapterGetInfo(adapter, &info) != WGPUStatus_Success) return false;
		Cout() << "adapter=" << Text(info.device) << " backend=Vulkan vendor=" << info.vendorID << EOL;
		a.AdapterInfoFreeMembers(info);
		WGPULimits limits = WGPU_LIMITS_INIT;
		if(a.AdapterGetLimits(adapter, &limits) != WGPUStatus_Success) return false;
		Cout() << "adapter_max_texture_2d=" << limits.maxTextureDimension2D
		       << " adapter_max_buffer=" << limits.maxBufferSize << EOL;
		WGPUDeviceDescriptor dd = WGPU_DEVICE_DESCRIPTOR_INIT;
		dd.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
		dd.deviceLostCallbackInfo.callback = Lost;
		dd.uncapturedErrorCallbackInfo.callback = Error;
		WGPURequestDeviceCallbackInfo callback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
		callback.mode = WGPUCallbackMode_AllowSpontaneous;
		callback.callback = Requested;
		callbacks.requested = nullptr;
		callbacks.request_done = false;
		a.AdapterRequestDevice(adapter, &dd, callback);
		// Pinned native v29 implements this request synchronously (src/lib.rs).
		// This is not an assumption for the future browser implementation.
		if(!callbacks.request_done || !callbacks.requested) return false;
		device = callbacks.requested;
		queue = a.DeviceGetQueue(device);
		if(!queue) return false;
		limits = WGPU_LIMITS_INIT;
		if(a.DeviceGetLimits(device, &limits) != WGPUStatus_Success) return false;
		Cout() << "device_max_texture_2d=" << limits.maxTextureDimension2D << EOL;
		return true;
	}

	bool ClearAndRead() {
		WGPUTextureDescriptor td = WGPU_TEXTURE_DESCRIPTOR_INIT;
		td.size = { 3, 2, 1 };
		td.dimension = WGPUTextureDimension_2D;
		td.format = WGPUTextureFormat_RGBA8Unorm;
		td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
		texture = a.DeviceCreateTexture(device, &td);
		if(!texture) return false;
		view = a.TextureCreateView(texture, nullptr);
		if(!view) return false;
		WGPUBufferDescriptor bd = WGPU_BUFFER_DESCRIPTOR_INIT;
		bd.size = 512;
		bd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
		buffer = a.DeviceCreateBuffer(device, &bd);
		if(!buffer) return false;
		encoder = a.DeviceCreateCommandEncoder(device, nullptr);
		if(!encoder) return false;
		WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
		color.view = view;
		color.loadOp = WGPULoadOp_Clear;
		color.storeOp = WGPUStoreOp_Store;
		color.clearValue = { 0, 1, 0, 1 };
		WGPURenderPassDescriptor pd = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
		pd.colorAttachmentCount = 1; pd.colorAttachments = &color;
		pass = a.CommandEncoderBeginRenderPass(encoder, &pd);
		if(!pass) return false;
		a.RenderPassEncoderEnd(pass);
		a.RenderPassEncoderRelease(pass); pass = nullptr;
		WGPUTexelCopyTextureInfo from = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
		from.texture = texture;
		WGPUTexelCopyBufferInfo to = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
		to.buffer = buffer; to.layout.bytesPerRow = 256; to.layout.rowsPerImage = 2;
		a.CommandEncoderCopyTextureToBuffer(encoder, &from, &to, &td.size);
		command = a.CommandEncoderFinish(encoder, nullptr);
		if(!command) return false;
		a.QueueSubmit(queue, 1, &command);
		callbacks.map_status.store(-1);
		WGPUBufferMapCallbackInfo cb = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
		cb.mode = WGPUCallbackMode_AllowSpontaneous; cb.callback = Mapped;
		a.BufferMapAsync(buffer, WGPUMapMode_Read, 0, 512, cb);
		auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while(callbacks.map_status.load(std::memory_order_acquire) < 0 && std::chrono::steady_clock::now() < deadline) {
			a.DevicePoll(device, WGPU_FALSE, nullptr);
			Sleep(1);
		}
		if(callbacks.map_status.load() != (int)WGPUMapAsyncStatus_Success) return false;
		mapped = true;
		const byte *pixels = static_cast<const byte *>(a.BufferGetConstMappedRange(buffer, 0, 512));
		if(!pixels) return false;
		for(int y = 0; y < 2; ++y)
			for(int x = 0; x < 3; ++x) {
				const byte *p = pixels + y * 256 + x * 4;
				if(p[0] != 0 || p[1] != 255 || p[2] != 0 || p[3] != 255) return false;
			}
		ReleaseWork();
		Cout() << "offscreen_clear_readback=PASS pixels=6 row_pitch=256" << EOL;
		return true;
	}

	bool CheckReleased() {
		ReleaseDevice();
		WGPUGlobalReport report {};
		a.GenerateReport(instance, &report);
		const WGPUHubReport& h = report.hub;
		size_t kept = h.adapters.numKeptFromUser + h.devices.numKeptFromUser + h.queues.numKeptFromUser +
		              h.buffers.numKeptFromUser + h.textures.numKeptFromUser + h.textureViews.numKeptFromUser +
		              h.commandBuffers.numKeptFromUser + h.renderPipelines.numKeptFromUser +
		              h.shaderModules.numKeptFromUser + report.surfaces.numKeptFromUser;
		Cout() << "native_user_handles=" << (uint64)kept << " errors=" << callbacks.errors.load()
		       << " unexpected_device_loss=" << callbacks.unexpected_loss.load() << EOL;
		return kept == 0 && callbacks.errors.load() == 0 && callbacks.unexpected_loss.load() == 0;
	}
};

// Win32 host feasibility: direct native handles, no legacy Vulkan/RHI adapter.
struct SurfaceProbe {
	Probe& p;
	HWND hwnd = nullptr;
	WGPUSurface surface = nullptr;
	WGPUShaderModule shader = nullptr;
	WGPURenderPipeline pipeline = nullptr;
	WGPUTextureFormat format = WGPUTextureFormat_Undefined;
	bool configured = false;
	explicit SurfaceProbe(Probe& probe) : p(probe) {}
	~SurfaceProbe() {
		p.ReleaseWork();
		p.a.DevicePoll(p.device, WGPU_TRUE, nullptr);
		if(pipeline) p.a.RenderPipelineRelease(pipeline);
		if(shader) p.a.ShaderModuleRelease(shader);
		if(configured) p.a.SurfaceUnconfigure(surface);
		if(surface) p.a.SurfaceRelease(surface);
		if(hwnd && IsWindow(hwnd)) DestroyWindow(hwnd);
	}
	bool Configure() {
		RECT rect {};
		if(!GetClientRect(hwnd, &rect) || rect.right <= 0 || rect.bottom <= 0) return false;
		WGPUSurfaceConfiguration config = WGPU_SURFACE_CONFIGURATION_INIT;
		config.device = p.device;
		config.format = format;
		config.width = rect.right;
		config.height = rect.bottom;
		config.presentMode = WGPUPresentMode_Fifo;
		config.alphaMode = WGPUCompositeAlphaMode_Opaque;
		p.a.SurfaceConfigure(surface, &config);
		configured = true;
		return callbacks.errors.load() == 0;
	}
	bool Open() {
		WNDCLASSW wc {};
		wc.lpfnWndProc = DefWindowProcW;
		wc.hInstance = GetModuleHandleW(nullptr);
		wc.lpszClassName = L"RenderWgpuNativeProbe";
		if(!RegisterClassW(&wc) && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
		hwnd = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName,
		                       L"wgpu Vulkan host qualification", WS_OVERLAPPEDWINDOW,
		                       40, 40, 400, 300, nullptr, nullptr, wc.hInstance, nullptr);
		if(!hwnd) return false;
		WGPUSurfaceSourceWindowsHWND source = WGPU_SURFACE_SOURCE_WINDOWS_HWND_INIT;
		source.hinstance = wc.hInstance; source.hwnd = hwnd;
		WGPUSurfaceDescriptor desc = WGPU_SURFACE_DESCRIPTOR_INIT;
		desc.nextInChain = &source.chain;
		surface = p.a.InstanceCreateSurface(p.instance, &desc);
		if(!surface) return false;
		WGPUSurfaceCapabilities caps = WGPU_SURFACE_CAPABILITIES_INIT;
		if(p.a.SurfaceGetCapabilities(surface, p.adapter, &caps) != WGPUStatus_Success) return false;
		for(size_t i = 0; i < caps.formatCount; ++i)
			if(caps.formats[i] == WGPUTextureFormat_BGRA8UnormSrgb ||
			   caps.formats[i] == WGPUTextureFormat_RGBA8UnormSrgb) { format = caps.formats[i]; break; }
		bool opaque = false;
		for(size_t i = 0; i < caps.alphaModeCount; ++i)
			opaque |= caps.alphaModes[i] == WGPUCompositeAlphaMode_Opaque;
		p.a.SurfaceCapabilitiesFreeMembers(caps);
		if(format == WGPUTextureFormat_Undefined || !opaque) return false;
		const char *wgsl = R"WGSL(
@vertex fn vs(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
    let points = array<vec2f, 3>(vec2f(-0.75, -0.7), vec2f(0.75, -0.7), vec2f(0.0, 0.75));
    return vec4f(points[i], 0.0, 1.0);
}
@fragment fn fs() -> @location(0) vec4f { return vec4f(0.1, 0.65, 0.95, 1.0); }
)WGSL";
		WGPUShaderSourceWGSL source_code = WGPU_SHADER_SOURCE_WGSL_INIT;
		source_code.code = { wgsl, WGPU_STRLEN };
		WGPUShaderModuleDescriptor sd = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
		sd.nextInChain = &source_code.chain;
		shader = p.a.DeviceCreateShaderModule(p.device, &sd);
		if(!shader) return false;
		WGPUColorTargetState target = WGPU_COLOR_TARGET_STATE_INIT;
		target.format = format;
		WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
		fragment.module = shader; fragment.entryPoint = { "fs", WGPU_STRLEN };
		fragment.targetCount = 1; fragment.targets = &target;
		WGPURenderPipelineDescriptor pipeline_desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
		pipeline_desc.vertex.module = shader; pipeline_desc.vertex.entryPoint = { "vs", WGPU_STRLEN };
		pipeline_desc.fragment = &fragment;
		pipeline = p.a.DeviceCreateRenderPipeline(p.device, &pipeline_desc);
		if(!pipeline || !Configure()) return false;
		ShowWindow(hwnd, SW_SHOWNOACTIVATE);
		return true;
	}
	bool Frame() {
		WGPUSurfaceTexture frame = WGPU_SURFACE_TEXTURE_INIT;
		p.a.SurfaceGetCurrentTexture(surface, &frame);
		p.texture = frame.texture;
		if(!p.texture || (frame.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
		                  frame.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal))
			return false;
		p.view = p.a.TextureCreateView(p.texture, nullptr);
		if(!p.view) return false;
		p.encoder = p.a.DeviceCreateCommandEncoder(p.device, nullptr);
		if(!p.encoder) return false;
		WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
		color.view = p.view; color.loadOp = WGPULoadOp_Clear; color.storeOp = WGPUStoreOp_Store;
		color.clearValue = { 0.025, 0.03, 0.04, 1 };
		WGPURenderPassDescriptor pd = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
		pd.colorAttachmentCount = 1; pd.colorAttachments = &color;
		p.pass = p.a.CommandEncoderBeginRenderPass(p.encoder, &pd);
		if(!p.pass) return false;
		p.a.RenderPassEncoderSetPipeline(p.pass, pipeline);
		p.a.RenderPassEncoderDraw(p.pass, 3, 1, 0, 0);
		p.a.RenderPassEncoderEnd(p.pass);
		p.a.RenderPassEncoderRelease(p.pass); p.pass = nullptr;
		p.command = p.a.CommandEncoderFinish(p.encoder, nullptr);
		if(!p.command) return false;
		p.a.QueueSubmit(p.queue, 1, &p.command);
		bool ok = p.a.SurfacePresent(surface) == WGPUStatus_Success;
		p.ReleaseWork();
		p.a.DevicePoll(p.device, WGPU_FALSE, nullptr);
		return ok && callbacks.errors.load() == 0 && callbacks.unexpected_loss.load() == 0;
	}
	bool Run() {
		if(!Open()) return false;
		for(int i = 0; i < 60; ++i) {
			MSG msg {};
			while(PeekMessageW(&msg, hwnd, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg); DispatchMessageW(&msg);
			}
			if(!IsWindow(hwnd)) return false;
			if(i == 20) {
				if(!SetWindowPos(hwnd, nullptr, 40, 40, 640, 420, SWP_NOZORDER | SWP_NOACTIVATE) ||
				   !Configure()) return false;
			}
			if(i == 40) {
				ShowWindow(hwnd, SW_MINIMIZE);
				Sleep(50);
				ShowWindow(hwnd, SW_SHOWNOACTIVATE);
				if(!Configure()) return false;
			}
			if(!Frame()) return false;
		}
		Cout() << "win32_triangle=PASS present_calls=60 resize=PASS minimize_restore=PASS" << EOL;
		return true;
	}
};

CONSOLE_APP_MAIN
{
	const Vector<String>& args = CommandLine();
	if(args.GetCount() != 2 || args[0] != "--dll") {
		Cout() << "Usage: RenderWgpuNativeTest --dll <absolute pinned wgpu_native.dll>" << EOL;
		SetExitCode(2); return;
	}
	NativeApi api;
	if(!api.Open(args[1])) { SetExitCode(2); return; }
	for(int iteration = 0; iteration < 3; ++iteration) {
		Probe probe(api);
		bool ok = probe.Open() && probe.ClearAndRead();
		if(ok) { SurfaceProbe surface(probe); ok = surface.Run(); }
		if(!ok || !probe.CheckReleased()) {
			Cout() << "FAIL: native probe iteration=" << iteration << EOL;
			SetExitCode(1); return;
		}
	}
	Cout() << "PASS: pinned C ABI, Vulkan device, offscreen pixels, Win32 triangle/lifecycle and repeated cleanup" << EOL;
}
