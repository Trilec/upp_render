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

static GpuFormat TestGpuFormat(VkFormat format)
{
	switch(format) {
	case VK_FORMAT_R8G8B8A8_UNORM: return GpuFormat::RGBA8;
	case VK_FORMAT_B8G8R8A8_UNORM: return GpuFormat::BGRA8;
	case VK_FORMAT_R8G8B8A8_SRGB: return GpuFormat::RGBA8Srgb;
	case VK_FORMAT_B8G8R8A8_SRGB: return GpuFormat::BGRA8Srgb;
	default: return GpuFormat::Unknown;
	}
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
			            "S17C adapter should advertise implemented graphics capabilities");

			GpuSurfaceId invalid_surface;
			GpuSurfaceDesc invalid_surface_desc;
			ok &= Check(device.CreateSurface(invalid_surface_desc, invalid_surface) == GpuResult::InvalidArgument && !invalid_surface.IsValid(),
			            "neutral Vulkan surface should reject an incomplete descriptor");

			GpuSurfaceId surface;
			GpuSurfaceDesc surface_desc;
			surface_desc.label = "neutral-session-surface";
			surface_desc.size = Size(64, 64);
			surface_desc.native_window.kind = GpuNativeWindowKind::Win32;
			surface_desc.native_window.handle = (uintptr_t)hwnd;
			ok &= Check(device.CreateSurface(surface_desc, surface) == GpuResult::Ok && surface.IsValid(),
			            "neutral Vulkan surface should bind the borrowed session window");
			ok &= Check(device.GetLiveSurfaceCount() == 1, "adapter should expose one logical surface");
			GpuSurfaceId duplicate_surface;
			ok &= Check(device.CreateSurface(surface_desc, duplicate_surface) == GpuResult::InvalidState && !duplicate_surface.IsValid(),
			            "adapter should refuse a second logical surface for one borrowed session");

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
			GpuPipelineDesc invert_pipeline_desc = pipeline_desc;
			invert_pipeline_desc.blend_mode = GpuBlendMode::DestinationInvert;
			invert_pipeline_desc.label = "destination-invert graphics pipeline";
			GpuPipelineId invert_pipeline;
			ok &= Check(device.CreatePipeline(invert_pipeline_desc, invert_pipeline) == GpuResult::Ok,
			            "destination-invert graphics pipeline should create");
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
				ok &= Check(device.SetPipeline(commands, invert_pipeline) == GpuResult::Ok,
				            "destination-invert pipeline should bind for a real draw");
				ok &= Check(device.Draw(commands, 3) == GpuResult::Ok,
				            "destination-invert pipeline draw should record");
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

			GpuSwapchainDesc swapchain_desc;
			swapchain_desc.label = "neutral-session-swapchain";
			swapchain_desc.surface = surface;
			swapchain_desc.size = Size(64, 64);
			swapchain_desc.color_format = GpuFormat::RGBA8;
			swapchain_desc.image_count = 2;
			GpuSwapchainId swapchain;
			ok &= Check(device.CreateSwapchain(swapchain_desc, swapchain) == GpuResult::Ok && swapchain.IsValid(),
			            "neutral swapchain should bind the session-owned Vulkan swapchain");
			ok &= Check(device.GetLiveSwapchainCount() == 1 && session.HasSwapchain(),
			            "logical and session swapchain ownership should agree");
			ok &= Check(device.DestroySurface(surface) == GpuResult::InvalidState,
			            "surface destruction should be refused while its logical swapchain is live");
			GpuSwapchainId duplicate_swapchain;
			ok &= Check(device.CreateSwapchain(swapchain_desc, duplicate_swapchain) == GpuResult::InvalidState && !duplicate_swapchain.IsValid(),
			            "duplicate neutral swapchain creation should be refused");

			const GpuFormat negotiated_format = TestGpuFormat(session.GetReport().swapchain_format);
			ok &= Check(negotiated_format != GpuFormat::Unknown, "session swapchain format should have an exact neutral identity");

			GpuFrameInfo first_frame;
			ok &= Check(device.BeginFrame(swapchain, first_frame) == GpuResult::Ok && first_frame.frame.IsValid() && first_frame.color_target.IsValid(),
			            "neutral BeginFrame should acquire a borrowed session image");
			ok &= Check(first_frame.color_format == negotiated_format, "frame should report the exact negotiated swapchain format");
			ok &= Check(first_frame.size.cx == session.GetReport().swapchain_extent.cx && first_frame.size.cy == session.GetReport().swapchain_extent.cy,
			            "frame should report the exact session swapchain extent");
			ok &= Check(device.GetLiveFrameCount() == 1, "one neutral frame should be active after acquisition");
			GpuFrameInfo duplicate_frame;
			ok &= Check(device.BeginFrame(swapchain, duplicate_frame) == GpuResult::InvalidState,
			            "a second neutral frame should be refused while one is active");
			byte backbuffer_data[4] = {};
			GpuTextureWriteDesc backbuffer_write;
			backbuffer_write.size = Size(1, 1);
			backbuffer_write.row_pitch = 4;
			ok &= Check(device.WriteTexture(first_frame.color_target, backbuffer_write, backbuffer_data, sizeof(backbuffer_data)) == GpuResult::InvalidState,
			            "borrowed swapchain images must not accept texture uploads");
			ok &= Check(device.DestroyTexture(first_frame.color_target) == GpuResult::InvalidState,
			            "borrowed swapchain images must not be destroyed through GpuDevice");
			ok &= Check(device.Present(first_frame.frame) == GpuResult::Ok, "no-render neutral frame should present through the session path");
			ok &= Check(device.GetLiveFrameCount() == 0, "no frame should remain active after presentation");

			GpuFrameInfo load_guard_frame;
			ok &= Check(device.BeginFrame(swapchain, load_guard_frame) == GpuResult::Ok, "load-guard frame should acquire");
			ok &= Check(load_guard_frame.color_format == negotiated_format, "load-guard frame format should remain negotiated format");
			GpuPipelineDesc swap_pipeline_desc = pipeline_desc;
			swap_pipeline_desc.color_format = load_guard_frame.color_format;
			GpuPipelineId swap_pipeline;
			ok &= Check(device.CreatePipeline(swap_pipeline_desc, swap_pipeline) == GpuResult::Ok,
			            "swapchain-format dynamic-rendering pipeline should create");

			GpuCommandListId load_guard_commands;
			ok &= Check(device.BeginCommands(load_guard_commands) == GpuResult::Ok, "load-guard command list should begin");
			GpuRenderPassDesc invalid_load_pass;
			invalid_load_pass.color_target = load_guard_frame.color_target;
			invalid_load_pass.color_format = load_guard_frame.color_format;
			invalid_load_pass.color_load = GpuLoadOp::Load;
			invalid_load_pass.color_store = GpuStoreOp::Store;
			ok &= Check(device.BeginRenderPass(load_guard_commands, invalid_load_pass) == GpuResult::InvalidState,
			            "Load should reject an acquired swapchain image with no stored neutral content");
			ok &= Check(device.Present(load_guard_frame.frame) == GpuResult::Ok,
			            "failed backbuffer Load must not falsely bind unrelated command work to the frame");
			GpuRenderPassDesc unrelated_pass;
			unrelated_pass.color_target = target;
			unrelated_pass.color_format = GpuFormat::RGBA8;
			unrelated_pass.color_load = GpuLoadOp::Load;
			unrelated_pass.color_store = GpuStoreOp::Store;
			ok &= Check(device.BeginRenderPass(load_guard_commands, unrelated_pass) == GpuResult::Ok,
			            "command list should remain usable for unrelated work after failed backbuffer Load");
			ok &= Check(device.SetPipeline(load_guard_commands, pipeline) == GpuResult::Ok, "unrelated pipeline should bind after failed Load");
			ok &= Check(device.SetVertexBuffer(load_guard_commands, vertex_buffer) == GpuResult::Ok, "unrelated vertex buffer should bind after failed Load");
			ok &= Check(device.Draw(load_guard_commands, 3) == GpuResult::Ok, "unrelated draw should record after failed Load");
			ok &= Check(device.EndRenderPass(load_guard_commands) == GpuResult::Ok, "unrelated render pass should end");
			ok &= Check(device.EndCommands(load_guard_commands) == GpuResult::Ok, "load-guard command list should end");
			ok &= Check(device.Submit(load_guard_commands) == GpuResult::Ok, "unrelated work should submit after frame presentation");

			GpuFrameInfo rendered_frame;
			ok &= Check(device.BeginFrame(swapchain, rendered_frame) == GpuResult::Ok, "rendered frame should acquire");
			ok &= Check(rendered_frame.color_format == negotiated_format, "rendered frame should preserve negotiated format");
			GpuCommandListId frame_commands;
			ok &= Check(device.BeginCommands(frame_commands) == GpuResult::Ok, "frame command list should begin");
			GpuRenderPassDesc discard_pass;
			discard_pass.color_target = rendered_frame.color_target;
			discard_pass.color_format = rendered_frame.color_format;
			discard_pass.color_load = GpuLoadOp::Clear;
			discard_pass.color_store = GpuStoreOp::DontCare;
			ok &= Check(device.BeginRenderPass(frame_commands, discard_pass) == GpuResult::Ok, "discarding swapchain pass should begin");
			ok &= Check(device.EndRenderPass(frame_commands) == GpuResult::Ok, "discarding swapchain pass should end");
			GpuRenderPassDesc load_after_discard = discard_pass;
			load_after_discard.color_load = GpuLoadOp::Load;
			load_after_discard.color_store = GpuStoreOp::Store;
			ok &= Check(device.BeginRenderPass(frame_commands, load_after_discard) == GpuResult::InvalidState,
			            "StoreOp::DontCare must not make later Load legal within the same command list");
			GpuRenderPassDesc draw_pass = discard_pass;
			draw_pass.color_store = GpuStoreOp::Store;
			ok &= Check(device.BeginRenderPass(frame_commands, draw_pass) == GpuResult::Ok, "stored swapchain render pass should begin");
			ok &= Check(device.SetPipeline(frame_commands, swap_pipeline) == GpuResult::Ok, "swapchain pipeline should bind");
			ok &= Check(device.SetVertexBuffer(frame_commands, vertex_buffer) == GpuResult::Ok, "swapchain vertex buffer should bind");
			ok &= Check(device.Draw(frame_commands, 3) == GpuResult::Ok, "swapchain draw should record");
			ok &= Check(device.EndRenderPass(frame_commands) == GpuResult::Ok, "stored swapchain render pass should end");
			ok &= Check(device.EndCommands(frame_commands) == GpuResult::Ok, "frame command list should end");
			ok &= Check(device.Present(rendered_frame.frame) == GpuResult::InvalidState,
			            "Present should refuse active frame command work before submission");
			ok &= Check(device.ResizeSwapchain(swapchain, Size(96, 80)) == GpuResult::InvalidState,
			            "ResizeSwapchain should refuse an active frame");
			ok &= Check(device.DestroySwapchain(swapchain) == GpuResult::InvalidState,
			            "DestroySwapchain should refuse an active frame");
			ok &= Check(device.Submit(frame_commands) == GpuResult::Ok, "frame command work should submit synchronously");
			ok &= Check(device.Present(rendered_frame.frame) == GpuResult::Ok, "externally rendered frame should present through session handoff");
			ok &= Check(device.Present(rendered_frame.frame) == GpuResult::InvalidState, "presenting the same neutral frame twice should fail");

			const uint64_t pre_resize_swapchain_id = session.GetReport().swapchain_id;
			ok &= Check(device.ResizeSwapchain(swapchain, Size(96, 80)) == GpuResult::Ok, "neutral swapchain resize should recreate session swapchain");
			ok &= Check(session.GetReport().swapchain_id != 0 && session.GetReport().swapchain_id != pre_resize_swapchain_id,
			            "resize should produce a new session swapchain identity");
			GpuFrameInfo resized_frame;
			ok &= Check(device.BeginFrame(swapchain, resized_frame) == GpuResult::Ok, "frame should acquire after resize");
			ok &= Check(resized_frame.size.cx == session.GetReport().swapchain_extent.cx && resized_frame.size.cy == session.GetReport().swapchain_extent.cy,
			            "resized frame should report recreated session extent");
			ok &= Check(resized_frame.color_format == TestGpuFormat(session.GetReport().swapchain_format),
			            "resized frame should report exact recreated swapchain format");
			GpuCommandListId resized_commands;
			ok &= Check(device.BeginCommands(resized_commands) == GpuResult::Ok, "resized-frame command list should begin");
			GpuRenderPassDesc resized_load;
			resized_load.color_target = resized_frame.color_target;
			resized_load.color_format = resized_frame.color_format;
			resized_load.color_load = GpuLoadOp::Load;
			resized_load.color_store = GpuStoreOp::Store;
			ok &= Check(device.BeginRenderPass(resized_commands, resized_load) == GpuResult::InvalidState,
			            "swapchain recreation should reset neutral per-image content validity");
			ok &= Check(device.EndCommands(resized_commands) == GpuResult::Ok, "resized-frame empty command list should end");
			ok &= Check(device.Submit(resized_commands) == GpuResult::Ok, "resized-frame empty command list should submit");
			ok &= Check(device.Present(resized_frame.frame) == GpuResult::Ok, "resized no-render frame should present");
			ok &= Check(device.DestroySwapchain(swapchain) == GpuResult::Ok, "logical swapchain should destroy through session authority");
			ok &= Check(device.DestroySurface(surface) == GpuResult::Ok, "logical surface should destroy after swapchain");
			ok &= Check(device.GetLiveSurfaceCount() == 0 && device.GetLiveSwapchainCount() == 0 && device.GetLiveFrameCount() == 0,
			            "neutral surface/swapchain/frame logical ownership should be fully released");

			ok &= Check(device.DestroyPipeline(swap_pipeline) == GpuResult::Ok, "swapchain pipeline should destroy");
			ok &= Check(device.DestroyPipeline(invert_pipeline) == GpuResult::Ok,
			            "destination-invert pipeline should destroy before its shaders");
			ok &= Check(device.DestroyPipeline(pipeline) == GpuResult::Ok, "pipeline should destroy");
			ok &= Check(device.DestroyShader(vertex_shader) == GpuResult::Ok, "vertex shader should destroy after pipelines");
			ok &= Check(device.DestroyShader(fragment_shader) == GpuResult::Ok, "fragment shader should destroy after pipeline");
			ok &= Check(device.DestroyTexture(target) == GpuResult::Ok, "color target should destroy");
			ok &= Check(device.DestroyBuffer(vertex_buffer) == GpuResult::Ok, "vertex buffer should destroy");
			ok &= Check(device.GetLiveBufferCount() == 0 && device.GetLiveTextureCount() == 0 &&
			            device.GetLiveShaderCount() == 0 && device.GetLivePipelineCount() == 0 && device.GetLiveCommandCount() == 0,
			            "explicit S17C graphics destruction should leave zero adapter-owned resources");
		}
		session.Close();
		ok &= Check(session.GetReport().validation_warning_count == 0, "S17C graphics path should emit zero validation warnings");
		ok &= Check(session.GetReport().validation_error_count == 0, "S17C graphics path should emit zero validation errors");
	}
	else
		session.Close();

	if(hwnd)
		DestroyWindow(hwnd);
	const VulkanRuntimeDeviceDiagnostics diag = GetVulkanRuntimeDeviceDiagnostics();
	ok &= Check(diag.runtime_live_count == 0 && diag.instance_live_count == 0 && diag.debug_messenger_live_count == 0 &&
	            diag.surface_live_count == 0 && diag.device_live_count == 0 && diag.swapchain_live_count == 0,
	            "S17C graphics test should finish with zero Vulkan ownership diagnostics");

	if(ok) {
		Cout() << "RenderVulkanGraphicsTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
