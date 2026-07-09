#include <RenderCanvas/RenderCanvas.h>
#include <RenderNull/RenderNull.h>

using namespace Upp;

static bool Check(bool cond, const char *msg)
{
	if(!cond)
		Cout() << "FAIL: " << msg << EOL;
	return cond;
}

static GpuBufferDesc MakeVertexBufferDesc(int64 size)
{
	GpuBufferDesc desc;
	desc.size = size;
	desc.usage = GpuBufferUsage_Vertex;
	return desc;
}

static GpuTextureDesc MakeColorTextureDesc(Size size)
{
	GpuTextureDesc desc;
	desc.size = size;
	desc.format = GpuFormat::RGBA8;
	desc.usage = GpuTextureUsage_ColorAttachment;
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
	return Check(!buffer.IsValid(), "default buffer invalid") &&
	       Check(!texture.IsValid(), "default texture invalid") &&
	       Check(!shader.IsValid(), "default shader invalid") &&
	       Check(!pipeline.IsValid(), "default pipeline invalid") &&
	       Check(!commands.IsValid(), "default command list invalid") &&
	       Check(!adapter.IsValid(), "default adapter invalid") &&
	       Check(!device.IsValid(), "default device invalid") &&
	       Check(buffer.Dump() == "Buffer#0", "buffer dump deterministic");
}

static bool TestResourceLifetime(NullGpuDevice& device)
{
	GpuBufferId buffer;
	if(!Check(device.CreateBuffer(MakeVertexBufferDesc(256), buffer) == GpuResult::Ok, "buffer create should succeed")) return false;
	if(!Check(buffer.IsValid(), "created buffer valid")) return false;
	GpuBufferId zero_buffer;
	if(!Check(device.CreateBuffer(MakeVertexBufferDesc(0), zero_buffer) == GpuResult::InvalidArgument, "zero buffer should fail")) return false;

	if(!Check(device.DestroyBuffer(buffer) == GpuResult::Ok, "buffer destroy should succeed")) return false;
	if(!Check(device.DestroyBuffer(buffer) == GpuResult::InvalidHandle, "double buffer destroy should fail")) return false;
	GpuBufferId unknown_buffer;
	unknown_buffer.value = 99;
	if(!Check(device.DestroyBuffer(unknown_buffer) == GpuResult::InvalidHandle, "unknown buffer destroy should fail")) return false;

	GpuTextureId texture;
	if(!Check(device.CreateTexture(MakeColorTextureDesc(Size(64, 32)), texture) == GpuResult::Ok, "texture create should succeed")) return false;
	if(!Check(texture.IsValid(), "created texture valid")) return false;

	GpuTextureDesc zero_width = MakeColorTextureDesc(Size(0, 32));
	GpuTextureId zero_texture;
	if(!Check(device.CreateTexture(zero_width, zero_texture) == GpuResult::InvalidArgument, "zero width texture should fail")) return false;

	GpuTextureDesc zero_height = MakeColorTextureDesc(Size(64, 0));
	if(!Check(device.CreateTexture(zero_height, zero_texture) == GpuResult::InvalidArgument, "zero height texture should fail")) return false;

	GpuTextureDesc bad_format = MakeColorTextureDesc(Size(64, 32));
	bad_format.format = GpuFormat::Unknown;
	if(!Check(device.CreateTexture(bad_format, zero_texture) == GpuResult::InvalidArgument, "unknown format texture should fail")) return false;

	if(!Check(device.DestroyTexture(texture) == GpuResult::Ok, "texture destroy should succeed")) return false;
	if(!Check(device.DestroyTexture(texture) == GpuResult::InvalidHandle, "double texture destroy should fail")) return false;
	GpuTextureId unknown_texture;
	unknown_texture.value = 77;
	if(!Check(device.DestroyTexture(unknown_texture) == GpuResult::InvalidHandle, "unknown texture destroy should fail")) return false;
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
	if(!Check(device.CreateTexture(MakeColorTextureDesc(Size(16, 16)), texture) == GpuResult::Ok, "setup texture should create")) return false;
	if(!Check(device.CreatePipeline(MakePipelineDesc(), pipeline) == GpuResult::Ok, "setup pipeline should create")) return false;
	if(!Check(device.CreateBuffer(MakeVertexBufferDesc(64), buffer) == GpuResult::Ok, "setup buffer should create")) return false;

	GpuCommandListId list;
	if(!Check(device.BeginCommands(list) == GpuResult::Ok, "begin commands should succeed")) return false;
	if(!Check(list.IsValid(), "command list handle valid")) return false;

	GpuRenderPassDesc pass_desc;
	pass_desc.color_target = texture;
	pass_desc.color_format = GpuFormat::RGBA8;
	pass_desc.color_load = GpuLoadOp::Clear;
	pass_desc.color_store = GpuStoreOp::Store;

	if(!Check(device.BeginRenderPass(list, pass_desc) == GpuResult::Ok, "begin render pass should succeed")) return false;
	if(!Check(device.SetPipeline(list, pipeline) == GpuResult::Ok, "set pipeline should succeed")) return false;
	if(!Check(device.SetVertexBuffer(list, buffer) == GpuResult::Ok, "set vertex buffer should succeed")) return false;
	if(!Check(device.Draw(list, 3, 0) == GpuResult::Ok, "draw should succeed")) return false;
	if(!Check(device.EndRenderPass(list) == GpuResult::Ok, "end render pass should succeed")) return false;
	if(!Check(device.EndCommands(list) == GpuResult::Ok, "end commands should succeed")) return false;

	if(!Check(device.BeginRenderPass(list, pass_desc) == GpuResult::InvalidState, "begin render pass after end should fail")) return false;
	if(!Check(device.SetPipeline(list, pipeline) == GpuResult::InvalidState, "set pipeline after end should fail")) return false;
	if(!Check(device.SetVertexBuffer(list, buffer) == GpuResult::InvalidState, "set vertex buffer after end should fail")) return false;
	if(!Check(device.Draw(list, 3, 0) == GpuResult::InvalidState, "draw after end should fail")) return false;
	if(!Check(device.EndRenderPass(list) == GpuResult::InvalidState, "end render pass after end should fail")) return false;
	if(!Check(device.EndCommands(list) == GpuResult::InvalidState, "end commands twice should fail")) return false;
	if(!Check(device.Submit(list) == GpuResult::Ok, "submit should succeed")) return false;
	if(!Check(device.BeginRenderPass(list, pass_desc) == GpuResult::InvalidState, "begin render pass after submit should fail")) return false;
	if(!Check(device.SetPipeline(list, pipeline) == GpuResult::InvalidState, "set pipeline after submit should fail")) return false;
	if(!Check(device.Draw(list, 3, 0) == GpuResult::InvalidState, "draw after submit should fail")) return false;
	if(!Check(device.Submit(list) == GpuResult::InvalidState, "submit twice should fail")) return false;
	return true;
}

static bool TestRecordingGuards(NullGpuDevice& device)
{
	GpuTextureId texture;
	GpuPipelineId pipeline;
	if(!Check(device.CreateTexture(MakeColorTextureDesc(Size(8, 8)), texture) == GpuResult::Ok, "guard texture should create")) return false;
	if(!Check(device.CreatePipeline(MakePipelineDesc(), pipeline) == GpuResult::Ok, "guard pipeline should create")) return false;

	GpuCommandListId list;
	if(!Check(device.BeginCommands(list) == GpuResult::Ok, "guard begin commands should succeed")) return false;
	if(!Check(device.SetPipeline(list, pipeline) == GpuResult::InvalidState, "set pipeline outside render pass fails")) return false;
	if(!Check(device.Draw(list, 3, 0) == GpuResult::InvalidState, "draw outside render pass fails")) return false;

	GpuRenderPassDesc pass_desc;
	pass_desc.color_target = texture;
	pass_desc.color_format = GpuFormat::RGBA8;
	if(!Check(device.BeginRenderPass(list, pass_desc) == GpuResult::Ok, "guard begin render pass should succeed")) return false;
	if(!Check(device.Draw(list, 0, 0) == GpuResult::InvalidState, "draw without pipeline fails")) return false;
	if(!Check(device.SetPipeline(list, pipeline) == GpuResult::Ok, "guard set pipeline should succeed")) return false;
	if(!Check(device.Draw(list, 0, 0) == GpuResult::InvalidArgument, "draw with zero vertices fails")) return false;
	if(!Check(device.Draw(list, 3, 0) == GpuResult::Ok, "guard draw should succeed")) return false;
	if(!Check(device.EndRenderPass(list) == GpuResult::Ok, "guard end render pass should succeed")) return false;
	if(!Check(device.EndCommands(list) == GpuResult::Ok, "guard end commands should succeed")) return false;
	if(!Check(device.Submit(list) == GpuResult::Ok, "guard submit should succeed")) return false;
	return true;
}

static String RunValidSequence()
{
	NullGpuDevice device;
	GpuTextureId texture;
	GpuPipelineId pipeline;
	GpuBufferId buffer;
	GpuCommandListId list;
	device.CreateTexture(MakeColorTextureDesc(Size(16, 16)), texture);
	device.CreatePipeline(MakePipelineDesc(), pipeline);
	device.CreateBuffer(MakeVertexBufferDesc(64), buffer);
	device.BeginCommands(list);
	GpuRenderPassDesc pass_desc;
	pass_desc.color_target = texture;
	pass_desc.color_format = GpuFormat::RGBA8;
	pass_desc.color_load = GpuLoadOp::Clear;
	pass_desc.color_store = GpuStoreOp::Store;
	device.BeginRenderPass(list, pass_desc);
	device.SetPipeline(list, pipeline);
	device.SetVertexBuffer(list, buffer);
	device.Draw(list, 3, 0);
	device.EndRenderPass(list);
	device.EndCommands(list);
	device.Submit(list);
	return device.DumpLog();
}

static bool TestDeterministicLog()
{
	String a = RunValidSequence();
	String b = RunValidSequence();
	if(!Check(a == b, "identical sequences should match")) return false;
	if(!Check(a.Find("0x") < 0, "log should not contain addresses")) return false;
	if(!Check(a.Find("SetPipeline") >= 0, "log should contain pipeline commands")) return false;
	if(!Check(a.Find("Draw list=") >= 0, "log should contain draw command")) return false;
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
	ok &= TestResourceLifetime(device);
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
