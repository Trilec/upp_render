#include <RenderCanvas/RenderCanvas.h>
#include <RenderNull/RenderNull.h>

using namespace Upp;

static bool Check(bool cond, const char *msg)
{
	if(!cond)
		Cout() << "FAIL: " << msg << EOL;
	return cond;
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
	GpuBufferDesc buffer_desc;
	buffer_desc.size = 256;
	buffer_desc.usage = GpuBufferUsage_Vertex;
	GpuBufferId buffer;
	if(!Check(device.CreateBuffer(buffer_desc, buffer) == GpuResult::Ok, "buffer create should succeed")) return false;
	if(!Check(buffer.IsValid(), "created buffer valid")) return false;

	GpuBufferDesc zero_buffer_desc;
	zero_buffer_desc.size = 0;
	zero_buffer_desc.usage = GpuBufferUsage_Vertex;
	GpuBufferId zero_buffer;
	if(!Check(device.CreateBuffer(zero_buffer_desc, zero_buffer) == GpuResult::InvalidArgument, "zero buffer should fail")) return false;
	if(!Check(!zero_buffer.IsValid(), "zero buffer handle invalid")) return false;

	if(!Check(device.DestroyBuffer(buffer) == GpuResult::Ok, "buffer destroy should succeed")) return false;
	if(!Check(device.DestroyBuffer(buffer) == GpuResult::InvalidHandle, "double buffer destroy should fail")) return false;
	GpuBufferId unknown_buffer;
	unknown_buffer.value = 99;
	if(!Check(device.DestroyBuffer(unknown_buffer) == GpuResult::InvalidHandle, "unknown buffer destroy should fail")) return false;

	GpuTextureDesc texture_desc;
	texture_desc.size = Size(64, 32);
	texture_desc.format = GpuFormat::RGBA8;
	texture_desc.usage = GpuTextureUsage_ColorAttachment | GpuTextureUsage_Sampled;
	GpuTextureId texture;
	if(!Check(device.CreateTexture(texture_desc, texture) == GpuResult::Ok, "texture create should succeed")) return false;
	if(!Check(texture.IsValid(), "created texture valid")) return false;

	GpuTextureDesc zero_width = texture_desc;
	zero_width.size = Size(0, 32);
	GpuTextureId tmp_texture;
	if(!Check(device.CreateTexture(zero_width, tmp_texture) == GpuResult::InvalidArgument, "zero width texture should fail")) return false;

	GpuTextureDesc zero_height = texture_desc;
	zero_height.size = Size(64, 0);
	if(!Check(device.CreateTexture(zero_height, tmp_texture) == GpuResult::InvalidArgument, "zero height texture should fail")) return false;

	GpuTextureDesc bad_format = texture_desc;
	bad_format.format = GpuFormat::Unknown;
	if(!Check(device.CreateTexture(bad_format, tmp_texture) == GpuResult::InvalidArgument, "unknown format texture should fail")) return false;

	if(!Check(device.DestroyTexture(texture) == GpuResult::Ok, "texture destroy should succeed")) return false;
	if(!Check(device.DestroyTexture(texture) == GpuResult::InvalidHandle, "double texture destroy should fail")) return false;
	GpuTextureId unknown_texture;
	unknown_texture.value = 77;
	if(!Check(device.DestroyTexture(unknown_texture) == GpuResult::InvalidHandle, "unknown texture destroy should fail")) return false;
	return true;
}

static bool TestCommandLifecycle(NullGpuDevice& device)
{
	GpuTextureDesc texture_desc;
	texture_desc.size = Size(16, 16);
	texture_desc.format = GpuFormat::RGBA8;
	texture_desc.usage = GpuTextureUsage_ColorAttachment;
	GpuTextureId color_target;
	if(!Check(device.CreateTexture(texture_desc, color_target) == GpuResult::Ok, "color target should create")) return false;

	GpuCommandListId list;
	if(!Check(device.BeginCommands(list) == GpuResult::Ok, "begin commands should succeed")) return false;
	if(!Check(list.IsValid(), "command list handle valid")) return false;
	if(!Check(device.BeginRenderPass(GpuCommandListId(), GpuRenderPassDesc()) == GpuResult::InvalidState, "begin render pass without list should fail")) return false;

	GpuRenderPassDesc pass_desc;
	pass_desc.color_target = color_target;
	pass_desc.color_format = GpuFormat::RGBA8;
	pass_desc.color_load = GpuLoadOp::Clear;
	pass_desc.color_store = GpuStoreOp::Store;
	if(!Check(device.BeginRenderPass(list, pass_desc) == GpuResult::Ok, "begin render pass should succeed")) return false;
	if(!Check(device.EndCommands(list) == GpuResult::InvalidState, "end commands during active pass should fail")) return false;
	if(!Check(device.EndRenderPass(list) == GpuResult::Ok, "end render pass should succeed")) return false;
	if(!Check(device.EndCommands(list) == GpuResult::Ok, "end commands should succeed")) return false;
	if(!Check(device.Submit(list) == GpuResult::Ok, "submit should succeed")) return false;
	if(!Check(device.Submit(list) == GpuResult::InvalidState, "resubmit should fail")) return false;
	if(!Check(device.BeginRenderPass(list, pass_desc) == GpuResult::InvalidState, "submitted list should not reopen")) return false;
	if(!Check(device.EndCommands(list) == GpuResult::InvalidState, "submitted list should not end again")) return false;
	if(!Check(device.EndRenderPass(list) == GpuResult::InvalidState, "submitted list should not end pass again")) return false;
	if(!Check(device.DestroyTexture(color_target) == GpuResult::Ok, "cleanup texture should succeed")) return false;
	return true;
}

static bool TestUnfinishedSubmit(NullGpuDevice& device)
{
	GpuCommandListId list;
	if(!Check(device.BeginCommands(list) == GpuResult::Ok, "begin commands should succeed")) return false;
	if(!Check(device.Submit(list) == GpuResult::InvalidState, "unfinished submit should fail")) return false;
	if(!Check(device.EndCommands(list) == GpuResult::Ok, "ending commands should succeed")) return false;
	if(!Check(device.Submit(list) == GpuResult::Ok, "finished submit should succeed")) return false;
	return true;
}

static bool TestDeterministicLog()
{
	GpuTextureDesc texture_desc;
	texture_desc.size = Size(8, 8);
	texture_desc.format = GpuFormat::RGBA8;
	texture_desc.usage = GpuTextureUsage_ColorAttachment;

	auto RunSequence = [&]() -> String {
		NullGpuDevice device;
		GpuTextureId texture;
		GpuCommandListId list;
		device.CreateTexture(texture_desc, texture);
		device.BeginCommands(list);
		GpuRenderPassDesc pass;
		pass.color_target = texture;
		pass.color_format = GpuFormat::RGBA8;
		device.BeginRenderPass(list, pass);
		device.EndRenderPass(list);
		device.EndCommands(list);
		device.Submit(list);
		return device.DumpLog();
	};

	String a = RunSequence();
	String b = RunSequence();
	if(!Check(a == b, "identical sequences should match")) return false;
	if(!Check(a.Find("0x") < 0, "log should not contain addresses")) return false;
	if(!Check(a.Find("CreateTexture") >= 0, "log should contain texture creation")) return false;
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
	ok &= TestCommandLifecycle(device);
	ok &= TestUnfinishedSubmit(device);
	ok &= TestDeterministicLog();
	ok &= TestStage1Independence();
	if(!ok) {
		SetExitCode(1);
		return;
	}
	Cout() << "RenderRhiTest passed" << EOL;
}
