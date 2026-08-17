#include <RenderNull/RenderNull.h>

using namespace Upp;

static bool Check(bool condition, const char *message)
{
	if(!condition)
		Cout() << "FAIL: " << message << EOL;
	return condition;
}

static String MakeSpirVStub()
{
	static const byte data[] = {
		0x03, 0x02, 0x23, 0x07,
		0x00, 0x00, 0x01, 0x00,
	};
	return String(reinterpret_cast<const char *>(data), (int)sizeof(data));
}

static GpuShaderDesc MakeShader(GpuShaderStage stage)
{
	GpuShaderDesc desc;
	desc.stage = stage;
	desc.format = GpuShaderFormat::SpirV;
	desc.code = MakeSpirVStub();
	desc.entry_point = "main";
	return desc;
}

static GpuTextureDesc MakeTexture(Size size, GpuFormat format, int usage)
{
	GpuTextureDesc desc;
	desc.size = size;
	desc.format = format;
	desc.usage = usage;
	return desc;
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	NullGpuDevice device;

	ok &= Check(DumpGpuVertexLayout(GpuVertexLayout::Position2Uv2Color4F) == "Position2Uv2Color4F",
	            "UV vertex layout dump should be deterministic");
	ok &= Check(DumpGpuSamplerFilter(GpuSamplerFilter::Linear) == "Linear",
	            "sampler filter dump should be deterministic");
	ok &= Check(DumpGpuSamplerAddressMode(GpuSamplerAddressMode::ClampToEdge) == "ClampToEdge",
	            "sampler address dump should be deterministic");

	GpuTextureId sampled;
	ok &= Check(device.CreateTexture(MakeTexture(Size(2, 2), GpuFormat::RGBA8Srgb,
	                                                GpuTextureUsage_Sampled | GpuTextureUsage_TransferDst), sampled) == GpuResult::Ok,
	            "sRGB sampled texture should create");
	const byte pixels[16] = {
		255, 0, 0, 255, 0, 255, 0, 255,
		0, 0, 255, 255, 255, 255, 255, 128,
	};
	GpuTextureWriteDesc upload;
	upload.size = Size(2, 2);
	upload.row_pitch = 8;
	ok &= Check(device.WriteTexture(sampled, upload, pixels, sizeof(pixels)) == GpuResult::Ok,
	            "sRGB sampled texture upload should use four bytes per pixel");

	GpuTextureId target;
	ok &= Check(device.CreateTexture(MakeTexture(Size(32, 24), GpuFormat::RGBA8,
	                                                GpuTextureUsage_ColorAttachment), target) == GpuResult::Ok,
	            "color target should create");

	GpuShaderId vertex_shader, fragment_shader;
	ok &= Check(device.CreateShader(MakeShader(GpuShaderStage::Vertex), vertex_shader) == GpuResult::Ok,
	            "sampled vertex shader should create");
	ok &= Check(device.CreateShader(MakeShader(GpuShaderStage::Fragment), fragment_shader) == GpuResult::Ok,
	            "sampled fragment shader should create");

	GpuPipelineDesc desc;
	desc.color_format = GpuFormat::RGBA8;
	desc.vertex_shader = vertex_shader;
	desc.fragment_shader = fragment_shader;
	desc.vertex_layout = GpuVertexLayout::Position2Uv2Color4F;
	desc.blend_mode = GpuBlendMode::SourceOver;
	desc.sampled_texture_count = 1;
	desc.sampler_filter = GpuSamplerFilter::Linear;
	desc.sampler_address = GpuSamplerAddressMode::ClampToEdge;
	GpuPipelineId pipeline;
	ok &= Check(device.CreatePipeline(desc, pipeline) == GpuResult::Ok,
	            "one-slot sampled pipeline should create");

	GpuPipelineDesc too_many = desc;
	too_many.sampled_texture_count = 2;
	GpuPipelineId invalid_pipeline;
	ok &= Check(device.CreatePipeline(too_many, invalid_pipeline) == GpuResult::InvalidArgument,
	            "more than one sampled slot should be rejected by the bounded Stage-5 contract");
	GpuPipelineDesc wrong_layout = desc;
	wrong_layout.vertex_layout = GpuVertexLayout::Position2Color4F;
	ok &= Check(device.CreatePipeline(wrong_layout, invalid_pipeline) == GpuResult::InvalidArgument,
	            "sampled pipeline should require UV vertex layout");

	GpuBufferDesc buffer_desc;
	buffer_desc.size = 3 * 8 * (int)sizeof(float);
	buffer_desc.usage = GpuBufferUsage_Vertex;
	GpuBufferId vertices;
	ok &= Check(device.CreateBuffer(buffer_desc, vertices) == GpuResult::Ok,
	            "sampled vertex buffer should create");

	GpuCommandListId commands;
	ok &= Check(device.BeginCommands(commands) == GpuResult::Ok, "sampled commands should begin");
	GpuRenderPassDesc pass;
	pass.color_target = target;
	pass.color_format = GpuFormat::RGBA8;
	pass.color_load = GpuLoadOp::Clear;
	pass.color_store = GpuStoreOp::Store;
	ok &= Check(device.BeginRenderPass(commands, pass) == GpuResult::Ok, "sampled render pass should begin");
	ok &= Check(device.SetPipeline(commands, pipeline) == GpuResult::Ok, "sampled pipeline should bind");
	ok &= Check(device.SetVertexBuffer(commands, vertices) == GpuResult::Ok, "sampled vertex buffer should bind");
	ok &= Check(device.Draw(commands, 3) == GpuResult::InvalidState,
	            "draw should fail until the declared sampled slot is bound");
	ok &= Check(device.SetSampledTexture(commands, 1, sampled) == GpuResult::InvalidArgument,
	            "sampled slot outside the pipeline range should fail");
	ok &= Check(device.SetSampledTexture(commands, 0, target) == GpuResult::InvalidArgument,
	            "non-sampled color target should not bind as sampled input");
	ok &= Check(device.SetSampledTexture(commands, 0, sampled) == GpuResult::Ok,
	            "sampled texture should bind to slot zero");
	ok &= Check(device.DestroyTexture(sampled) == GpuResult::InvalidState,
	            "bound sampled texture must remain alive while command work is live");
	ok &= Check(device.Draw(commands, 3) == GpuResult::Ok,
	            "draw should succeed after sampled texture binding");
	ok &= Check(device.EndRenderPass(commands) == GpuResult::Ok, "sampled render pass should end");
	ok &= Check(device.DestroyTexture(sampled) == GpuResult::InvalidState,
	            "recorded sampled texture must remain alive until submission completes");
	ok &= Check(device.EndCommands(commands) == GpuResult::Ok, "sampled commands should end");
	ok &= Check(device.Submit(commands) == GpuResult::Ok, "sampled commands should submit");
	ok &= Check(device.DestroyTexture(sampled) == GpuResult::Ok,
	            "sampled texture should become destroyable after synchronous submission");

	ok &= Check(device.DestroyBuffer(vertices) == GpuResult::Ok, "sampled vertex buffer should destroy");
	ok &= Check(device.DestroyPipeline(pipeline) == GpuResult::Ok, "sampled pipeline should destroy");
	ok &= Check(device.DestroyShader(vertex_shader) == GpuResult::Ok, "sampled vertex shader should destroy");
	ok &= Check(device.DestroyShader(fragment_shader) == GpuResult::Ok, "sampled fragment shader should destroy");
	ok &= Check(device.DestroyTexture(target) == GpuResult::Ok, "sampled color target should destroy");

	if(ok) {
		Cout() << "RenderSampledRhiTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
