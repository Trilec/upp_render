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

static String SpirV(const uint32_t *words, int count)
{
	return String(reinterpret_cast<const char *>(words), count * (int)sizeof(uint32_t));
}

// Deterministic SPIR-V 1.0 generated from the minimal S17C shader interface:
// location 0 vec2 position, location 1 vec4 colour, location 0 fragment colour.
static const uint32_t kVertexShader[] = {
	0x07230203u, 0x00010000u, 0x00000000u, 0x00000016u, 0x00000000u, 0x00020011u,
	0x00000001u, 0x0003000eu, 0x00000000u, 0x00000001u, 0x0009000fu, 0x00000000u,
	0x0000000fu, 0x6e69616du, 0x00000000u, 0x00000007u, 0x00000009u, 0x0000000bu,
	0x0000000cu, 0x00040047u, 0x00000007u, 0x0000001eu, 0x00000000u, 0x00040047u,
	0x00000009u, 0x0000001eu, 0x00000001u, 0x00040047u, 0x0000000bu, 0x0000000bu,
	0x00000000u, 0x00040047u, 0x0000000cu, 0x0000001eu, 0x00000000u, 0x00020013u,
	0x00000001u, 0x00030021u, 0x00000002u, 0x00000001u, 0x00030016u, 0x00000003u,
	0x00000020u, 0x00040017u, 0x00000004u, 0x00000003u, 0x00000002u, 0x00040017u,
	0x00000005u, 0x00000003u, 0x00000004u, 0x00040020u, 0x00000006u, 0x00000001u,
	0x00000004u, 0x0004003bu, 0x00000006u, 0x00000007u, 0x00000001u, 0x00040020u,
	0x00000008u, 0x00000001u, 0x00000005u, 0x0004003bu, 0x00000008u, 0x00000009u,
	0x00000001u, 0x00040020u, 0x0000000au, 0x00000003u, 0x00000005u, 0x0004003bu,
	0x0000000au, 0x0000000bu, 0x00000003u, 0x0004003bu, 0x0000000au, 0x0000000cu,
	0x00000003u, 0x0004002bu, 0x00000003u, 0x0000000du, 0x00000000u, 0x0004002bu,
	0x00000003u, 0x0000000eu, 0x3f800000u, 0x00050036u, 0x00000001u, 0x0000000fu,
	0x00000000u, 0x00000002u, 0x000200f8u, 0x00000010u, 0x0004003du, 0x00000004u,
	0x00000011u, 0x00000007u, 0x00050051u, 0x00000003u, 0x00000012u, 0x00000011u,
	0x00000000u, 0x00050051u, 0x00000003u, 0x00000013u, 0x00000011u, 0x00000001u,
	0x00070050u, 0x00000005u, 0x00000014u, 0x00000012u, 0x00000013u, 0x0000000du,
	0x0000000eu, 0x0003003eu, 0x0000000bu, 0x00000014u, 0x0004003du, 0x00000005u,
	0x00000015u, 0x00000009u, 0x0003003eu, 0x0000000cu, 0x00000015u, 0x000100fdu,
	0x00010038u,
};

static const uint32_t kFragmentShader[] = {
	0x07230203u, 0x00010000u, 0x00000000u, 0x0000000cu, 0x00000000u, 0x00020011u,
	0x00000001u, 0x0003000eu, 0x00000000u, 0x00000001u, 0x0007000fu, 0x00000004u,
	0x00000009u, 0x6e69616du, 0x00000000u, 0x00000006u, 0x00000008u, 0x00030010u,
	0x00000009u, 0x00000007u, 0x00040047u, 0x00000006u, 0x0000001eu, 0x00000000u,
	0x00040047u, 0x00000008u, 0x0000001eu, 0x00000000u, 0x00020013u, 0x00000001u,
	0x00030021u, 0x00000002u, 0x00000001u, 0x00030016u, 0x00000003u, 0x00000020u,
	0x00040017u, 0x00000004u, 0x00000003u, 0x00000004u, 0x00040020u, 0x00000005u,
	0x00000001u, 0x00000004u, 0x0004003bu, 0x00000005u, 0x00000006u, 0x00000001u,
	0x00040020u, 0x00000007u, 0x00000003u, 0x00000004u, 0x0004003bu, 0x00000007u,
	0x00000008u, 0x00000003u, 0x00050036u, 0x00000001u, 0x00000009u, 0x00000000u,
	0x00000002u, 0x000200f8u, 0x0000000au, 0x0004003du, 0x00000004u, 0x0000000bu,
	0x00000006u, 0x0003003eu, 0x00000008u, 0x0000000bu, 0x000100fdu, 0x00010038u,
};

struct Vertex {
	float x, y;
	float r, g, b, a;
};

CONSOLE_APP_MAIN
{
	bool ok = true;
	ClearVulkanRuntimeDeviceDiagnostics();
	HWND hwnd = CreateWindowExW(0, L"STATIC", L"RenderVulkanGraphicsTest", WS_POPUP,
	                            0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	ok &= Check(hwnd != nullptr, "hidden Win32 test window should create");

	VulkanSurfaceSession session;
	if(hwnd) {
		GpuNativeWindowDesc native_window;
		native_window.kind = GpuNativeWindowKind::Win32;
		native_window.handle = (uintptr_t)hwnd;
		ok &= Check(session.Open(true, native_window, &TestResolver), "Vulkan session should open with validation");
		ok &= Check(session.IsReady(), "Vulkan session should be ready");
	}

	if(session.IsReady()) {
		{
			VulkanGpuDevice device(session);
			ok &= Check(device.IsReady(), "VulkanGpuDevice should be ready");
			const int expected_caps = GpuCapability_Buffers | GpuCapability_Textures | GpuCapability_RenderPass | GpuCapability_Pipelines | GpuCapability_Shaders;
			ok &= Check((device.GetAdapterInfo().capability_flags & expected_caps) == expected_caps,
			            "S17C-B1 should advertise implemented graphics capabilities");

			GpuSurfaceId surface;
			GpuSurfaceDesc surface_desc;
			ok &= Check(device.CreateSurface(surface_desc, surface) == GpuResult::Unsupported && !surface.IsValid(),
			            "session-bound neutral surface convergence remains explicitly deferred to B2");

			const Vertex vertices[] = {
				{-0.75f, -0.60f, 1.0f, 0.1f, 0.1f, 1.0f},
				{ 0.75f, -0.60f, 0.1f, 1.0f, 0.1f, 1.0f},
				{ 0.00f,  0.75f, 0.1f, 0.2f, 1.0f, 1.0f},
			};
			GpuBufferDesc buffer_desc;
			buffer_desc.size = sizeof(vertices);
			buffer_desc.usage = GpuBufferUsage_Vertex | GpuBufferUsage_TransferDst;
			GpuBufferId vertex_buffer;
			ok &= Check(device.CreateBuffer(buffer_desc, vertex_buffer) == GpuResult::Ok, "vertex buffer should create");
			ok &= Check(device.WriteBuffer(vertex_buffer, 0, vertices, sizeof(vertices)) == GpuResult::Ok, "vertex data should upload");

			GpuTextureDesc target_desc;
			target_desc.size = Size(64, 64);
			target_desc.format = GpuFormat::RGBA8;
			target_desc.usage = GpuTextureUsage_ColorAttachment | GpuTextureUsage_Sampled;
			GpuTextureId target;
			ok &= Check(device.CreateTexture(target_desc, target) == GpuResult::Ok, "color target should create");

			GpuShaderDesc vs_desc;
			vs_desc.stage = GpuShaderStage::Vertex;
			vs_desc.format = GpuShaderFormat::SpirV;
			vs_desc.code = SpirV(kVertexShader, (int)(sizeof(kVertexShader) / sizeof(kVertexShader[0])));
			GpuShaderDesc fs_desc;
			fs_desc.stage = GpuShaderStage::Fragment;
			fs_desc.format = GpuShaderFormat::SpirV;
			fs_desc.code = SpirV(kFragmentShader, (int)(sizeof(kFragmentShader) / sizeof(kFragmentShader[0])));
			GpuShaderId vertex_shader, fragment_shader;
			ok &= Check(device.CreateShader(vs_desc, vertex_shader) == GpuResult::Ok, "vertex shader module should create");
			ok &= Check(device.CreateShader(fs_desc, fragment_shader) == GpuResult::Ok, "fragment shader module should create");

			GpuShaderDesc bad_shader = vs_desc;
			bad_shader.code = "bad";
			GpuShaderId invalid_shader;
			ok &= Check(device.CreateShader(bad_shader, invalid_shader) == GpuResult::InvalidArgument && !invalid_shader.IsValid(),
			            "invalid SPIR-V payload should be rejected deterministically");

			GpuPipelineDesc pipeline_desc;
			pipeline_desc.topology = GpuPrimitiveTopology::TriangleList;
			pipeline_desc.color_format = GpuFormat::RGBA8;
			pipeline_desc.vertex_shader = vertex_shader;
			pipeline_desc.fragment_shader = fragment_shader;
			pipeline_desc.vertex_layout = GpuVertexLayout::Position2Color4F;
			GpuPipelineId pipeline;
			ok &= Check(device.CreatePipeline(pipeline_desc, pipeline) == GpuResult::Ok, "dynamic-rendering graphics pipeline should create");
			ok &= Check(device.DestroyShader(vertex_shader) == GpuResult::InvalidState,
			            "shader destruction while referenced by a pipeline should be refused");

			for(int pass = 0; pass < 2; ++pass) {
				GpuCommandListId commands;
				ok &= Check(device.BeginCommands(commands) == GpuResult::Ok && commands.IsValid(), "command list should begin");
				GpuRenderPassDesc render_pass;
				render_pass.color_target = target;
				render_pass.color_format = GpuFormat::RGBA8;
				render_pass.color_load = pass == 0 ? GpuLoadOp::Clear : GpuLoadOp::Load;
				render_pass.color_store = GpuStoreOp::Store;
				render_pass.clear_color.red = 0.02f;
				render_pass.clear_color.green = 0.04f;
				render_pass.clear_color.blue = 0.08f;
				ok &= Check(device.BeginRenderPass(commands, render_pass) == GpuResult::Ok, "dynamic render pass should begin");
				if(pass == 0)
					ok &= Check(device.Draw(commands, 3) == GpuResult::InvalidState, "draw before pipeline/buffer should be rejected");
				ok &= Check(device.SetPipeline(commands, pipeline) == GpuResult::Ok, "pipeline should bind");
				ok &= Check(device.SetVertexBuffer(commands, vertex_buffer) == GpuResult::Ok, "vertex buffer should bind");
				ok &= Check(device.Draw(commands, 3) == GpuResult::Ok, "real vkCmdDraw recording should succeed");
				ok &= Check(device.DestroyBuffer(vertex_buffer) == GpuResult::InvalidState,
				            "resource destruction while command work is live should be refused");
				ok &= Check(device.EndRenderPass(commands) == GpuResult::Ok, "render pass should end");
				ok &= Check(device.EndCommands(commands) == GpuResult::Ok, "command list should end");
				ok &= Check(device.Submit(commands) == GpuResult::Ok, "synchronous graphics submission should succeed");
				ok &= Check(device.GetLiveCommandCount() == 0, "Submit should consume command-list ownership");
			}

			GpuTextureId multi_target;
			ok &= Check(device.CreateTexture(target_desc, multi_target) == GpuResult::Ok, "multi-pass color target should create");
			GpuCommandListId multi_commands;
			ok &= Check(device.BeginCommands(multi_commands) == GpuResult::Ok, "multi-pass command list should begin");
			for(int pass = 0; pass < 2; ++pass) {
				GpuRenderPassDesc render_pass;
				render_pass.color_target = multi_target;
				render_pass.color_format = GpuFormat::RGBA8;
				render_pass.color_load = pass == 0 ? GpuLoadOp::Clear : GpuLoadOp::Load;
				render_pass.color_store = GpuStoreOp::Store;
				ok &= Check(device.BeginRenderPass(multi_commands, render_pass) == GpuResult::Ok,
				            "multiple render passes in one command list should preserve pending layout/initialization state");
				if(pass == 1)
					ok &= Check(device.Draw(multi_commands, 3) == GpuResult::InvalidState,
					            "a later render pass must not inherit prior pipeline/vertex bindings");
				ok &= Check(device.SetPipeline(multi_commands, pipeline) == GpuResult::Ok, "multi-pass pipeline should bind");
				ok &= Check(device.SetVertexBuffer(multi_commands, vertex_buffer) == GpuResult::Ok, "multi-pass vertex buffer should bind");
				ok &= Check(device.Draw(multi_commands, 3) == GpuResult::Ok, "multi-pass draw should record");
				ok &= Check(device.EndRenderPass(multi_commands) == GpuResult::Ok, "multi-pass render pass should end");
			}
			ok &= Check(device.EndCommands(multi_commands) == GpuResult::Ok, "multi-pass command list should end");
			ok &= Check(device.Submit(multi_commands) == GpuResult::Ok, "multi-pass command list should submit");
			ok &= Check(device.DestroyTexture(multi_target) == GpuResult::Ok, "multi-pass color target should destroy");

			ok &= Check(device.DestroyPipeline(pipeline) == GpuResult::Ok, "pipeline should destroy");
			ok &= Check(device.DestroyShader(vertex_shader) == GpuResult::Ok, "vertex shader should destroy after pipeline");
			ok &= Check(device.DestroyShader(fragment_shader) == GpuResult::Ok, "fragment shader should destroy after pipeline");
			ok &= Check(device.DestroyTexture(target) == GpuResult::Ok, "color target should destroy");
			ok &= Check(device.DestroyBuffer(vertex_buffer) == GpuResult::Ok, "vertex buffer should destroy");
			ok &= Check(device.GetLiveBufferCount() == 0 && device.GetLiveTextureCount() == 0 &&
			            device.GetLiveShaderCount() == 0 && device.GetLivePipelineCount() == 0 && device.GetLiveCommandCount() == 0,
			            "explicit S17C-B1 destruction should leave zero adapter-owned resources");
		}
		session.Close();
		ok &= Check(session.GetReport().validation_warning_count == 0, "S17C-B1 graphics path should emit zero validation warnings");
		ok &= Check(session.GetReport().validation_error_count == 0, "S17C-B1 graphics path should emit zero validation errors");
	}
	else
		session.Close();

	if(hwnd)
		DestroyWindow(hwnd);
	const VulkanRuntimeDeviceDiagnostics diag = GetVulkanRuntimeDeviceDiagnostics();
	ok &= Check(diag.runtime_live_count == 0 && diag.instance_live_count == 0 && diag.debug_messenger_live_count == 0 &&
	            diag.surface_live_count == 0 && diag.device_live_count == 0 && diag.swapchain_live_count == 0,
	            "S17C-B1 graphics test should finish with zero Vulkan ownership diagnostics");

	if(ok) {
		Cout() << "RenderVulkanGraphicsTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
