#include <RenderCanvas/RenderCanvas.h>
#include <RenderNull/RenderNull.h>

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
	desc.label = label;
	return desc;
}

static GpuPipelineDesc MakePipelineDesc()
{
	GpuPipelineDesc desc;
	desc.topology = GpuPrimitiveTopology::TriangleList;
	desc.color_format = GpuFormat::RGBA8;
	desc.label = "basic";
	return desc;
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
	if(!Check(device.CreatePipeline(MakePipelineDesc(), pipeline) == GpuResult::Ok, "frame pipeline should create")) return false;
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

static bool TestPipelineLifecycle(NullGpuDevice& device)
{
	GpuPipelineId pipeline;
	if(!Check(device.CreatePipeline(MakePipelineDesc(), pipeline) == GpuResult::Ok, "pipeline create should succeed")) return false;
	if(!Check(pipeline.IsValid(), "created pipeline valid")) return false;

	GpuPipelineDesc bad_format = MakePipelineDesc();
	bad_format.color_format = GpuFormat::Unknown;
	GpuPipelineId tmp;
	if(!Check(device.CreatePipeline(bad_format, tmp) == GpuResult::InvalidArgument, "unknown format pipeline should fail")) return false;

	GpuPipelineDesc bad_topology = MakePipelineDesc();
	bad_topology.topology = static_cast<GpuPrimitiveTopology>(99);
	if(!Check(device.CreatePipeline(bad_topology, tmp) == GpuResult::InvalidArgument, "invalid topology pipeline should fail")) return false;

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
	if(!Check(device.CreatePipeline(MakePipelineDesc(), pipeline) == GpuResult::Ok, "setup pipeline should create")) return false;
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
	if(!Check(device.CreateTexture(MakeTextureDesc(Size(8, 8)), texture) == GpuResult::Ok, "guard texture should create")) return false;
	if(!Check(device.CreatePipeline(MakePipelineDesc(), pipeline) == GpuResult::Ok, "guard pipeline should create")) return false;

	GpuCommandListId list;
	if(!Check(device.BeginCommands(list) == GpuResult::Ok, "guard begin commands should succeed")) return false;
	if(!Check(device.SetPipeline(list, pipeline) == GpuResult::InvalidState, "set pipeline outside render pass fails")) return false;
	if(!Check(device.Draw(list, 3, 0) == GpuResult::InvalidState, "draw outside render pass fails")) return false;

	if(!Check(device.BeginRenderPass(list, MakeRenderPassDesc(texture, GpuLoadOp::Load, "guard")) == GpuResult::Ok, "guard begin render pass should succeed")) return false;
	if(!Check(device.Draw(list, 0, 0) == GpuResult::InvalidState, "draw without pipeline fails")) return false;
	if(!Check(device.SetPipeline(list, pipeline) == GpuResult::Ok, "guard set pipeline should succeed")) return false;
	if(!Check(device.Draw(list, 0, 0) == GpuResult::InvalidArgument, "draw with zero vertices fails")) return false;
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
	device.CreatePipeline(MakePipelineDesc(), pipeline);
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
	ok &= TestSurfaceLifecycle(device);
	ok &= TestSwapchainLifecycle(device);
	ok &= TestFrameLifecycle(device);
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
