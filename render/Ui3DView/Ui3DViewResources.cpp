#include "Ui3DViewRenderer.h"
#include "../../shaders/Ui3DSurfaceVertSpirv.h"
#include "../../shaders/Ui3DSurfaceFragSpirv.h"
#include "../../shaders/Ui3DSurfaceEdgeFragSpirv.h"
#include "../../shaders/Ui3DResolveVertSpirv.h"
#include "../../shaders/Ui3DResolveFragSpirv.h"
#include <algorithm>
#include <cmath>

namespace Upp { namespace Ui3DInternal {
bool ViewContent::Result(GpuDevice& device, GpuResult result, const char *operation, String& error)
{
	if(result == GpuResult::Ok) return true;
	error = String(operation) + ": " + DumpGpuResult(result);
	String detail = device.GetLastError();
	if(!detail.IsEmpty()) error << " (" << detail << ")";
	state->error = error;
	return false;
}

bool ViewContent::Buffer(GpuDevice& device, int usage, const void *data, int64 bytes,
                         GpuBufferId& id, String& error)
{
	GpuBufferDesc desc;
	desc.size = bytes;
	desc.usage = usage | GpuBufferUsage_TransferDst;
	desc.label = "Ui3DView retained geometry";
	if(!Result(device, device.CreateBuffer(desc, id), "create mesh buffer", error)) return false;
	if(!Result(device, device.WriteBuffer(id, 0, data, bytes), "upload mesh buffer", error)) {
		device.DestroyBuffer(id); id = GpuBufferId(); return false;
	}
	state->stats.uploaded_bytes += bytes;
	return true;
}

void ViewContent::ReleaseMesh(GpuDevice& device, MeshGpu& mesh)
{
	if(mesh.edges.IsValid()) device.DestroyBuffer(mesh.edges);
	if(mesh.indices.IsValid()) device.DestroyBuffer(mesh.indices);
	if(mesh.vertices.IsValid()) device.DestroyBuffer(mesh.vertices);
	mesh = MeshGpu();
}

bool ViewContent::EnsureMesh(GpuDevice& device, std::shared_ptr<const Scene3DMesh> source, String& error)
{
	if(meshes.find(source.get()) != meshes.end()) return true;
	MeshGpu mesh;
	mesh.source = source;
	std::vector<MeshVertex> vertices;
	vertices.reserve(source->GetVertices().size());
	for(const auto& v : source->GetVertices()) {
		MeshVertex vertex = {
			(float)v.position.x, (float)v.position.y, (float)v.position.z,
			(float)v.normal.x, (float)v.normal.y, (float)v.normal.z,
			(float)v.u, (float)v.v, v.r, v.g, v.b, v.a
		};
		vertices.push_back(vertex);
	}
	const auto& indices = source->GetIndices();
	struct Edge { uint32 a, b; };
	static_assert(sizeof(Edge) == 2 * sizeof(uint32), "packed edge index pair");
	std::vector<Edge> edges;
	edges.reserve(indices.size());
	auto edge = [&](uint32 a, uint32 b) { edges.push_back({ std::min(a, b), std::max(a, b) }); };
	for(size_t i = 0; i < indices.size(); i += 3) {
		edge(indices[i], indices[i + 1]); edge(indices[i + 1], indices[i + 2]); edge(indices[i + 2], indices[i]);
	}
	std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
		return a.a < b.a || (a.a == b.a && a.b < b.b);
	});
	edges.erase(std::unique(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
		return a.a == b.a && a.b == b.b;
	}), edges.end());
	bool ok = Buffer(device, GpuBufferUsage_Vertex, vertices.data(),
	                 (int64)vertices.size() * sizeof(MeshVertex), mesh.vertices, error) &&
	          Buffer(device, GpuBufferUsage_Index, indices.data(),
	                 (int64)indices.size() * sizeof(uint32), mesh.indices, error) &&
	          Buffer(device, GpuBufferUsage_Index, edges.data(),
	                 (int64)edges.size() * sizeof(Edge), mesh.edges, error);
	if(!ok) { ReleaseMesh(device, mesh); return false; }
	mesh.index_count = (int)indices.size();
	mesh.edge_count = (int)edges.size() * 2;
	meshes.emplace(source.get(), std::move(mesh));
	++state->stats.mesh_uploads;
	state->stats.resident_meshes = (int)meshes.size();
	return true;
}

bool ViewContent::EnsureDecorations(GpuDevice& device, Size viewport, String& error)
{
	if(!state->options.show_grid && !state->options.show_axes) return true;
	Ui3DViewGridLayout next;
	if(!ComputeUi3DViewGridLayout(state->camera, state->options, viewport, next)) {
		error = state->error = "invalid camera/grid state"; return false;
	}
	if(have_grid_layout && next == grid_layout && decoration_revision == state->decoration_revision) return true;
	std::vector<MeshVertex> vertices;
	std::vector<uint32> indices;
	auto line = [&](Scene3DVec3 a, Scene3DVec3 b, Rgba8 color) {
		const uint32 index = (uint32)vertices.size();
		for(const Scene3DVec3& p : { a, b })
			vertices.push_back({(float)p.x, (float)p.y, (float)p.z, 0, 1, 0, 0, 0,
			                    Linear(color.r), Linear(color.g), Linear(color.b), 1});
		indices.push_back(index); indices.push_back(index + 1);
	};
	const double step = next.spacing, span = next.extent * step;
	for(int i = -next.extent; i <= next.extent; ++i) {
		const double u = next.center_u + i * step, v = next.center_v + i * step;
		line(next.ToWorld(next.center_u - span, v), next.ToWorld(next.center_u + span, v), state->options.grid_color);
		line(next.ToWorld(u, next.center_v - span), next.ToWorld(u, next.center_v + span), state->options.grid_color);
	}
	MeshGpu new_grid, new_axes;
	bool ok = Buffer(device, GpuBufferUsage_Vertex, vertices.data(),
	                 (int64)vertices.size() * sizeof(MeshVertex), new_grid.vertices, error) &&
	          Buffer(device, GpuBufferUsage_Index, indices.data(),
	                 (int64)indices.size() * sizeof(uint32), new_grid.indices, error);
	new_grid.index_count = (int)indices.size();
	vertices.clear(); indices.clear();
	const double length = std::min(span, 3.0 * step);
	line(Scene3DVec3(), Scene3DVec3(length, 0, 0), Rgba8(213, 91, 94));
	line(Scene3DVec3(), Scene3DVec3(0, length, 0), Rgba8(110, 186, 120));
	line(Scene3DVec3(), Scene3DVec3(0, 0, length), Rgba8(99, 153, 226));
	ok = ok &&
	     Buffer(device, GpuBufferUsage_Vertex, vertices.data(),
	            (int64)vertices.size() * sizeof(MeshVertex), new_axes.vertices, error) &&
	     Buffer(device, GpuBufferUsage_Index, indices.data(),
	            (int64)indices.size() * sizeof(uint32), new_axes.indices, error);
	new_axes.index_count = (int)indices.size();
	if(!ok) { ReleaseMesh(device, new_grid); ReleaseMesh(device, new_axes); return false; }
	ReleaseMesh(device, grid); ReleaseMesh(device, axes);
	grid = std::move(new_grid); axes = std::move(new_axes);
	grid_layout = next; have_grid_layout = true;
	decoration_revision = state->decoration_revision;
	return true;
}

bool ViewContent::EnsureDepth(GpuDevice& device, Size size, String& error)
{
	if(depth.IsValid() && size == depth_size) return true;
	if(size.cx <= 0 || size.cy <= 0 || (int64)size.cx * size.cy > 16 * 1024 * 1024) {
		error = state->error = "Ui3DView target must be nonempty and at most 16 million pixels";
		return false;
	}
	GpuTextureCapabilities caps;
	if(!Result(device, device.GetTextureCapabilities(GpuFormat::D32F,
	                  GpuTextureUsage_DepthAttachment, caps), "query depth support", error)) return false;
	if(!caps.depth_attachment) {
		error = state->error = "backend does not support the required D32F depth attachment"; return false;
	}
	GpuTextureDesc desc;
	desc.size = size; desc.format = GpuFormat::D32F;
	desc.usage = GpuTextureUsage_DepthAttachment;
	desc.label = "Ui3DView retained depth";
	GpuTextureId replacement;
	if(!Result(device, device.CreateTexture(desc, replacement), "create depth target", error)) return false;
	if(depth.IsValid()) device.DestroyTexture(depth);
	depth = replacement; depth_size = size;
	++state->stats.depth_rebuilds;
	return true;
}


float Linear(byte value)
{
    const float x=value/255.0f;
    return x<=0.04045f?x/12.92f:std::pow((x+0.055f)/1.055f,2.4f);
}
bool IsSrgb(GpuFormat format)
{
    return format==GpuFormat::RGBA8Srgb || format==GpuFormat::BGRA8Srgb;
}
GpuClearColor ClearColor(Rgba8 color)
{
    GpuClearColor result;
    result.red=Linear(color.r); result.green=Linear(color.g); result.blue=Linear(color.b);
    result.alpha=1; return result;
}

bool ViewContent::EnsureTexture(GpuDevice& device,std::shared_ptr<const Scene3DTexture> source,
                               GpuTextureId& out,String& error)
{
    if(source) {
        const auto found=textures.find(source.get());
        if(found!=textures.end()) { out=found->second.texture; return true; }
    }
    else if(white_texture.IsValid()) { out=white_texture; return true; }
    const byte white[]={255,255,255,255};
    const Size size=source?source->GetSize():Size(1,1);
    const GpuFormat format=source && source->GetColorSpace()==Scene3DTextureColorSpace::Srgb?
        GpuFormat::RGBA8Srgb:GpuFormat::RGBA8;
    GpuTextureCapabilities caps;
    if(!Result(device,device.GetTextureCapabilities(format,
        GpuTextureUsage_Sampled|GpuTextureUsage_TransferDst,caps),"query material texture support",error))
        return false;
    if(!caps.sampled || !caps.linear_filter || !caps.transfer_dst) {
        error=state->error="The backend cannot linearly sample the declared material texture."; return false;
    }
    GpuTextureDesc desc;
    desc.size=size; desc.format=format; desc.usage=GpuTextureUsage_Sampled|GpuTextureUsage_TransferDst;
    desc.label=source?"Ui3DView immutable material pixels":"Ui3DView neutral white material";
    GpuTextureId texture;
    if(!Result(device,device.CreateTexture(desc,texture),"create material texture",error)) return false;
    GpuTextureWriteDesc write; write.size=size; write.row_pitch=int64(size.cx)*4;
    const void* data=source?static_cast<const void*>(source->GetPixels().data()):white;
    const int64 bytes=source?(int64)source->GetPixels().size():4;
    if(!Result(device,device.WriteTexture(texture,write,data,bytes),"upload immutable material pixels",error)) {
        device.DestroyTexture(texture); return false;
    }
    if(source) {
        TextureGpu entry; entry.source=source; entry.texture=texture;
        textures.emplace(source.get(),std::move(entry));
        state->stats.resident_texture_bytes+=bytes;
        state->stats.resident_textures=(int)textures.size();
    }
    else white_texture=texture;
    ++state->stats.texture_uploads; state->stats.uploaded_bytes+=bytes;
    out=texture; return true;
}

bool ViewContent::EnsureShaders(GpuDevice& device,String& error)
{
    struct ShaderSource {
        GpuShaderStage stage; const uint32_t* words; int bytes; const char* name; GpuShaderId* id;
    };
    const ShaderSource sources[]={
        {GpuShaderStage::Vertex,kUi3DSurfaceVertex,sizeof(kUi3DSurfaceVertex),"Ui3DView surface vertex",&vertex_shader},
        {GpuShaderStage::Fragment,kUi3DSurfaceFragment,sizeof(kUi3DSurfaceFragment),"Ui3DView material coverage",&surface_shader},
        {GpuShaderStage::Fragment,kUi3DSurfaceEdgeFragment,sizeof(kUi3DSurfaceEdgeFragment),"Ui3DView material edges",&edge_shader},
        {GpuShaderStage::Vertex,kUi3DResolveVertex,sizeof(kUi3DResolveVertex),"Ui3DView resolve vertex",&resolve_vertex_shader},
        {GpuShaderStage::Fragment,kUi3DResolveFragment,sizeof(kUi3DResolveFragment),"Ui3DView linear to sRGB resolve",&resolve_fragment_shader}
    };
    for(const auto& source:sources) if(!source.id->IsValid()) {
        GpuShaderDesc desc; desc.stage=source.stage; desc.format=GpuShaderFormat::SpirV;
        desc.code=String(reinterpret_cast<const char*>(source.words),source.bytes); desc.label=source.name;
        if(!Result(device,device.CreateShader(desc,*source.id),"create scene shader",error)) return false;
    }
    return true;
}

bool ViewContent::EnsurePipeline(GpuDevice& device,GpuFormat format,int kind,bool blend,
                                bool cull,bool repeat,GpuPipelineId& out,String& error)
{
    if(target_format!=format) {
        for(const auto& entry:pipelines) device.DestroyPipeline(entry.second);
        pipelines.clear(); target_format=format;
    }
    const auto key=std::make_tuple(kind,blend,cull,repeat);
    const auto found=pipelines.find(key);
    if(found!=pipelines.end()) { out=found->second; return true; }
    GpuPipelineDesc desc;
    desc.color_format=format; desc.vertex_shader=vertex_shader;
    desc.fragment_shader=kind?edge_shader:surface_shader;
    desc.vertex_layout=GpuVertexLayout::Position3Normal3Uv2Color4F;
    desc.parameter_bytes=sizeof(MeshParameters);
    desc.sampled_texture_count=1;
    desc.sampler_filter=GpuSamplerFilter::Linear;
    desc.sampler_address=repeat?GpuSamplerAddressMode::Repeat:GpuSamplerAddressMode::ClampToEdge;
    desc.blend_mode=blend?GpuBlendMode::SourceOver:GpuBlendMode::Opaque;
    desc.depth_format=GpuFormat::D32F; desc.depth_test=true; desc.depth_write=kind!=2 && !blend;
    desc.depth_compare=GpuCompareOp::LessEqual;
    desc.cull_mode=kind==0 && cull?GpuCullMode::Back:GpuCullMode::None;
    desc.front_face=GpuFrontFace::CounterClockwise;
    desc.topology=kind?GpuPrimitiveTopology::LineList:GpuPrimitiveTopology::TriangleList;
    desc.label=kind?"Ui3DView depth-tested material edges":"Ui3DView sampled material surface";
    GpuPipelineId pipeline;
    if(!Result(device,device.CreatePipeline(desc,pipeline),"create scene pipeline",error)) return false;
    pipelines.emplace(key,pipeline); out=pipeline; return true;
}

bool ViewContent::EnsureTarget(GpuDevice& device,const GpuFrameInfo& frame,
                              GpuFrameInfo& target,String& error)
{
    target=frame;
    if(frame.color_format!=GpuFormat::RGBA8 && frame.color_format!=GpuFormat::BGRA8 &&
       !IsSrgb(frame.color_format)) {
        error=state->error="Ui3DView requires an explicit RGBA/BGRA UNORM or sRGB target."; return false;
    }
    if(IsSrgb(frame.color_format)) {
        if(working_color.IsValid()) device.DestroyTexture(working_color);
        working_color=GpuTextureId(); working_size=Size(0,0); return true;
    }
    // Blend in a genuine sRGB attachment's linear working space, then encode
    // exactly once into an UNORM display target. Gamma-space blending is avoided.
    if(!working_color.IsValid() || working_size!=frame.size) {
        GpuTextureDesc desc; desc.size=frame.size; desc.format=GpuFormat::RGBA8Srgb;
        desc.usage=GpuTextureUsage_ColorAttachment|GpuTextureUsage_Sampled;
        desc.label="Ui3DView retained linear-blending attachment";
        GpuTextureId replacement;
        if(!Result(device,device.CreateTexture(desc,replacement),"create linear-blending target",error)) return false;
        if(working_color.IsValid()) device.DestroyTexture(working_color);
        working_color=replacement; working_size=frame.size; ++state->stats.working_target_rebuilds;
    }
    target.color_target=working_color; target.color_format=GpuFormat::RGBA8Srgb;
    return true;
}

bool ViewContent::Resolve(GpuDevice& device,const GpuFrameInfo& frame,String& error)
{
    if(IsSrgb(frame.color_format)) return true;
    if(resolve_format!=frame.color_format) {
        if(resolve_pipeline.IsValid()) device.DestroyPipeline(resolve_pipeline);
        resolve_pipeline=GpuPipelineId(); resolve_format=frame.color_format;
    }
    if(!resolve_pipeline.IsValid()) {
        GpuPipelineDesc desc; desc.color_format=frame.color_format;
        desc.vertex_shader=resolve_vertex_shader; desc.fragment_shader=resolve_fragment_shader;
        desc.vertex_layout=GpuVertexLayout::Position2Uv2Color4F;
        desc.sampled_texture_count=1; desc.sampler_filter=GpuSamplerFilter::Nearest;
        desc.blend_mode=GpuBlendMode::Opaque; desc.label="Ui3DView final sRGB encoding";
        if(!Result(device,device.CreatePipeline(desc,resolve_pipeline),"create color resolve",error)) return false;
    }
    if(!resolve_vertices.IsValid()) {
        // A fullscreen triangle in the same top-down viewport convention.
        const float vertices[]={
            -1,-1,0,0,1,1,1,1,
             3,-1,2,0,1,1,1,1,
            -1, 3,0,2,1,1,1,1
        };
        if(!Buffer(device,GpuBufferUsage_Vertex,vertices,sizeof(vertices),resolve_vertices,error)) return false;
    }
    GpuCommandListId command;
    if(!Result(device,device.BeginCommands(command),"begin color resolve",error)) return false;
    auto fail=[&] { device.DiscardCommands(command); return false; };
    GpuRenderPassDesc pass; pass.color_target=frame.color_target; pass.color_format=frame.color_format;
    pass.color_load=GpuLoadOp::Clear; pass.color_store=GpuStoreOp::Store; pass.flip_viewport_y=false;
    pass.label="Ui3DView linear to sRGB output";
    if(!Result(device,device.BeginRenderPass(command,pass),"begin resolve pass",error) ||
       !Result(device,device.SetPipeline(command,resolve_pipeline),"bind resolve pipeline",error) ||
       !Result(device,device.SetVertexBuffer(command,resolve_vertices),"bind resolve triangle",error) ||
       !Result(device,device.SetSampledTexture(command,0,working_color),"bind linear working texture",error) ||
       !Result(device,device.Draw(command,3),"draw color resolve",error) ||
       !Result(device,device.EndRenderPass(command),"end color resolve",error) ||
       !Result(device,device.EndCommands(command),"end resolve commands",error) ||
       !Result(device,device.Submit(command),"submit color resolve",error)) return fail();
    return true;
}

} }
