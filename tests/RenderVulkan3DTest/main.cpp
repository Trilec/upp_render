#include <RenderVulkan/RenderVulkanRhi.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include "../../shaders/Ui3DMeshVertSpirv.h"
#include "../../shaders/Ui3DMeshFragSpirv.h"
#include "../../shaders/Ui3DEdgeFragSpirv.h"
#include <cmath>
#include <cstring>
#include <limits>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Upp;

namespace {
int checks = 0, failures = 0, oracle_pixels = 0;
bool Check(bool value, const char *message)
{
	++checks;
	if(!value) { ++failures; Cout() << "FAIL: " << message << EOL; }
	return value;
}
FARPROC WINAPI Resolver(HMODULE, LPCSTR) { return reinterpret_cast<FARPROC>(1); }

struct Vertex { float x,y,z,nx,ny,nz,r,g,b,a; };
struct Parameters { float mvp[16], normal[12], tint[4]; };
static_assert(sizeof(Vertex) == 40 && sizeof(Parameters) == 128, "mesh shader contract");

Parameters Identity(bool flip_y = true)
{
	Parameters p {};
	p.mvp[0] = p.mvp[10] = p.mvp[15] = 1; p.mvp[5] = flip_y ? -1.f : 1.f;
	p.normal[0] = p.normal[5] = p.normal[10] = 1;
	for(float& x : p.tint) x = 1;
	return p;
}

// Both triangles cover the same XY region, but their depth planes intersect.
// This makes the correct front-most color vary across one rendered primitive.
const Vertex kVertices[] = {
	{-.8f,-.7f,.2f,0,0,1,1,0,0,1},
	{ .8f,-.7f,.8f,0,0,1,1,0,0,1},
	{  0, .85f,.5f,0,0,1,1,0,0,1},
	{-.8f,-.7f,.8f,0,0,1,0,1,0,1},
	{ .8f,-.7f,.2f,0,0,1,0,1,0,1},
	{  0, .85f,.5f,0,0,1,0,1,0,1}
};
const uint32 kIndices[] = {0,1,2,3,4,5};

class Fixture {
public:
	VulkanGpuDevice& device;
	Vector<GpuBufferId> buffers;
	Vector<GpuTextureId> textures;
	Vector<GpuPipelineId> pipelines;
	Vector<GpuShaderId> shaders;
	GpuShaderId vs, fs;
	GpuBufferId vertices, indices, short_indices;
	GpuTextureId color, depth;
	GpuPipelineDesc pipeline_desc;
	GpuPipelineId pipeline;
	explicit Fixture(VulkanGpuDevice& d) : device(d) {}
	~Fixture()
	{
		for(auto p : pipelines) device.DestroyPipeline(p);
		for(auto s : shaders) device.DestroyShader(s);
		for(auto t : textures) device.DestroyTexture(t);
		for(auto b : buffers) device.DestroyBuffer(b);
	}
	bool Result(GpuResult result, const char *message)
	{
		if(!Check(result == GpuResult::Ok, message)) {
			Cout() << "  " << DumpGpuResult(result) << ": " << device.GetLastError() << EOL;
			return false;
		}
		return true;
	}
	GpuBufferId Buffer(int usage, const void *data, int bytes)
	{
		GpuBufferDesc desc; desc.size = bytes; desc.usage = usage | GpuBufferUsage_TransferDst;
		GpuBufferId id;
		if(Result(device.CreateBuffer(desc, id), "buffer create")) {
			buffers.Add(id);
			if(data) Result(device.WriteBuffer(id, 0, data, bytes), "buffer upload");
		}
		return id;
	}
	GpuTextureId Texture(GpuFormat format, int usage, Size size = Size(64,64))
	{
		GpuTextureDesc desc; desc.size = size; desc.format = format; desc.usage = usage;
		GpuTextureId id;
		if(Result(device.CreateTexture(desc, id), "texture create")) textures.Add(id);
		return id;
	}
	GpuPipelineId Pipeline(const GpuPipelineDesc& desc)
	{
		GpuPipelineId id;
		if(Result(device.CreatePipeline(desc, id), "pipeline create")) pipelines.Add(id);
		return id;
	}
	GpuRenderPassDesc Pass() const
	{
		GpuRenderPassDesc pass;
		pass.color_target = color; pass.color_format = GpuFormat::RGBA8;
		pass.color_load = GpuLoadOp::Clear; pass.color_store = GpuStoreOp::Store;
		pass.depth_target = depth; pass.depth_format = GpuFormat::D32F;
		pass.depth_load = GpuLoadOp::Clear; pass.depth_store = GpuStoreOp::Store;
		pass.flip_viewport_y = false;
		return pass;
	}
	bool Init()
	{
		GpuShaderDesc shader; shader.format = GpuShaderFormat::SpirV;
		shader.stage = GpuShaderStage::Vertex;
		shader.code = String(reinterpret_cast<const char *>(kUi3DMeshVertex), sizeof(kUi3DMeshVertex));
		if(!Result(device.CreateShader(shader, vs), "mesh vertex shader create")) return false;
		shaders.Add(vs);
		shader.stage = GpuShaderStage::Fragment;
		shader.code = String(reinterpret_cast<const char *>(kUi3DMeshFragment), sizeof(kUi3DMeshFragment));
		if(!Result(device.CreateShader(shader, fs), "mesh fragment shader create")) return false;
		shaders.Add(fs);
		vertices = Buffer(GpuBufferUsage_Vertex, kVertices, sizeof(kVertices));
		indices = Buffer(GpuBufferUsage_Index, kIndices, sizeof(kIndices));
		const uint16 small[] = {0,1,2,3,4,5};
		short_indices = Buffer(GpuBufferUsage_Index, small, sizeof(small));
		color = Texture(GpuFormat::RGBA8, GpuTextureUsage_ColorAttachment | GpuTextureUsage_TransferSrc);
		depth = Texture(GpuFormat::D32F, GpuTextureUsage_DepthAttachment);
		pipeline_desc.color_format = GpuFormat::RGBA8; pipeline_desc.depth_format = GpuFormat::D32F;
		pipeline_desc.vertex_shader = vs; pipeline_desc.fragment_shader = fs;
		pipeline_desc.vertex_layout = GpuVertexLayout::Position3Normal3Color4F;
		pipeline_desc.parameter_bytes = sizeof(Parameters);
		pipeline_desc.depth_test = pipeline_desc.depth_write = true;
		pipeline_desc.depth_compare = GpuCompareOp::Less;
		pipeline = Pipeline(pipeline_desc);
		return vertices.IsValid() && indices.IsValid() && short_indices.IsValid() &&
		       color.IsValid() && depth.IsValid() && pipeline.IsValid();
	}
	bool Render(Vector<byte>& pixels, bool reverse = false, bool shorts = false,
	            GpuPipelineId override_pipeline = GpuPipelineId(),
	            const Parameters *override_parameters = nullptr, bool viewport_flip = false,
	            bool single = false)
	{
		GpuCommandListId cmd;
		if(!Result(device.BeginCommands(cmd), "begin finite frame")) return false;
		auto abort = [&] { device.DiscardCommands(cmd); return false; };
		auto pass = Pass(); pass.flip_viewport_y = viewport_flip;
		Parameters parameters = override_parameters ? *override_parameters : Identity();
		if(!Result(device.BeginRenderPass(cmd, pass), "begin depth pass") ||
		   !Result(device.SetPipeline(cmd, override_pipeline.IsValid() ? override_pipeline : pipeline), "bind depth pipeline") ||
		   !Result(device.SetVertexBuffer(cmd, vertices), "bind vertices") ||
		   !Result(device.SetIndexBuffer(cmd, shorts ? short_indices : indices,
		                                shorts ? GpuIndexType::UInt16 : GpuIndexType::UInt32), "bind indices") ||
		   !Result(device.SetGraphicsParameters(cmd, &parameters, sizeof(parameters)), "push matrix and material") ||
		   !Result(device.DrawIndexed(cmd, 3, reverse ? 3 : 0), "first indexed triangle")) return abort();
		if(!single && !Result(device.DrawIndexed(cmd, 3, reverse ? 0 : 3), "second indexed triangle")) return abort();
		if(!Result(device.EndRenderPass(cmd), "end depth pass") ||
		   !Result(device.EndCommands(cmd), "end finite frame") ||
		   !Result(device.Submit(cmd), "submit finite frame")) return abort();
		return Check(device.ReadTexturePixels(color, pixels), "read bounded color oracle");
	}
};

void TestRejectionAndDiscard(Fixture& f)
{
	auto& d = f.device;
	GpuTextureCapabilities caps;
	f.Result(d.GetTextureCapabilities(GpuFormat::D32F, GpuTextureUsage_DepthAttachment, caps), "query real D32F support");
	Check(caps.depth_attachment && caps.bytes_per_pixel == 4, "D32F must advertise actual depth support");
	Check(!caps.sampled && !caps.linear_filter && !caps.transfer_src && !caps.transfer_dst &&
	      !caps.color_attachment && !caps.color_blend, "D32F reports only implemented backend operations");
	Check(d.GetTextureCapabilities(GpuFormat::D32F, GpuTextureUsage_Sampled, caps) == GpuResult::Unsupported,
	      "unimplemented depth sampling must not be advertised");
	Check(d.GetTextureCapabilities(GpuFormat::RGBA8, GpuTextureUsage_DepthAttachment, caps) == GpuResult::InvalidArgument,
	      "color format cannot be a depth attachment");
	const int pipeline_count = d.GetLivePipelineCount();
	GpuPipelineId rejected;
	auto bad_pipeline = f.pipeline_desc; bad_pipeline.parameter_bytes = 132;
	Check(d.CreatePipeline(bad_pipeline, rejected) == GpuResult::InvalidArgument && !rejected.IsValid(), "oversized push payload rejected");
	bad_pipeline = f.pipeline_desc; bad_pipeline.parameter_bytes = 127;
	Check(d.CreatePipeline(bad_pipeline, rejected) == GpuResult::InvalidArgument, "unaligned push payload rejected");
	bad_pipeline = f.pipeline_desc; bad_pipeline.depth_format = GpuFormat::Unknown;
	Check(d.CreatePipeline(bad_pipeline, rejected) == GpuResult::InvalidArgument, "depth test requires declared depth format");
	bad_pipeline = f.pipeline_desc; bad_pipeline.cull_mode = (GpuCullMode)999;
	Check(d.CreatePipeline(bad_pipeline, rejected) == GpuResult::InvalidArgument, "invalid culling enum rejected");
	Check(d.GetLivePipelineCount() == pipeline_count, "rejected pipeline creates no resource");
	GpuTextureId mismatched = f.Texture(GpuFormat::D32F, GpuTextureUsage_DepthAttachment, Size(32,32));
	const uint32 invalid_indices[] = {0,1,999999};
	GpuBufferId invalid = f.Buffer(GpuBufferUsage_Index, invalid_indices, sizeof(invalid_indices));
	GpuBufferId partial = f.Buffer(GpuBufferUsage_Index, nullptr, sizeof(kIndices));
	f.Result(d.WriteBuffer(partial, 0, kIndices, 4), "partial index upload accepted for later completion");
	GpuCommandListId cmd;
	f.Result(d.BeginCommands(cmd), "begin rejection command");
	auto pass = f.Pass(); pass.depth_load = GpuLoadOp::Load;
	Check(d.BeginRenderPass(cmd, pass) == GpuResult::InvalidState, "uninitialized depth Load rejected");
	pass = f.Pass(); pass.depth_target = mismatched;
	Check(d.BeginRenderPass(cmd, pass) == GpuResult::InvalidArgument, "depth size mismatch rejected");
	pass = f.Pass(); pass.clear_depth = std::numeric_limits<float>::quiet_NaN();
	Check(d.BeginRenderPass(cmd, pass) == GpuResult::InvalidArgument, "NaN depth clear rejected");
	pass.clear_depth = 1.5f;
	Check(d.BeginRenderPass(cmd, pass) == GpuResult::InvalidArgument, "out-of-range depth clear rejected");
	if(f.Result(d.BeginRenderPass(cmd, f.Pass()), "valid pass survives rejected descriptors") &&
	   f.Result(d.SetPipeline(cmd, f.pipeline), "bind pipeline for rejection checks") &&
	   f.Result(d.SetVertexBuffer(cmd, f.vertices), "bind vertices for rejection checks")) {
		Check(d.SetIndexBuffer(cmd, partial, GpuIndexType::UInt32) == GpuResult::InvalidState, "partial index initialization rejected");
		f.Result(d.SetIndexBuffer(cmd, f.indices, GpuIndexType::UInt32), "valid indices bind");
		Check(d.DrawIndexed(cmd, 3) == GpuResult::InvalidState, "missing graphics parameters rejected");
		auto params = Identity();
		Check(d.SetGraphicsParameters(cmd, &params, 124) == GpuResult::InvalidArgument, "short parameter payload rejected");
		f.Result(d.SetGraphicsParameters(cmd, &params, sizeof(params)), "complete parameters bind");
		f.Result(d.DrawIndexed(cmd, 3), "valid draw populates bounded index-maximum cache");
		Check(d.DestroyBuffer(f.indices) == GpuResult::InvalidState, "index destruction while referenced commands live rejected");
		Check(d.DestroyTexture(f.depth) == GpuResult::InvalidState, "depth destruction while commands live rejected");
		Check(d.DrawIndexed(cmd, 100) == GpuResult::InvalidArgument, "index range overflow rejected");
		Check(d.Draw(cmd, 100) == GpuResult::InvalidArgument, "vertex range overflow rejected");
		f.Result(d.SetIndexBuffer(cmd, invalid, GpuIndexType::UInt32), "corrupt index buffer binds for checked draw");
		Check(d.DrawIndexed(cmd, 3) == GpuResult::InvalidArgument, "index value outside vertices rejected before GPU recording");
		Check(d.WriteBuffer(f.indices, 0, kIndices, sizeof(kIndices)) == GpuResult::InvalidState, "index mutation while commands live rejected");
	}
	f.Result(d.DiscardCommands(cmd), "discard active unsubmitted pass");
	Check(d.GetLiveCommandCount() == 0, "discard releases command ownership");
	Check(d.DiscardCommands(cmd) == GpuResult::InvalidHandle, "discard is not silently repeated");
	f.Result(d.BeginCommands(cmd), "begin post-discard command");
	pass = f.Pass(); pass.depth_load = GpuLoadOp::Load;
	Check(d.BeginRenderPass(cmd, pass) == GpuResult::InvalidState, "discard never publishes depth initialization");
	f.Result(d.DiscardCommands(cmd), "discard rejected command");
	// Reuse the cached range after a successful partial upload changes its maximum.
	const uint32 outside = 999999, restored = 0;
	f.Result(d.WriteBuffer(f.indices, 0, &outside, sizeof(outside)), "partial index rewrite succeeds after discard");
	f.Result(d.BeginCommands(cmd), "begin rewritten-index check");
	f.Result(d.BeginRenderPass(cmd, f.Pass()), "begin rewritten-index pass");
	f.Result(d.SetPipeline(cmd, f.pipeline), "bind rewritten-index pipeline");
	f.Result(d.SetVertexBuffer(cmd, f.vertices), "bind rewritten-index vertices");
	f.Result(d.SetIndexBuffer(cmd, f.indices, GpuIndexType::UInt32), "bind rewritten index data");
	auto params = Identity();
	f.Result(d.SetGraphicsParameters(cmd, &params, sizeof(params)), "bind rewritten-index parameters");
	Check(d.DrawIndexed(cmd, 3) == GpuResult::InvalidArgument, "partial write invalidates the cached index maximum");
	f.Result(d.DiscardCommands(cmd), "discard rewritten-index check");
	f.Result(d.WriteBuffer(f.indices, 0, &restored, sizeof(restored)), "restore index for rendering oracle");
	f.Result(d.DestroyBuffer(invalid), "index resource may destroy after discard");
	for(int i = 0; i < f.buffers.GetCount(); ++i) if(f.buffers[i] == invalid) { f.buffers.Remove(i); break; }
	f.Result(d.DestroyTexture(mismatched), "depth resource may destroy after discard");
	for(int i = 0; i < f.textures.GetCount(); ++i) if(f.textures[i] == mismatched) { f.textures.Remove(i); break; }

}

void CheckOracle(const Vector<byte>& pixels)
{
	if(!Check(pixels.GetCount() == 64*64*4, "color oracle has expected layout")) return;
	int compared = 0, wrong = 0;
	for(int y = 0; y < 64; ++y) for(int x = 0; x < 64; ++x) {
		// Independent barycentric expectation at pixel centres, no renderer math.
		const double px = (x + .5) / 32 - 1, py = 1 - (y + .5) / 32;
		const double c = (py + .7) / 1.55;
		const double b = (px + .8*(1-c)) / 1.6, a = 1-b-c;
		if(a < .04 || b < .04 || c < .04) continue; // Avoid rasterization edge conventions.
		const double za = a*.2 + b*.8 + c*.5, zb = a*.8 + b*.2 + c*.5;
		if(std::fabs(za-zb) < .01) continue; // Avoid an intentional exact-depth tie.
		const byte *rgba = pixels.Begin() + 4*(y*64+x);
		const bool red = za < zb;
		if(rgba[0] != (red ? 255 : 0) || rgba[1] != (red ? 0 : 255) || rgba[2] || rgba[3] != 255) ++wrong;
		++compared;
	}
	oracle_pixels += compared;
	Check(compared > 500 && wrong == 0, "intersecting depth planes agree with independent CPU color/depth oracle");
}

bool Same(const Vector<byte>& a, const Vector<byte>& b)
{
	return a.GetCount() == b.GetCount() && a.GetCount() > 0 &&
	       std::memcmp(a.Begin(), b.Begin(), a.GetCount()) == 0;
}
bool Black(const Vector<byte>& image)
{
	if(image.GetCount() != 64*64*4) return false;
	for(int i = 0; i < image.GetCount(); i += 4) if(image[i] || image[i+1] || image[i+2]) return false;
	return true;
}


void TestVisibleEdgeOverlay(Fixture& f)
{
	auto& d = f.device;
	GpuShaderDesc shader;
	shader.format = GpuShaderFormat::SpirV; shader.stage = GpuShaderStage::Fragment;
	shader.code = String(reinterpret_cast<const char *>(kUi3DEdgeFragment), sizeof(kUi3DEdgeFragment));
	GpuShaderId edge_shader;
	if(!f.Result(d.CreateShader(shader, edge_shader), "create visible edge shader")) return;
	f.shaders.Add(edge_shader);
	auto desc = f.pipeline_desc;
	desc.topology = GpuPrimitiveTopology::LineList; desc.fragment_shader = edge_shader;
	desc.depth_write = false; desc.depth_compare = GpuCompareOp::LessEqual;
	GpuPipelineId edge_pipeline = f.Pipeline(desc);
	desc.depth_compare = GpuCompareOp::Equal;
	GpuPipelineId probe_pipeline = f.Pipeline(desc);
	desc.depth_test = false;
	GpuPipelineId clip_pipeline = f.Pipeline(desc);
	if(!edge_pipeline.IsValid() || !probe_pipeline.IsValid() || !clip_pipeline.IsValid()) return;
	const Vertex quad[] = {
		{-.8f,-.8f,.5f,0,0,1,0,0,1,1}, { .8f,-.8f,.5f,0,0,1,0,0,1,1},
		{ .8f, .8f,.5f,0,0,1,0,0,1,1}, {-.8f, .8f,.5f,0,0,1,0,0,1,1}
	};
	const uint32 quad_indices[] = {0,1,2,0,2,3};
	// Black authored vertex colors deliberately oppose the requested green/red
	// outline. Selection/edge RGB must remain the uniform requested color.
	const Vertex lines[] = {
		{-.65f, .25f,.5f,0,0,1,0,0,0,1}, { .65f, .25f,.5f,0,0,1,0,0,0,1},
		{-.65f,-.25f,.75f,0,0,1,0,0,0,1}, { .65f,-.25f,.75f,0,0,1,0,0,0,1}
	};
	const uint32 line_indices[] = {0,1,2,3};
	const float outside_far = std::nextafter(1.0f, 2.0f);
	const Vertex clipped_lines[] = {
		{-.65f, .5f, 1e-7f,0,0,1,0,0,0,1}, { .65f, .5f, 1e-7f,0,0,1,0,0,0,1},
		{-.65f,   0,-1e-7f,0,0,1,0,0,0,1}, { .65f,   0,-1e-7f,0,0,1,0,0,0,1},
		{-.65f,-.5f,outside_far,0,0,1,0,0,0,1}, { .65f,-.5f,outside_far,0,0,1,0,0,0,1}
	};
	const uint32 clip_indices[] = {0,1,2,3,4,5};
	GpuBufferId quad_v = f.Buffer(GpuBufferUsage_Vertex, quad, sizeof(quad));
	GpuBufferId quad_i = f.Buffer(GpuBufferUsage_Index, quad_indices, sizeof(quad_indices));
	GpuBufferId line_v = f.Buffer(GpuBufferUsage_Vertex, lines, sizeof(lines));
	GpuBufferId line_i = f.Buffer(GpuBufferUsage_Index, line_indices, sizeof(line_indices));
	GpuBufferId clip_v = f.Buffer(GpuBufferUsage_Vertex, clipped_lines, sizeof(clipped_lines));
	GpuBufferId clip_i = f.Buffer(GpuBufferUsage_Index, clip_indices, sizeof(clip_indices));
	if(!quad_v.IsValid() || !quad_i.IsValid() || !line_v.IsValid() || !line_i.IsValid() || !clip_v.IsValid() || !clip_i.IsValid()) return;
	Parameters flat = Identity(), green = Identity(), red = Identity(), yellow = Identity();
	green.tint[0] = green.tint[2] = 0; green.normal[11] = 2e-7f;
	red.tint[1] = red.tint[2] = 0; red.normal[11] = 2e-7f;
	yellow.tint[2] = 0; // No bias: equal-depth probe must see the original solid depth.
	auto record = [&](GpuCommandListId cmd, GpuPipelineId pipeline, GpuBufferId vertices,
	                  GpuBufferId indices, const Parameters& parameters, int count, int first) {
		return f.Result(d.SetPipeline(cmd, pipeline), "edge oracle pipeline") &&
		       f.Result(d.SetVertexBuffer(cmd, vertices), "edge oracle vertices") &&
		       f.Result(d.SetIndexBuffer(cmd, indices, GpuIndexType::UInt32), "edge oracle indices") &&
		       f.Result(d.SetGraphicsParameters(cmd, &parameters, sizeof(parameters)), "edge oracle parameters") &&
		       f.Result(d.DrawIndexed(cmd, count, first), "edge oracle indexed draw");
	};
	auto finish = [&](GpuCommandListId cmd, Vector<byte>& pixels) {
		if(!f.Result(d.EndRenderPass(cmd), "end edge oracle pass") ||
		   !f.Result(d.EndCommands(cmd), "end edge oracle command") ||
		   !f.Result(d.Submit(cmd), "submit finite edge oracle")) { d.DiscardCommands(cmd); return false; }
		return Check(d.ReadTexturePixels(f.color, pixels) && pixels.GetCount() == 64*64*4, "read edge oracle pixels");
	};
	auto count_rgb = [](const Vector<byte>& pixels, int r, int g, int b) {
		int count = 0;
		for(int i = 0; i + 3 < pixels.GetCount(); i += 4)
			if(pixels[i] == r && pixels[i+1] == g && pixels[i+2] == b) ++count;
		return count;
	};
	GpuCommandListId cmd;
	if(!f.Result(d.BeginCommands(cmd), "begin solid and edge oracle")) return;
	if(!f.Result(d.BeginRenderPass(cmd, f.Pass()), "begin solid and edge pass") ||
	   !record(cmd, f.pipeline, quad_v, quad_i, flat, 6, 0) ||
	   !record(cmd, edge_pipeline, line_v, line_i, green, 2, 0) ||
	   !record(cmd, edge_pipeline, line_v, line_i, red, 2, 2)) { d.DiscardCommands(cmd); return; }
	Vector<byte> pixels;
	if(!finish(cmd, pixels)) return;
	Check(count_rgb(pixels,0,0,255) > 2000, "Solid plus edges retains opaque solid face interiors");
	Check(count_rgb(pixels,0,255,0) >= 35, "coplanar visible edge uses exact uniform color despite black authored colors");
	Check(count_rgb(pixels,255,0,0) == 0, "rear edge remains occluded behind the opaque solid");

	if(!f.Result(d.BeginCommands(cmd), "begin depth preservation probe")) return;
	auto load = f.Pass(); load.color_load = GpuLoadOp::Load; load.depth_load = GpuLoadOp::Load;
	if(!f.Result(d.BeginRenderPass(cmd, load), "load solid depth after edge overlay") ||
	   !record(cmd, probe_pipeline, line_v, line_i, yellow, 2, 0)) { d.DiscardCommands(cmd); return; }
	if(!finish(cmd, pixels)) return;
	Check(count_rgb(pixels,255,255,0) >= 35, "edge overlay does not modify stored solid depth");

	if(!f.Result(d.BeginCommands(cmd), "begin biased edge clipping oracle")) return;
	if(!f.Result(d.BeginRenderPass(cmd, f.Pass()), "clear biased edge clipping target") ||
	   !record(cmd, clip_pipeline, clip_v, clip_i, green, 2, 0) ||
	   !record(cmd, clip_pipeline, clip_v, clip_i, red, 4, 2)) { d.DiscardCommands(cmd); return; }
	if(!finish(cmd, pixels)) return;
	Check(count_rgb(pixels,0,255,0) >= 35, "edge just inside near plane survives fragment bias");
	Check(count_rgb(pixels,255,0,0) == 0, "fragment bias cannot pull near/far-clipped geometry into view");
}

void TestRendering(Fixture& f)
{
	Vector<byte> forward, reverse, short_image, orientation;
	const uint64 allocated = f.device.GetLiveAllocationBytes();
	if(f.Render(forward)) CheckOracle(forward);
	if(f.Render(reverse, true)) CheckOracle(reverse);
	Check(Same(forward, reverse), "depth result is independent of primitive submission order");
	f.Render(short_image, false, true);
	Check(Same(forward, short_image), "UInt16 and UInt32 index paths produce the same pixels");
	auto unflipped = Identity(false);
	f.Render(orientation, false, false, GpuPipelineId(), &unflipped, true);
	Check(Same(forward, orientation), "matrix Y flip and explicit viewport Y flip are equivalent exactly once");
	Check(f.device.GetLiveAllocationBytes() == allocated && f.device.GetLiveCommandCount() == 0,
	      "finite steady-state draws retain bounded allocations and consume command state");

	auto no_depth = f.pipeline_desc; no_depth.depth_test = no_depth.depth_write = false;
	GpuPipelineId clipping = f.Pipeline(no_depth);
	Parameters clip_parameters = Identity();
	clip_parameters.mvp[10] = 0; clip_parameters.mvp[14] = -.2f;
	Vector<byte> clipped;
	f.Render(clipped, false, false, clipping, &clip_parameters);
	Check(Black(clipped), "near-plane clipping rejects negative clip depth even with depth test disabled");
	clip_parameters.mvp[14] = 1.2f;
	f.Render(clipped, false, false, clipping, &clip_parameters);
	Check(Black(clipped), "far-plane clipping rejects clip depth greater than W");

	auto cull = f.pipeline_desc; cull.cull_mode = GpuCullMode::Back;
	cull.front_face = GpuFrontFace::CounterClockwise;
	GpuPipelineId ccw = f.Pipeline(cull);
	cull.front_face = GpuFrontFace::Clockwise;
	GpuPipelineId cw = f.Pipeline(cull);
	Vector<byte> ccw_image, cw_image;
	f.Render(ccw_image, false, false, ccw, nullptr, false, true);
	f.Render(cw_image, false, false, cw, nullptr, false, true);
	Check(Black(ccw_image) != Black(cw_image), "front-face selection reverses back-face culling");
	const bool ccw_visible = !Black(ccw_image);
	auto mirror = Identity(); mirror.mvp[0] = -1;
	f.Render(ccw_image, false, false, ccw, &mirror, false, true);
	Check((!Black(ccw_image)) != ccw_visible, "mirrored transform reverses winding and culling");

	// World = Y-rotation(90 degrees) * positive nonuniform scale(2,1,.5).
	// Projection cancels its position change, but the independent inverse-transpose
	// normal columns are (0,0,-.5), (0,1,0), (2,0,0). For input (1,1,1),
	// the shader must light the normalized world normal (2,1,-.5).
	Vertex lit_vertices[6]; std::memcpy(lit_vertices, kVertices, sizeof(kVertices));
	for(auto& v : lit_vertices) v.nx = v.ny = v.nz = 1;
	f.Result(f.device.WriteBuffer(f.vertices, 0, lit_vertices, sizeof(lit_vertices)), "upload independent normal oracle");
	auto lighting = Identity();
	for(float& n : lighting.normal) n = 0;
	lighting.normal[2] = -.5f; lighting.normal[3] = 1; // key/fill lighting enabled
	lighting.normal[5] = 1; lighting.normal[8] = 2;
	Vector<byte> lit;
	if(f.Render(lit, false, false, GpuPipelineId(), &lighting, false, true)) {
		const double normal_length = std::sqrt(5.25);
		const double key = max(0.0, (-.45*2 + .8 - .35*.5) / (normal_length*std::sqrt(.965)));
		const double fill = max(0.0, (.8*2 + .2 + .2*.5) / (normal_length*std::sqrt(.72)));
		const int expected = (int)std::lround(255*(.24 + .66*key + .10*fill));
		const int pixel = 4*(32*64 + 32);
		Check(lit.GetCount() == 64*64*4 && abs((int)lit[pixel]-expected) <= 1 && lit[pixel+1] == 0 && lit[pixel+2] == 0,
		      "GPU lighting matches independent rotated nonuniform inverse-transpose normal oracle");
	}
	f.Result(f.device.WriteBuffer(f.vertices, 0, kVertices, sizeof(kVertices)), "restore flat test vertices");

	GpuCommandListId cmd;
	f.Result(f.device.BeginCommands(cmd), "begin stored-depth load check");
	auto pass = f.Pass(); pass.depth_load = GpuLoadOp::Load; pass.depth_store = GpuStoreOp::DontCare;
	f.Result(f.device.BeginRenderPass(cmd, pass), "stored depth may load");
	f.Result(f.device.EndRenderPass(cmd), "end discarded-depth pass");
	Check(f.device.BeginRenderPass(cmd, pass) == GpuResult::InvalidState,
	      "depth DontCare store makes later Load invalid within the same command list");
	f.Result(f.device.DiscardCommands(cmd), "discard pending depth state");
	f.Result(f.device.BeginCommands(cmd), "begin persistent-depth check");
	f.Result(f.device.BeginRenderPass(cmd, pass), "discard retains previously submitted depth validity");
	f.Result(f.device.EndRenderPass(cmd), "end persistent-depth check");
	f.Result(f.device.EndCommands(cmd), "end persistent-depth command");
	f.Result(f.device.Submit(cmd), "submit depth DontCare invalidation");
	f.Result(f.device.BeginCommands(cmd), "begin invalidated-depth check");
	Check(f.device.BeginRenderPass(cmd, pass) == GpuResult::InvalidState, "submitted depth DontCare invalidates subsequent Load");
	f.Result(f.device.DiscardCommands(cmd), "finish invalidated-depth check");
}
}

CONSOLE_APP_MAIN
{
	using namespace VulkanTestHooks;
	ClearVulkanRuntimeDeviceDiagnostics();
	HWND window = CreateWindowExW(0, L"STATIC", L"RenderVulkan3DTest", WS_POPUP,
	                            0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
	VulkanSurfaceSession session;
	if(Check(window != nullptr, "hidden finite-test window created")) {
		GpuNativeWindowDesc native; native.kind = GpuNativeWindowKind::Win32; native.handle = (uintptr_t)window;
		if(Check(session.Open(true, native, &Resolver) && session.IsReady(), "validated Vulkan session opened")) {
			Check(session.GetReport().validation_available && session.GetReport().debug_utils_available &&
			      GetVulkanRuntimeDeviceDiagnostics().debug_messenger_live_count > 0,
			      "validation layer and diagnostic messenger are actually active");
			{
				VulkanGpuDevice device(session);
				if(Check(device.IsReady(), "Vulkan device adapter ready")) {
					{
						Fixture fixture(device);
						if(fixture.Init()) {
							TestRejectionAndDiscard(fixture);
							TestRendering(fixture);
							TestVisibleEdgeOverlay(fixture);
						}
					}
					Check(device.GetLiveBufferCount() == 0 && device.GetLiveTextureCount() == 0 &&
					      device.GetLiveShaderCount() == 0 && device.GetLivePipelineCount() == 0 && device.GetLiveCommandCount() == 0,
					      "all explicit mesh/depth resources released");
				}
			}
		}
	}
	session.Close();
	Check(session.GetReport().validation_error_count == 0, "zero Vulkan validation errors");
	Check(session.GetReport().validation_warning_count == 0, "zero Vulkan validation warnings");
	if(session.GetReport().validation_error_count || session.GetReport().validation_warning_count)
		for(const String& message : session.GetReport().validation_messages) Cout() << message << EOL;
	if(window) DestroyWindow(window);
	auto live = GetVulkanRuntimeDeviceDiagnostics();
	Check(live.runtime_live_count == 0 && live.instance_live_count == 0 && live.debug_messenger_live_count == 0 &&
	      live.surface_live_count == 0 && live.device_live_count == 0 && live.swapchain_live_count == 0,
	      "Vulkan session ownership released");
	Check(oracle_pixels > 1000, "pixel oracle actually executed");
	Cout() << "RenderVulkan3DTest: " << checks << " checks, " << oracle_pixels << " oracle pixels, " << failures << " failures" << EOL;
	if(failures) SetExitCode(1);
}
