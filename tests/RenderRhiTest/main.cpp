#include <RenderCanvas/RenderCanvas.h>
#include <RenderNull/RenderNull.h>

#include <limits>

using namespace Upp;

static bool Check(bool cond, const char *msg)
{
	if(!cond)
		Cout() << "FAIL: " << msg << EOL;
	return cond;
}

static GpuSurfaceDesc MakeSurfaceDesc(Size size)
{
	GpuSurfaceDesc desc;
	desc.label = "surface";
	desc.size = size;
	desc.native_window.kind = GpuNativeWindowKind::None;
	desc.native_window.handle = 0;
	return desc;
}

static GpuSurfaceDesc MakeSurfaceDesc(Size size, GpuNativeWindowKind kind, uintptr_t handle)
{
	GpuSurfaceDesc desc = MakeSurfaceDesc(size);
	desc.native_window.kind = kind;
	desc.native_window.handle = handle;
	return desc;
}

static GpuSwapchainDesc MakeSwapchainDesc(GpuSurfaceId surface, Size size)
{
	GpuSwapchainDesc desc;
	desc.label = "swapchain";
	desc.surface = surface;
	desc.size = size;
	desc.color_format = GpuFormat::RGBA8;
	desc.image_count = 2;
	return desc;
}

static GpuBufferDesc MakeVertexBufferDesc(int64 size)
{
	GpuBufferDesc desc;
	desc.size = size;
	desc.usage = GpuBufferUsage_Vertex;
	return desc;
}

static GpuTextureDesc MakeTextureDesc(Size size)
{
	GpuTextureDesc desc;
	desc.size = size;
	desc.format = GpuFormat::RGBA8;
	desc.usage = GpuTextureUsage_ColorAttachment;
	return desc;
}

static GpuRenderPassDesc MakeRenderPassDesc(GpuTextureId color_target, GpuLoadOp load, const char *label)
{
	GpuRenderPassDesc desc;
	desc.color_target = color_target;
	desc.color_format = GpuFormat::RGBA8;
	desc.color_load = load;
	desc.color_store = GpuStoreOp::Store;
	desc.clear_color.red = 0.1f;
	desc.clear_color.green = 0.2f;
	desc.clear_color.blue = 0.3f;
	desc.clear_color.alpha = 1.0f;
	desc.label = label;
	return desc;
}

static String MakeSpirVStub()
{
	static const byte data[] = {
		0x03, 0x02, 0x23, 0x07,
		0x00, 0x00, 0x01, 0x00,
	};
	String out;
	out.Cat(reinterpret_cast<const char *>(data), (int)sizeof(data));
	return out;
}

static GpuShaderDesc MakeShaderDesc(GpuShaderStage stage)
{
	GpuShaderDesc desc;
	desc.stage = stage;
	desc.format = GpuShaderFormat::SpirV;
	desc.code = MakeSpirVStub();
	desc.entry_point = "main";
	desc.label = stage == GpuShaderStage::Vertex ? "vertex" : "fragment";
	return desc;
}

static GpuPipelineDesc MakePipelineDesc(GpuShaderId vertex_shader, GpuShaderId fragment_shader)
{
	GpuPipelineDesc desc;
	desc.topology = GpuPrimitiveTopology::TriangleList;
	desc.color_format = GpuFormat::RGBA8;
	desc.vertex_shader = vertex_shader;
	desc.fragment_shader = fragment_shader;
	desc.vertex_layout = GpuVertexLayout::Position2Color4F;
	desc.label = "basic";
	return desc;
}

static bool CreateBasicShaders(NullGpuDevice& device, GpuShaderId& vertex_shader, GpuShaderId& fragment_shader)
{
	return device.CreateShader(MakeShaderDesc(GpuShaderStage::Vertex), vertex_shader) == GpuResult::Ok &&
	       device.CreateShader(MakeShaderDesc(GpuShaderStage::Fragment), fragment_shader) == GpuResult::Ok;
}

static bool CreateBasicPipeline(NullGpuDevice& device, GpuPipelineId& pipeline)
{
	GpuShaderId vertex_shader, fragment_shader;
	if(!CreateBasicShaders(device, vertex_shader, fragment_shader))
		return false;
	return device.CreatePipeline(MakePipelineDesc(vertex_shader, fragment_shader), pipeline) == GpuResult::Ok;
}

static bool TestHandles()
{
	GpuBufferId buffer;
	GpuTextureId texture;
	GpuShaderId shader;
	GpuPipelineId pipeline;
	GpuCommandListId commands;
	GpuAdapterId adapter;
	GpuDeviceId device;
	GpuSurfaceId surface;
	GpuSwapchainId swapchain;
	GpuFrameId frame;
	GpuNativeWindowDesc none_window;
	GpuNativeWindowDesc win32_window;
	win32_window.kind = GpuNativeWindowKind::Win32;
	win32_window.handle = 1;
	return Check(!buffer.IsValid(), "default buffer invalid") &&
	       Check(!texture.IsValid(), "default texture invalid") &&
	       Check(!shader.IsValid(), "default shader invalid") &&
	       Check(!pipeline.IsValid(), "default pipeline invalid") &&
	       Check(!commands.IsValid(), "default command list invalid") &&
	       Check(!adapter.IsValid(), "default adapter invalid") &&
	       Check(!device.IsValid(), "default device invalid") &&
	       Check(!surface.IsValid(), "default surface invalid") &&
	       Check(!swapchain.IsValid(), "default swapchain invalid") &&
	       Check(!frame.IsValid(), "default frame invalid") &&
	       Check(none_window.IsValid(), "none native window should be valid") &&
	       Check(win32_window.IsValid(), "win32 native window should be valid") &&
	       Check(DumpGpuNativeWindowDesc(none_window).Find("handle=unset") >= 0, "native window dump should redact handle") &&
	       Check(DumpGpuNativeWindowDesc(win32_window).Find("handle=set") >= 0, "win32 native window dump should redact handle") &&
	       Check(DumpGpuShaderStage(GpuShaderStage::Vertex) == "Vertex", "shader stage dump deterministic") &&
	       Check(DumpGpuShaderFormat(GpuShaderFormat::SpirV) == "SpirV", "shader format dump deterministic") &&
	       Check(DumpGpuVertexLayout(GpuVertexLayout::Position2Color4F) == "Position2Color4F", "vertex layout dump deterministic") &&
	       Check(buffer.Dump() == "Buffer#0", "buffer dump deterministic");
}

static bool TestSurfaceLifecycle(NullGpuDevice& device)
{
	GpuSurfaceId surface;
	if(!Check(device.CreateSurface(MakeSurfaceDesc(Size(640, 480)), surface) == GpuResult::Ok, "surface create should succeed")) return false;
	if(!Check(surface.IsValid(), "surface handle valid")) return false;

	GpuSurfaceId zero_surface;
	if(!Check(device.CreateSurface(MakeSurfaceDesc(Size(0, 480)), zero_surface) == GpuResult::InvalidArgument, "zero width surface should fail")) return false;
	GpuSurfaceId bad_handle_surface;
	if(!Check(device.CreateSurface(MakeSurfaceDesc(Size(640, 480), GpuNativeWindowKind::None, 1), bad_handle_surface) == GpuResult::InvalidArgument, "none window with handle should fail")) return false;
	if(!Check(device.CreateSurface(MakeSurfaceDesc(Size(640, 480), GpuNativeWindowKind::Win32, 0), bad_handle_surface) == GpuResult::InvalidArgument, "win32 window with zero handle should fail")) return false;
	if(!Check(device.CreateSurface(MakeSurfaceDesc(Size(640, 480), static_cast<GpuNativeWindowKind>(99), 1), bad_handle_surface) == GpuResult::InvalidArgument, "unknown native window kind should fail")) return false;
	if(!Check(device.DestroySurface(surface) == GpuResult::Ok, "surface destroy should succeed")) return false;
	if(!Check(device.DestroySurface(surface) == GpuResult::InvalidHandle, "double surface destroy should fail")) return false;
	GpuSurfaceId unknown_surface;
	unknown_surface.value = 99;
	if(!Check(device.DestroySurface(unknown_surface) == GpuResult::InvalidHandle, "unknown surface destroy should fail")) return false;
	return true;
}

static bool TestSwapchainLifecycle(NullGpuDevice& device)
{
	GpuSurfaceId surface;
	if(!Check(device.CreateSurface(MakeSurfaceDesc(Size(640, 480)), surface) == GpuResult::Ok, "swapchain surface should create")) return false;

	GpuSwapchainId swapchain;
	if(!Check(device.CreateSwapchain(MakeSwapchainDesc(surface, Size(640, 480)), swapchain) == GpuResult::Ok, "swapchain create should succeed")) return false;
	if(!Check(swapchain.IsValid(), "swapchain handle valid")) return false;
	if(!Check(device.DestroySurface(surface) == GpuResult::InvalidState, "destroy surface with live swapchain should fail")) return false;

	GpuSwapchainDesc invalid_surface = MakeSwapchainDesc(surface, Size(640, 480));
	invalid_surface.surface.value = 77;
	GpuSwapchainId tmp;
	if(!Check(device.CreateSwapchain(invalid_surface, tmp) == GpuResult::InvalidHandle, "invalid surface swapchain should fail")) return false;

	GpuSwapchainDesc zero_size = MakeSwapchainDesc(surface, Size(0, 480));
	if(!Check(device.CreateSwapchain(zero_size, tmp) == GpuResult::InvalidArgument, "zero width swapchain should fail")) return false;

	GpuSwapchainDesc bad_format = MakeSwapchainDesc(surface, Size(640, 480));
	bad_format.color_format = GpuFormat::Unknown;
	if(!Check(device.CreateSwapchain(bad_format, tmp) == GpuResult::InvalidArgument, "unknown format swapchain should fail")) return false;

	GpuSwapchainDesc too_few_images = MakeSwapchainDesc(surface, Size(640, 480));
	too_few_images.image_count = 1;
	if(!Check(device.CreateSwapchain(too_few_images, tmp) == GpuResult::InvalidArgument, "image count < 2 should fail")) return false;

	if(!Check(device.ResizeSwapchain(swapchain, Size(800, 600)) == GpuResult::Ok, "swapchain resize should succeed")) return false;
	if(!Check(device.ResizeSwapchain(swapchain, Size(0, 600)) == GpuResult::InvalidArgument, "zero resize should fail")) return false;

	GpuCommandListId list;
	if(!Check(device.BeginCommands(list) == GpuResult::Ok, "begin commands for active frame guard should succeed")) return false;
	GpuFrameInfo frame;
	if(!Check(device.BeginFrame(swapchain, frame) == GpuResult::Ok, "begin frame should succeed")) return false;
	if(!Check(device.ResizeSwapchain(swapchain, Size(1024, 768)) == GpuResult::InvalidState, "resize while frame active should fail")) return false;
	if(!Check(device.DestroySwapchain(swapchain) == GpuResult::InvalidState, "destroy swapchain with active frame should fail")) return false;
	if(!Check(device.EndCommands(list) == GpuResult::Ok, "end guard commands should succeed")) return false;
	if(!Check(device.Present(frame.frame) == GpuResult::Ok, "present active frame should succeed")) return false;
	if(!Check(device.DestroySwapchain(swapchain) == GpuResult::Ok, "destroy swapchain should succeed after present")) return false;
	if(!Check(device.DestroySurface(surface) == GpuResult::Ok, "destroy surface should succeed after swapchain destroy")) return false;
	return true;
}

static bool TestFrameLifecycle(NullGpuDevice& device)
{
	GpuSurfaceId surface;
	GpuSwapchainId swapchain;
	if(!Check(device.CreateSurface(MakeSurfaceDesc(Size(320, 240)), surface) == GpuResult::Ok, "frame surface should create")) return false;
	if(!Check(device.CreateSwapchain(MakeSwapchainDesc(surface, Size(320, 240)), swapchain) == GpuResult::Ok, "frame swapchain should create")) return false;

	GpuFrameInfo frame;
	if(!Check(device.BeginFrame(swapchain, frame) == GpuResult::Ok, "begin frame should succeed")) return false;
	if(!Check(frame.frame.IsValid(), "frame handle valid")) return false;
	if(!Check(frame.swapchain == swapchain, "frame swapchain should match")) return false;
	if(!Check(frame.color_target.IsValid(), "frame color target valid")) return false;
	if(!Check(frame.color_format == GpuFormat::RGBA8, "frame color format valid")) return false;

	GpuFrameInfo second_frame;
	if(!Check(device.BeginFrame(swapchain, second_frame) == GpuResult::InvalidState, "begin frame twice should fail")) return false;

	GpuPipelineId pipeline;
	GpuBufferId buffer;
	if(!Check(CreateBasicPipeline(device, pipeline), "frame pipeline should create")) return false;
	if(!Check(device.CreateBuffer(MakeVertexBufferDesc(64), buffer) == GpuResult::Ok, "frame buffer should create")) return false;

	GpuCommandListId list;
	if(!Check(device.BeginCommands(list) == GpuResult::Ok, "frame commands should begin")) return false;
	if(!Check(device.BeginRenderPass(list, MakeRenderPassDesc(frame.color_target, GpuLoadOp::Clear, "frame")) == GpuResult::Ok, "render pass to frame target should succeed")) return false;
	if(!Check(device.SetPipeline(list, pipeline) == GpuResult::Ok, "set pipeline in frame should succeed")) return false;
	if(!Check(device.SetVertexBuffer(list, buffer) == GpuResult::Ok, "set vertex buffer in frame should succeed")) return false;
	if(!Check(device.Draw(list, 3, 0) == GpuResult::Ok, "draw in frame should succeed")) return false;
	if(!Check(device.EndRenderPass(list) == GpuResult::Ok, "end render pass in frame should succeed")) return false;
	if(!Check(device.EndCommands(list) == GpuResult::Ok, "end commands in frame should succeed")) return false;
	if(!Check(device.Submit(list) == GpuResult::Ok, "submit in frame should succeed")) return false;
	if(!Check(device.Present(frame.frame) == GpuResult::Ok, "present should succeed")) return false;
	if(!Check(device.Present(frame.frame) == GpuResult::InvalidState, "present twice should fail")) return false;
	GpuFrameId unknown_frame;
	unknown_frame.value = 99;
	if(!Check(device.Present(unknown_frame) == GpuResult::InvalidHandle, "unknown frame present should fail")) return false;

	GpuCommandListId post_present_list;
	if(!Check(device.BeginCommands(post_present_list) == GpuResult::Ok, "post present commands should begin")) return false;
	if(!Check(device.BeginRenderPass(post_present_list, MakeRenderPassDesc(frame.color_target, GpuLoadOp::Load, "after")) == GpuResult::InvalidState, "render pass after present should fail")) return false;
	if(!Check(device.EndCommands(post_present_list) == GpuResult::Ok, "post present commands should end")) return false;

	GpuFrameInfo active_frame;
	if(!Check(device.BeginFrame(swapchain, active_frame) == GpuResult::Ok, "second active frame should begin")) return false;
	if(!Check(device.DestroySwapchain(swapchain) == GpuResult::InvalidState, "destroy swapchain with active frame should fail")) return false;
	if(!Check(device.ResizeSwapchain(swapchain, Size(400, 300)) == GpuResult::InvalidState, "resize with active frame should fail")) return false;
	if(!Check(device.Present(active_frame.frame) == GpuResult::Ok, "present second frame should succeed")) return false;
	if(!Check(device.DestroySwapchain(swapchain) == GpuResult::Ok, "cleanup swapchain should succeed")) return false;
	if(!Check(device.DestroySurface(surface) == GpuResult::Ok, "cleanup surface should succeed")) return false;
	return true;
}

static bool TestResourceUploads(NullGpuDevice& device)
{
	byte buffer_data[16] = {};
	GpuBufferId buffer;
	if(!Check(device.CreateBuffer(MakeVertexBufferDesc(16), buffer) == GpuResult::Ok, "upload buffer should create")) return false;
	if(!Check(device.WriteBuffer(buffer, 4, buffer_data, 8) == GpuResult::Ok, "in-range buffer write should succeed")) return false;
	if(!Check(device.WriteBuffer(buffer, 0, nullptr, 4) == GpuResult::InvalidArgument, "null buffer write should fail")) return false;
	if(!Check(device.WriteBuffer(buffer, -1, buffer_data, 4) == GpuResult::InvalidArgument, "negative buffer offset should fail")) return false;
	if(!Check(device.WriteBuffer(buffer, 0, buffer_data, 0) == GpuResult::InvalidArgument, "zero buffer write should fail")) return false;
	if(!Check(device.WriteBuffer(buffer, 12, buffer_data, 8) == GpuResult::InvalidArgument, "out-of-range buffer write should fail")) return false;
	GpuBufferId unknown_buffer;
	unknown_buffer.value = 999;
	if(!Check(device.WriteBuffer(unknown_buffer, 0, buffer_data, 4) == GpuResult::InvalidHandle, "unknown buffer write should fail")) return false;
	if(!Check(device.DestroyBuffer(buffer) == GpuResult::Ok, "upload buffer should destroy")) return false;
	if(!Check(device.WriteBuffer(buffer, 0, buffer_data, 4) == GpuResult::InvalidHandle, "destroyed buffer write should fail")) return false;

	GpuTextureDesc texture_desc = MakeTextureDesc(Size(4, 3));
	texture_desc.usage = GpuTextureUsage_Sampled;
	GpuTextureId texture;
	if(!Check(device.CreateTexture(texture_desc, texture) == GpuResult::Ok, "upload texture should create")) return false;
	byte texture_data[64] = {};
	GpuTextureWriteDesc whole;
	whole.size = Size(4, 3);
	whole.row_pitch = 16;
	if(!Check(device.WriteTexture(texture, whole, texture_data, 48) == GpuResult::Ok, "tight texture upload should succeed")) return false;
	GpuTextureWriteDesc partial;
	partial.origin = Point(1, 1);
	partial.size = Size(2, 2);
	partial.row_pitch = 12;
	if(!Check(device.WriteTexture(texture, partial, texture_data, 20) == GpuResult::Ok, "padded partial texture upload should succeed")) return false;
	GpuTextureWriteDesc short_pitch = partial;
	short_pitch.row_pitch = 7;
	if(!Check(device.WriteTexture(texture, short_pitch, texture_data, 20) == GpuResult::InvalidArgument, "short texture row pitch should fail")) return false;
	if(!Check(device.WriteTexture(texture, partial, texture_data, 19) == GpuResult::InvalidArgument, "short texture data should fail")) return false;
	GpuTextureWriteDesc outside = partial;
	outside.origin = Point(3, 2);
	if(!Check(device.WriteTexture(texture, outside, texture_data, 20) == GpuResult::InvalidArgument, "out-of-range texture upload should fail")) return false;
	GpuTextureWriteDesc negative = partial;
	negative.origin = Point(-1, 0);
	if(!Check(device.WriteTexture(texture, negative, texture_data, 20) == GpuResult::InvalidArgument, "negative texture origin should fail")) return false;
	if(!Check(device.WriteTexture(texture, partial, nullptr, 20) == GpuResult::InvalidArgument, "null texture upload should fail")) return false;
	GpuTextureId unknown_texture;
	unknown_texture.value = 999;
	if(!Check(device.WriteTexture(unknown_texture, partial, texture_data, 20) == GpuResult::InvalidHandle, "unknown texture upload should fail")) return false;
	if(!Check(device.DestroyTexture(texture) == GpuResult::Ok, "upload texture should destroy")) return false;
	if(!Check(device.WriteTexture(texture, partial, texture_data, 20) == GpuResult::InvalidHandle, "destroyed texture upload should fail")) return false;

	GpuSurfaceId surface;
	GpuSwapchainId swapchain;
	if(!Check(device.CreateSurface(MakeSurfaceDesc(Size(32, 32)), surface) == GpuResult::Ok, "upload guard surface should create")) return false;
	if(!Check(device.CreateSwapchain(MakeSwapchainDesc(surface, Size(32, 32)), swapchain) == GpuResult::Ok, "upload guard swapchain should create")) return false;
	GpuFrameInfo frame;
	if(!Check(device.BeginFrame(swapchain, frame) == GpuResult::Ok, "upload guard frame should begin")) return false;
	GpuTextureWriteDesc backbuffer_write;
	backbuffer_write.size = Size(1, 1);
	backbuffer_write.row_pitch = 4;
	if(!Check(device.WriteTexture(frame.color_target, backbuffer_write, texture_data, 4) == GpuResult::InvalidState, "swapchain backbuffer upload should fail")) return false;
	if(!Check(device.Present(frame.frame) == GpuResult::Ok, "upload guard frame should present")) return false;
	if(!Check(device.DestroySwapchain(swapchain) == GpuResult::Ok, "upload guard swapchain should destroy")) return false;
	if(!Check(device.DestroySurface(surface) == GpuResult::Ok, "upload guard surface should destroy")) return false;
	return true;
}

static bool TestShaderLifecycle(NullGpuDevice& device)
{
	GpuShaderId vertex, fragment;
	if(!Check(device.CreateShader(MakeShaderDesc(GpuShaderStage::Vertex), vertex) == GpuResult::Ok, "vertex shader create should succeed")) return false;
	if(!Check(device.CreateShader(MakeShaderDesc(GpuShaderStage::Fragment), fragment) == GpuResult::Ok, "fragment shader create should succeed")) return false;
	if(!Check(vertex.IsValid() && fragment.IsValid(), "shader handles should be valid")) return false;

	GpuShaderDesc bad_stage = MakeShaderDesc(GpuShaderStage::Vertex);
	bad_stage.stage = GpuShaderStage::Unknown;
	GpuShaderId tmp;
	if(!Check(device.CreateShader(bad_stage, tmp) == GpuResult::InvalidArgument, "unknown shader stage should fail")) return false;

	GpuShaderDesc bad_format = MakeShaderDesc(GpuShaderStage::Vertex);
	bad_format.format = GpuShaderFormat::Unknown;
	if(!Check(device.CreateShader(bad_format, tmp) == GpuResult::InvalidArgument, "unknown shader format should fail")) return false;

	GpuShaderDesc bad_entry = MakeShaderDesc(GpuShaderStage::Vertex);
	bad_entry.entry_point.Clear();
	if(!Check(device.CreateShader(bad_entry, tmp) == GpuResult::InvalidArgument, "empty shader entry point should fail")) return false;

	GpuShaderDesc bad_magic = MakeShaderDesc(GpuShaderStage::Vertex);
	bad_magic.code = "bad!";
	if(!Check(device.CreateShader(bad_magic, tmp) == GpuResult::InvalidArgument, "invalid SPIR-V magic should fail")) return false;

	GpuShaderDesc bad_size = MakeShaderDesc(GpuShaderStage::Vertex);
	bad_size.code = "abc";
	if(!Check(device.CreateShader(bad_size, tmp) == GpuResult::InvalidArgument, "misaligned SPIR-V size should fail")) return false;

	if(!Check(device.DestroyShader(vertex) == GpuResult::Ok, "shader destroy should succeed")) return false;
	if(!Check(device.DestroyShader(vertex) == GpuResult::InvalidHandle, "double shader destroy should fail")) return false;
	GpuShaderId unknown;
	unknown.value = 999;
	if(!Check(device.DestroyShader(unknown) == GpuResult::InvalidHandle, "unknown shader destroy should fail")) return false;
	if(!Check(device.DestroyShader(fragment) == GpuResult::Ok, "fragment shader destroy should succeed")) return false;
	return true;
}

static bool TestPipelineLifecycle(NullGpuDevice& device)
{
	GpuShaderId vertex, fragment;
	if(!Check(CreateBasicShaders(device, vertex, fragment), "pipeline shaders should create")) return false;
	GpuPipelineDesc valid = MakePipelineDesc(vertex, fragment);
	GpuPipelineId pipeline;
	if(!Check(device.CreatePipeline(valid, pipeline) == GpuResult::Ok, "pipeline create should succeed")) return false;
	if(!Check(pipeline.IsValid(), "created pipeline valid")) return false;

	GpuPipelineDesc bad_format = valid;
	bad_format.color_format = GpuFormat::Unknown;
	GpuPipelineId tmp;
	if(!Check(device.CreatePipeline(bad_format, tmp) == GpuResult::InvalidArgument, "unknown format pipeline should fail")) return false;

	GpuPipelineDesc bad_topology = valid;
	bad_topology.topology = static_cast<GpuPrimitiveTopology>(99);
	if(!Check(device.CreatePipeline(bad_topology, tmp) == GpuResult::InvalidArgument, "invalid topology pipeline should fail")) return false;

	GpuPipelineDesc bad_layout = valid;
	bad_layout.vertex_layout = GpuVertexLayout::Unknown;
	if(!Check(device.CreatePipeline(bad_layout, tmp) == GpuResult::InvalidArgument, "unknown vertex layout should fail")) return false;

	GpuPipelineDesc missing_shader = valid;
	missing_shader.vertex_shader = GpuShaderId();
	if(!Check(device.CreatePipeline(missing_shader, tmp) == GpuResult::InvalidHandle, "missing pipeline shader should fail")) return false;

	GpuPipelineDesc swapped_stages = valid;
	swapped_stages.vertex_shader = fragment;
	swapped_stages.fragment_shader = vertex;
	if(!Check(device.CreatePipeline(swapped_stages, tmp) == GpuResult::InvalidArgument, "shader stage mismatch should fail")) return false;

	if(!Check(device.DestroyShader(vertex) == GpuResult::Ok, "pipeline source vertex shader may be destroyed after pipeline creation")) return false;
	if(!Check(device.DestroyShader(fragment) == GpuResult::Ok, "pipeline source fragment shader may be destroyed after pipeline creation")) return false;
	if(!Check(device.DestroyPipeline(pipeline) == GpuResult::Ok, "pipeline destroy should succeed")) return false;
	if(!Check(device.DestroyPipeline(pipeline) == GpuResult::InvalidHandle, "double pipeline destroy should fail")) return false;
	GpuPipelineId unknown_pipeline;
	unknown_pipeline.value = 44;
	if(!Check(device.DestroyPipeline(unknown_pipeline) == GpuResult::InvalidHandle, "unknown pipeline destroy should fail")) return false;
	return true;
}

static bool TestCommandStateValidation(NullGpuDevice& device)
{
	GpuTextureId texture;
	GpuPipelineId pipeline;
	GpuBufferId buffer;
	if(!Check(device.CreateTexture(MakeTextureDesc(Size(16, 16)), texture) == GpuResult::Ok, "setup texture should create")) return false;
	if(!Check(CreateBasicPipeline(device, pipeline), "setup pipeline should create")) return false;
	if(!Check(device.CreateBuffer(MakeVertexBufferDesc(64), buffer) == GpuResult::Ok, "setup buffer should create")) return false;

	GpuCommandListId list;
	if(!Check(device.BeginCommands(list) == GpuResult::Ok, "begin commands should succeed")) return false;
	if(!Check(device.BeginRenderPass(list, MakeRenderPassDesc(texture, GpuLoadOp::Clear, "pass")) == GpuResult::Ok, "begin render pass should succeed")) return false;
	if(!Check(device.SetPipeline(list, pipeline) == GpuResult::Ok, "set pipeline should succeed")) return false;
	if(!Check(device.SetVertexBuffer(list, buffer) == GpuResult::Ok, "set vertex buffer should succeed")) return false;
	if(!Check(device.Draw(list, 3, 0) == GpuResult::Ok, "draw should succeed")) return false;
	if(!Check(device.EndRenderPass(list) == GpuResult::Ok, "end render pass should succeed")) return false;
	if(!Check(device.EndCommands(list) == GpuResult::Ok, "end commands should succeed")) return false;

	if(!Check(device.BeginRenderPass(list, MakeRenderPassDesc(texture, GpuLoadOp::Load, "after")) == GpuResult::InvalidState, "begin render pass after end should fail")) return false;
	if(!Check(device.SetPipeline(list, pipeline) == GpuResult::InvalidState, "set pipeline after end should fail")) return false;
	if(!Check(device.SetVertexBuffer(list, buffer) == GpuResult::InvalidState, "set vertex buffer after end should fail")) return false;
	if(!Check(device.Draw(list, 3, 0) == GpuResult::InvalidState, "draw after end should fail")) return false;
	if(!Check(device.EndRenderPass(list) == GpuResult::InvalidState, "end render pass after end should fail")) return false;
	if(!Check(device.EndCommands(list) == GpuResult::InvalidState, "end commands twice should fail")) return false;
	if(!Check(device.Submit(list) == GpuResult::Ok, "submit should succeed")) return false;
	if(!Check(device.BeginRenderPass(list, MakeRenderPassDesc(texture, GpuLoadOp::Load, "after")) == GpuResult::InvalidState, "begin render pass after submit should fail")) return false;
	if(!Check(device.SetPipeline(list, pipeline) == GpuResult::InvalidState, "set pipeline after submit should fail")) return false;
	if(!Check(device.Draw(list, 3, 0) == GpuResult::InvalidState, "draw after submit should fail")) return false;
	if(!Check(device.Submit(list) == GpuResult::InvalidState, "submit twice should fail")) return false;
	return true;
}

static bool TestRecordingGuards(NullGpuDevice& device)
{
	GpuTextureId texture;
	GpuPipelineId pipeline;
	GpuBufferId vertex_buffer;
	if(!Check(device.CreateTexture(MakeTextureDesc(Size(8, 8)), texture) == GpuResult::Ok, "guard texture should create")) return false;
	if(!Check(CreateBasicPipeline(device, pipeline), "guard pipeline should create")) return false;
	if(!Check(device.CreateBuffer(MakeVertexBufferDesc(64), vertex_buffer) == GpuResult::Ok, "guard vertex buffer should create")) return false;

	GpuBufferDesc non_vertex_desc;
	non_vertex_desc.size = 64;
	non_vertex_desc.usage = GpuBufferUsage_Uniform;
	GpuBufferId non_vertex;
	if(!Check(device.CreateBuffer(non_vertex_desc, non_vertex) == GpuResult::Ok, "non-vertex buffer should create")) return false;

	GpuCommandListId list;
	if(!Check(device.BeginCommands(list) == GpuResult::Ok, "guard begin commands should succeed")) return false;
	if(!Check(device.SetPipeline(list, pipeline) == GpuResult::InvalidState, "set pipeline outside render pass fails")) return false;
	if(!Check(device.Draw(list, 3, 0) == GpuResult::InvalidState, "draw outside render pass fails")) return false;

	GpuRenderPassDesc bad_load = MakeRenderPassDesc(texture, GpuLoadOp::Load, "bad-load");
	bad_load.color_load = static_cast<GpuLoadOp>(99);
	if(!Check(device.BeginRenderPass(list, bad_load) == GpuResult::InvalidArgument, "invalid load op should fail")) return false;
	GpuRenderPassDesc bad_store = MakeRenderPassDesc(texture, GpuLoadOp::Load, "bad-store");
	bad_store.color_store = static_cast<GpuStoreOp>(99);
	if(!Check(device.BeginRenderPass(list, bad_store) == GpuResult::InvalidArgument, "invalid store op should fail")) return false;
	GpuRenderPassDesc bad_clear = MakeRenderPassDesc(texture, GpuLoadOp::Clear, "bad-clear");
	bad_clear.clear_color.red = std::numeric_limits<float>::quiet_NaN();
	if(!Check(device.BeginRenderPass(list, bad_clear) == GpuResult::InvalidArgument, "non-finite clear color should fail")) return false;

	if(!Check(device.BeginRenderPass(list, MakeRenderPassDesc(texture, GpuLoadOp::Load, "guard")) == GpuResult::Ok, "guard begin render pass should succeed")) return false;
	if(!Check(device.Draw(list, 3, 0) == GpuResult::InvalidState, "draw without pipeline fails")) return false;
	if(!Check(device.SetPipeline(list, pipeline) == GpuResult::Ok, "guard set pipeline should succeed")) return false;
	if(!Check(device.Draw(list, 3, 0) == GpuResult::InvalidState, "draw without vertex buffer fails")) return false;
	if(!Check(device.SetVertexBuffer(list, non_vertex) == GpuResult::InvalidArgument, "non-vertex usage buffer should fail binding")) return false;
	if(!Check(device.SetVertexBuffer(list, vertex_buffer) == GpuResult::Ok, "guard vertex buffer should bind")) return false;
	if(!Check(device.Draw(list, 0, 0) == GpuResult::InvalidArgument, "draw with zero vertices fails")) return false;
	if(!Check(device.Draw(list, 3, -1) == GpuResult::InvalidArgument, "draw with negative first vertex fails")) return false;
	if(!Check(device.Draw(list, 3, 0) == GpuResult::Ok, "guard draw should succeed")) return false;
	if(!Check(device.EndRenderPass(list) == GpuResult::Ok, "guard end render pass should succeed")) return false;
	if(!Check(device.EndCommands(list) == GpuResult::Ok, "guard end commands should succeed")) return false;
	if(!Check(device.Submit(list) == GpuResult::Ok, "guard submit should succeed")) return false;
	return true;
}

static String RunFullSequence()
{
	NullGpuDevice device;
	GpuSurfaceId surface;
	GpuSwapchainId swapchain;
	GpuFrameInfo frame;
	GpuPipelineId pipeline;
	GpuBufferId buffer;
	GpuCommandListId list;
	device.CreateSurface(MakeSurfaceDesc(Size(640, 480)), surface);
	device.CreateSwapchain(MakeSwapchainDesc(surface, Size(640, 480)), swapchain);
	device.BeginFrame(swapchain, frame);
	CreateBasicPipeline(device, pipeline);
	device.CreateBuffer(MakeVertexBufferDesc(128), buffer);
	device.BeginCommands(list);
	device.BeginRenderPass(list, MakeRenderPassDesc(frame.color_target, GpuLoadOp::Clear, "frame"));
	device.SetPipeline(list, pipeline);
	device.SetVertexBuffer(list, buffer);
	device.Draw(list, 3, 0);
	device.EndRenderPass(list);
	device.EndCommands(list);
	device.Submit(list);
	device.Present(frame.frame);
	device.DestroySwapchain(swapchain);
	device.DestroySurface(surface);
	return device.DumpLog();
}

static bool TestDeterministicLog()
{
	String a = RunFullSequence();
	String b = RunFullSequence();
	if(!Check(a == b, "identical sequences should match")) return false;
	if(!Check(a.Find("CreateSurface") >= 0, "log should contain surface creation")) return false;
	if(!Check(a.Find("CreateShader") >= 0, "log should contain shader creation")) return false;
	if(!Check(a.Find("BeginFrame") >= 0, "log should contain begin frame")) return false;
	if(!Check(a.Find("Present frame=") >= 0, "log should contain present")) return false;
	return true;
}

static bool TestStage1Independence()
{
	UiDisplayListBuilder builder;
	builder.Save();
	builder.FillRect(Rectf(0, 0, 4, 4), Rgba8(1, 2, 3, 4));
	builder.Restore();
	UiDisplayList list;
	if(!Check(builder.Finish(list), "stage1 list should still finish")) return false;
	if(!Check(list.IsValid(), "stage1 list should still be valid")) return false;
	return true;
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	NullGpuDevice device;
	ok &= TestHandles();
	ok &= Check(device.GetBackendKind() == GpuBackendKind::Null, "backend kind should be null");
	ok &= Check(device.GetDeviceId().IsValid(), "device id should be valid");
	ok &= Check(device.GetAdapterInfo().adapter_id.IsValid(), "adapter id should be valid");
	ok &= Check((device.GetAdapterInfo().capability_flags & GpuCapability_Shaders) != 0, "null backend should advertise shader capability");
	ok &= TestSurfaceLifecycle(device);
	ok &= TestSwapchainLifecycle(device);
	ok &= TestFrameLifecycle(device);
	ok &= TestResourceUploads(device);
	ok &= TestShaderLifecycle(device);
	ok &= TestPipelineLifecycle(device);
	ok &= TestCommandStateValidation(device);
	ok &= TestRecordingGuards(device);
	ok &= TestDeterministicLog();
	ok &= TestStage1Independence();
	if(!ok) {
		SetExitCode(1);
		return;
	}
	Cout() << "RenderRhiTest passed" << EOL;
}
