#include "Ui3DViewRenderer.h"
#include "../../shaders/Ui3DSplatVertSpirv.h"
#include "../../shaders/Ui3DSplatFragSpirv.h"
#include <algorithm>
#include <chrono>
#include <set>

namespace Upp { namespace Ui3DInternal {
namespace {
uint64 VertexBytes(const Scene3DSplats& source)
{
    return uint64(source.GetSplats().size())*4*sizeof(SplatVertex);
}
uint64 IndexBytes(const Scene3DSplats& source)
{
    return uint64(source.GetSplats().size())*6*sizeof(uint32);
}
bool SameDirection(const Scene3DVec3& a,const Scene3DVec3& b)
{
    return a.x==b.x && a.y==b.y && a.z==b.z;
}
}
void ViewContent::PruneSplats(GpuDevice& device,const std::shared_ptr<const Scene3DSnapshot>& scene)
{
    std::set<const Scene3DSplats*> used;
    std::set<String> instances;
    if(scene) for(const auto& object:scene->GetObjects())
        if(object.visible && object.splats && object.opacity>0) {
            used.insert(object.splats.get());instances.insert(object.id);
        }
    for(auto it=splat_resources.begin();it!=splat_resources.end();) {
        if(used.find(it->first)==used.end()) {
            device.DestroyBuffer(it->second.vertices);
            state->stats.resident_splat_bytes-=VertexBytes(*it->second.source);
            it=splat_resources.erase(it);
        }
        else ++it;
    }
    for(auto it=splat_orders.begin();it!=splat_orders.end();) {
        const auto* object=scene?scene->FindObject(it->first):nullptr;
        if(instances.find(it->first)==instances.end() || !object ||
           it->second.source!=object->splats) {
            device.DestroyBuffer(it->second.indices);
            state->stats.resident_splat_bytes-=IndexBytes(*it->second.source);
            it=splat_orders.erase(it);
        }
        else ++it;
    }
    state->stats.resident_splat_resources=int(splat_resources.size());
    state->stats.resident_splat_instances=int(splat_orders.size());
}

bool ViewContent::EnsureSplats(GpuDevice& device,const Scene3DObject& object,
                                const Scene3DVec3& forward,String& error)
{
    const auto source=object.splats;
    const auto& points=source->GetSplats();
    if(splat_resources.find(source.get())==splat_resources.end()) {
        SplatGpu resource;resource.source=source;
        std::vector<SplatVertex> vertices;
        vertices.reserve(points.size()*4);
        constexpr float corners[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
        for(const auto& point:points) for(const auto& corner:corners) {
            SplatVertex vertex{float(point.position.x),float(point.position.y),float(point.position.z),
                float(point.covariance[0]),float(point.covariance[1]),float(point.covariance[2]),
                float(point.covariance[3]),float(point.covariance[4]),float(point.covariance[5]),
                corner[0],corner[1],point.r,point.g,point.b,point.a};
            vertices.push_back(vertex);
        }
        if(!Buffer(device,GpuBufferUsage_Vertex,vertices.data(),VertexBytes(*source),
                   resource.vertices,error)) return false;
        splat_resources.emplace(source.get(),std::move(resource));
        ++state->stats.splat_uploads;
        state->stats.resident_splat_bytes+=VertexBytes(*source);
        state->stats.resident_splat_resources=int(splat_resources.size());
    }
    // Translating the camera or cloud cannot change its internal depth order.
    // Only this transformed view-forward vector is a sorting dependency.
    const auto& m=object.world.m;
    const Scene3DVec3 local_forward(m[0]*forward.x+m[1]*forward.y+m[2]*forward.z,
        m[4]*forward.x+m[5]*forward.y+m[6]*forward.z,
        m[8]*forward.x+m[9]*forward.y+m[10]*forward.z);
    auto found=splat_orders.find(object.id);
    if(found!=splat_orders.end() && found->second.valid &&
       found->second.source==source && SameDirection(found->second.local_forward,local_forward))
        return true;
    const auto begin=std::chrono::steady_clock::now();
    std::vector<std::pair<double,uint32>> order;
    order.reserve(points.size());
    for(size_t i=0;i<points.size();++i)
        order.emplace_back(points[i].position.Dot(local_forward),uint32(i));
    std::sort(order.begin(),order.end(),[](const auto& a,const auto& b) {
        return a.first!=b.first ? a.first>b.first : a.second<b.second;
    });
    std::vector<uint32> indices;
    indices.reserve(points.size()*6);
    for(const auto& entry:order) {
        const uint32 first=entry.second*4;
        for(uint32 offset:{0u,1u,2u,0u,2u,3u}) indices.push_back(first+offset);
    }
    if(found==splat_orders.end()) {
        SplatOrderGpu instance;instance.source=source;
        if(!Buffer(device,GpuBufferUsage_Index,indices.data(),IndexBytes(*source),
                   instance.indices,error)) return false;
        found=splat_orders.emplace(object.id,std::move(instance)).first;
        state->stats.resident_splat_bytes+=IndexBytes(*source);
        state->stats.resident_splat_instances=int(splat_orders.size());
    }
    else {
        if(!Result(device,device.WriteBuffer(found->second.indices,0,indices.data(),IndexBytes(*source)),
                   "update Gaussian depth order",error)) return false;
        state->stats.uploaded_bytes+=IndexBytes(*source);
    }
    found->second.local_forward=local_forward;found->second.valid=true;
    ++state->stats.splat_sort_updates;
    state->stats.splat_sort_us_last_frame+=uint64(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()-begin).count());
    return true;
}

bool ViewContent::EnsureSplatPipeline(GpuDevice& device,GpuFormat format,GpuPipelineId& output,String& error)
{
    if(!splat_vertex_shader.IsValid()) {
        GpuShaderDesc desc;desc.stage=GpuShaderStage::Vertex;desc.format=GpuShaderFormat::SpirV;
        desc.code=String(reinterpret_cast<const char*>(kUi3DSplatVertex),sizeof(kUi3DSplatVertex));
        desc.label="Ui3DView full covariance Gaussian vertex";
        if(!Result(device,device.CreateShader(desc,splat_vertex_shader),"create Gaussian vertex shader",error)) return false;
    }
    if(!splat_fragment_shader.IsValid()) {
        GpuShaderDesc desc;desc.stage=GpuShaderStage::Fragment;desc.format=GpuShaderFormat::SpirV;
        desc.code=String(reinterpret_cast<const char*>(kUi3DSplatFragment),sizeof(kUi3DSplatFragment));
        desc.label="Ui3DView analytic Gaussian coverage";
        if(!Result(device,device.CreateShader(desc,splat_fragment_shader),"create Gaussian fragment shader",error)) return false;
    }
    if(splat_pipeline.IsValid() && splat_format!=format) {
        device.DestroyPipeline(splat_pipeline);splat_pipeline=GpuPipelineId();
    }
    if(!splat_pipeline.IsValid()) {
        GpuPipelineDesc desc;desc.color_format=format;
        desc.vertex_shader=splat_vertex_shader;desc.fragment_shader=splat_fragment_shader;
        desc.vertex_layout=GpuVertexLayout::Position3Covariance6Corner2Color4F;
        desc.parameter_bytes=sizeof(SplatParameters);desc.sampled_texture_count=0;
        desc.blend_mode=GpuBlendMode::SourceOver;
        desc.depth_format=GpuFormat::D32F;desc.depth_test=true;desc.depth_write=false;
        desc.depth_compare=GpuCompareOp::LessEqual;desc.cull_mode=GpuCullMode::None;
        desc.label="Ui3DView Gaussian quads, linear source-over";
        if(!Result(device,device.CreatePipeline(desc,splat_pipeline),"create Gaussian pipeline",error)) return false;
        splat_format=format;
    }
    output=splat_pipeline;return true;
}

void ViewContent::CloseSplats(GpuDevice& device)
{
    for(auto& entry:splat_orders) device.DestroyBuffer(entry.second.indices);
    for(auto& entry:splat_resources) device.DestroyBuffer(entry.second.vertices);
    splat_orders.clear();splat_resources.clear();
    if(splat_pipeline.IsValid()) device.DestroyPipeline(splat_pipeline);
    if(splat_vertex_shader.IsValid()) device.DestroyShader(splat_vertex_shader);
    if(splat_fragment_shader.IsValid()) device.DestroyShader(splat_fragment_shader);
    splat_pipeline=GpuPipelineId();splat_vertex_shader=GpuShaderId();splat_fragment_shader=GpuShaderId();
    splat_format=GpuFormat::Unknown;
    state->stats.resident_splat_resources=state->stats.resident_splat_instances=0;
    state->stats.resident_splat_bytes=0;
}
} }
