#include <RenderVulkan/RenderVulkanRhi.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Upp;
using Upp::VulkanTestHooks::ClearVulkanRuntimeDeviceDiagnostics;
using Upp::VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics;
using Upp::VulkanTestHooks::VulkanRuntimeDeviceDiagnostics;

static const char *g_missing_proc = nullptr;

static FARPROC WINAPI TestResolver(HMODULE module, LPCSTR name)
{
	(void)module;
	if(g_missing_proc && String(name) == g_missing_proc)
		return nullptr;
	return reinterpret_cast<FARPROC>(1);
}

static bool Check(bool condition, const char *message)
{
	if(!condition)
		Cout() << "FAIL: " << message << EOL;
	return condition;
}

static GpuTextureDesc MakeTextureDesc()
{
	GpuTextureDesc desc;
	desc.size = Size(4, 3);
	desc.format = GpuFormat::RGBA8;
	desc.usage = GpuTextureUsage_Sampled | GpuTextureUsage_TransferDst;
	desc.label = "resource-test-texture";
	return desc;
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	ClearVulkanRuntimeDeviceDiagnostics();

	HWND hwnd = CreateWindowExW(0, L"STATIC", L"RenderVulkanResourceTest", WS_POPUP,
	                            0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	ok &= Check(hwnd != nullptr, "hidden Win32 test window should create");

	VulkanSurfaceSession session;
	if(hwnd) {
		GpuNativeWindowDesc native_window;
		native_window.kind = GpuNativeWindowKind::Win32;
		native_window.handle = (uintptr_t)hwnd;
		ok &= Check(session.Open(true, native_window, &TestResolver), "Vulkan surface session should open with validation");
		ok &= Check(session.IsReady(), "Vulkan surface session should be ready");
	}

	if(session.IsReady()) {
		{
			VulkanGpuDevice device(session);
			ok &= Check(device.IsReady(), "VulkanGpuDevice should acquire accepted session interop");
			ok &= Check(device.GetBackendKind() == GpuBackendKind::Vulkan, "VulkanGpuDevice backend should be Vulkan");
			ok &= Check((device.GetAdapterInfo().capability_flags & (GpuCapability_Buffers | GpuCapability_Textures)) ==
			            (GpuCapability_Buffers | GpuCapability_Textures),
			            "adapter should advertise buffer and texture capabilities");

			byte buffer_data[64] = {};
			for(int i = 0; i < 64; ++i)
				buffer_data[i] = (byte)(i * 3 + 1);
			GpuBufferDesc buffer_desc;
			buffer_desc.size = 64;
			buffer_desc.usage = GpuBufferUsage_Vertex | GpuBufferUsage_TransferDst;
			buffer_desc.label = "resource-test-buffer";
			GpuBufferId buffer;
			ok &= Check(device.CreateBuffer(buffer_desc, buffer) == GpuResult::Ok, "real Vulkan buffer creation should succeed");
			ok &= Check(buffer.IsValid(), "real Vulkan buffer handle should be valid");
			ok &= Check(device.WriteBuffer(buffer, 8, buffer_data, 32) == GpuResult::Ok, "real Vulkan buffer write should succeed");
			ok &= Check(device.WriteBuffer(buffer, -1, buffer_data, 4) == GpuResult::InvalidArgument, "negative Vulkan buffer offset should fail");
			ok &= Check(device.WriteBuffer(buffer, 0, nullptr, 4) == GpuResult::InvalidArgument, "null Vulkan buffer data should fail");
			ok &= Check(device.WriteBuffer(buffer, 60, buffer_data, 8) == GpuResult::InvalidArgument, "out-of-range Vulkan buffer write should fail");
			GpuBufferId unknown_buffer;
			unknown_buffer.value = 999;
			ok &= Check(device.WriteBuffer(unknown_buffer, 0, buffer_data, 4) == GpuResult::InvalidHandle, "unknown Vulkan buffer write should fail");

			byte texture_data[64] = {};
			for(int i = 0; i < 64; ++i)
				texture_data[i] = (byte)(255 - i);
			GpuTextureId texture;
			GpuTextureDesc texture_desc = MakeTextureDesc();
			ok &= Check(device.CreateTexture(texture_desc, texture) == GpuResult::Ok, "real Vulkan texture creation should succeed");
			ok &= Check(texture.IsValid(), "real Vulkan texture handle should be valid");
			GpuTextureWriteDesc whole;
			whole.size = Size(4, 3);
			whole.row_pitch = 16;
			ok &= Check(device.WriteTexture(texture, whole, texture_data, 48) == GpuResult::Ok, "tight Vulkan texture upload should succeed");
			GpuTextureWriteDesc partial;
			partial.origin = Point(1, 1);
			partial.size = Size(2, 2);
			partial.row_pitch = 12;
			ok &= Check(device.WriteTexture(texture, partial, texture_data, 20) == GpuResult::Ok, "padded partial Vulkan texture upload should succeed");
			GpuTextureWriteDesc short_pitch = partial;
			short_pitch.row_pitch = 7;
			ok &= Check(device.WriteTexture(texture, short_pitch, texture_data, 20) == GpuResult::InvalidArgument, "short Vulkan texture row pitch should fail");
			ok &= Check(device.WriteTexture(texture, partial, texture_data, 19) == GpuResult::InvalidArgument, "short Vulkan texture byte span should fail");
			GpuTextureWriteDesc outside = partial;
			outside.origin = Point(3, 2);
			ok &= Check(device.WriteTexture(texture, outside, texture_data, 20) == GpuResult::InvalidArgument, "out-of-range Vulkan texture region should fail");
			ok &= Check(device.WriteTexture(texture, partial, nullptr, 20) == GpuResult::InvalidArgument, "null Vulkan texture data should fail");
			GpuTextureId unknown_texture;
			unknown_texture.value = 999;
			ok &= Check(device.WriteTexture(unknown_texture, partial, texture_data, 20) == GpuResult::InvalidHandle, "unknown Vulkan texture write should fail");

			ok &= Check(device.DestroyBuffer(buffer) == GpuResult::Ok, "real Vulkan buffer destroy should succeed");
			ok &= Check(device.WriteBuffer(buffer, 0, buffer_data, 4) == GpuResult::InvalidHandle, "destroyed Vulkan buffer write should fail");
			ok &= Check(device.DestroyTexture(texture) == GpuResult::Ok, "real Vulkan texture destroy should succeed");
			ok &= Check(device.WriteTexture(texture, partial, texture_data, 20) == GpuResult::InvalidHandle, "destroyed Vulkan texture write should fail");
			ok &= Check(device.GetLiveBufferCount() == 0 && device.GetLiveTextureCount() == 0,
			            "explicit Vulkan resource destruction should leave zero adapter-owned resources");

			GpuCommandListId commands;
			ok &= Check(device.BeginCommands(commands) == GpuResult::Ok && commands.IsValid(),
			            "graphics-enabled adapter should begin an empty command list");
			ok &= Check(device.EndCommands(commands) == GpuResult::Ok,
			            "empty command list should end cleanly");
			ok &= Check(device.Submit(commands) == GpuResult::Ok,
			            "empty command list should submit cleanly");

			GpuBufferId destructor_buffer;
			GpuTextureId destructor_texture;
			ok &= Check(device.CreateBuffer(buffer_desc, destructor_buffer) == GpuResult::Ok, "destructor-owned Vulkan buffer should create");
			ok &= Check(device.CreateTexture(texture_desc, destructor_texture) == GpuResult::Ok, "destructor-owned Vulkan texture should create");
			ok &= Check(device.GetLiveBufferCount() == 1 && device.GetLiveTextureCount() == 1,
			            "adapter should report live resources before destructor cleanup");
		}

		session.Close();
		ok &= Check(session.GetReport().validation_warning_count == 0,
		            "Vulkan resource path should emit zero validation warnings");
		ok &= Check(session.GetReport().validation_error_count == 0,
		            "Vulkan resource path should emit zero validation errors");
	}
	else {
		session.Close();
	}

	if(hwnd)
		DestroyWindow(hwnd);

	const VulkanRuntimeDeviceDiagnostics diag = GetVulkanRuntimeDeviceDiagnostics();
	ok &= Check(diag.runtime_live_count == 0 &&
	            diag.instance_live_count == 0 &&
	            diag.debug_messenger_live_count == 0 &&
	            diag.surface_live_count == 0 &&
	            diag.device_live_count == 0 &&
	            diag.swapchain_live_count == 0,
	            "resource test should finish with zero accepted Vulkan ownership diagnostics");

	if(ok) {
		Cout() << "RenderVulkanResourceTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
