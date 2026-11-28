// Shared source residency and per-view ordering for all visible Gaussian instances.
#include "Ui3DViewRenderer.h"
#include "../../shaders/Ui3DSplatGlobalVertSpirv.h"
#include "../../shaders/Ui3DSplatOrderSpirv.h"
#include "../../shaders/Ui3DSplatFragSpirv.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <chrono>
#include <limits>
#include <mutex>

namespace Upp { namespace Ui3DInternal {

struct GlobalSplatAtlas {
    std::vector<std::shared_ptr<const Scene3DSplats>> sources;
    std::map<const Scene3DSplats*, uint32> first;
    uint32 count=0;
};

namespace {
constexpr uint32 Workgroup=256, Radix=16, InstanceLimit=4096, AnchorLimit=128;
constexpr int64 OrderStride=8, RunStride=32;
static_assert(Scene3DMaxSplats <= (1 << 20));

enum class SortPass : uint32 { Generate, Histogram, Prefix, Scatter, Runs, Small };
struct SortParameters {
    uint32 count=0, groups=0, shift=0, mode=0, instances=0, anchors=0;
};
static_assert(sizeof(SortParameters)==24);
struct DrawParameters { float width=0,height=0; uint32 run=0; };
static_assert(sizeof(DrawParameters)==12);

std::shared_ptr<const GlobalSplatAtlas> ShareAtlas(
    const std::vector<const Scene3DObject*>& objects)
{
    // Source identity stays stable across independently evaluated view snapshots.
    std::map<const Scene3DSplats*, std::shared_ptr<const Scene3DSplats>> sources;
    for(const auto* object : objects) sources.emplace(object->splats.get(), object->splats);
    std::vector<uintptr_t> key;
    key.reserve(sources.size());
    for(const auto& source : sources) key.push_back(reinterpret_cast<uintptr_t>(source.first));
    static std::mutex lock;
    static std::map<std::vector<uintptr_t>, std::weak_ptr<const GlobalSplatAtlas>> cache;
    std::lock_guard<std::mutex> guard(lock);
    for(auto it=cache.begin();it!=cache.end();) {
        if(it->second.expired()) it=cache.erase(it);
        else ++it;
    }
    auto found=cache.find(key);
    if(found!=cache.end())
        if(auto atlas=found->second.lock()) return atlas;
    auto atlas=std::make_shared<GlobalSplatAtlas>();
    for(const auto& source : sources) {
        atlas->first.emplace(source.first, atlas->count);
        atlas->sources.push_back(source.second);
        atlas->count+=uint32(source.second->GetSplats().size());
    }
    cache.emplace(std::move(key), atlas);
    return atlas;
}

uint32 OrderedDepth(float depth)
{
    const uint32 bits=std::bit_cast<uint32>(depth==0 ? 0.f : depth);
    return (bits & 0x80000000u) ? bits : ~(bits ^ 0x80000000u);
}
bool SameOrder(const GlobalSplatInstance& a,const GlobalSplatInstance& b)
{
    return a.source==b.source && a.first==b.first && a.count==b.count &&
           std::equal(a.depth,a.depth+4,b.depth);
}
bool CoversFiniteDepth(const Scene3DObject& object,const float *formula)
{
    const auto& box=object.splats->GetBounds();
    const double extrema[]={std::max(std::abs(box.min.x),std::abs(box.max.x)),
                            std::max(std::abs(box.min.y),std::abs(box.max.y)),
                            std::max(std::abs(box.min.z),std::abs(box.max.z))};
    double bound=std::abs(double(formula[3]));
    for(int i=0;i<3;++i) bound+=std::abs(double(formula[i]))*extrema[i];
    return std::isfinite(bound) && bound < std::numeric_limits<float>::max()*.5;
}
}

bool ViewContent::PrepareGlobalSplats(GpuDevice& device,std::vector<const Scene3DObject*>& objects,
    const std::vector<const Scene3DObject*>& mesh_anchors,const Scene3DVec3& forward,
    const Scene3DMatrix& projection,GpuFormat format,bool& active,String& error)
{
    auto& gpu=global_splats;
    active=false;
    const auto started=std::chrono::steady_clock::now();
    auto fallback=[&] {
        if(gpu.source.IsValid()) ReleaseGlobalSplatBuffers(device);
        return true;
    };
    if(!state->options.gpu_splat_sort || objects.empty() ||
       objects.size()>InstanceLimit || mesh_anchors.size()>AnchorLimit) return fallback();
    if(!gpu.checked) {
        auto& c=gpu.compute_caps; auto& v=gpu.draw_caps;
        gpu.available=device.GetComputeCapabilities(c)==GpuResult::Ok &&
            device.GetVertexPullCapabilities(v)==GpuResult::Ok && v.indirect_draw &&
            c.max_storage_buffers>=4 && v.max_storage_buffers>=4 &&
            c.max_local_size[0]>=Workgroup && c.max_local_invocations>=Workgroup &&
            c.max_group_count[0]>=(Scene3DMaxSplats+Workgroup-1)/Workgroup &&
            c.max_parameter_bytes>=sizeof(SortParameters) && c.max_dispatches_per_command>=26;
        gpu.checked=true;
    }
    if(!gpu.available) return fallback();
    // Preserve the scene's existing single-cloud tie with a conventional mesh.
    const auto* first_object=objects.front();
    std::sort(objects.begin(),objects.end(),[](const auto* a,const auto* b) { return a->id<b->id; });
    uint64 count=0;
    for(const auto* object : objects) count+=object->splats->GetSplats().size();
    if(!count || count>Scene3DMaxSplats) return fallback();
    const auto atlas=ShareAtlas(objects);
    const int64 source_bytes=int64(atlas->count)*SplatStorageStride;
    const int64 instance_bytes=int64(objects.size())*sizeof(GlobalSplatInstance);
    const uint32 groups=(uint32(count)+Workgroup-1)/Workgroup;
    const int64 key_bytes=int64(count)*OrderStride;
    const int64 histogram_bytes=int64(groups+1)*Radix*sizeof(uint32);
    const uint64 required=uint64(std::max({source_bytes,instance_bytes,key_bytes,histogram_bytes,
                                          int64(AnchorLimit+1)*RunStride}));
    if(required>gpu.compute_caps.max_storage_range || required>gpu.draw_caps.max_storage_range)
        return fallback();

    const auto origin=objects.front()->world.TransformPoint({});
    std::vector<GlobalSplatInstance> instances;
    instances.reserve(objects.size());
    Vector<String> ids;
    uint32 first=0;
    for(const auto* object : objects) {
        GlobalSplatInstance value;
        const auto clip=projection*object->world;
        for(int i=0;i<16;++i) value.projection[i]=float(clip.m[i]);
        value.tint[0]=object->r; value.tint[1]=object->g;
        value.tint[2]=object->b; value.tint[3]=object->a;
        if(state->options.show_selection && object->selection_outline && selected.Find(object->id)>=0) {
            value.highlight[0]=Linear(state->options.selection_color.r);
            value.highlight[1]=Linear(state->options.selection_color.g);
            value.highlight[2]=Linear(state->options.selection_color.b);
            value.highlight[3]=1;
        }
        const auto& m=object->world.m;
        value.depth[0]=float(m[0]*forward.x+m[1]*forward.y+m[2]*forward.z);
        value.depth[1]=float(m[4]*forward.x+m[5]*forward.y+m[6]*forward.z);
        value.depth[2]=float(m[8]*forward.x+m[9]*forward.y+m[10]*forward.z);
        value.depth[3]=float((object->world.TransformPoint({})-origin).Dot(forward));
        if(!CoversFiniteDepth(*object,value.depth)) return fallback();
        value.source=atlas->first.at(object->splats.get());
        value.first=first;
        value.count=uint32(object->splats->GetSplats().size());
        first+=value.count;
        value.opacity=std::bit_cast<uint32>(object->opacity);
        instances.push_back(value);
        ids.Add(object->id);
    }
    const auto depth=[&](const Scene3DObject& object) {
        return (object.bounds.GetCenter()-origin).Dot(forward);
    };
    std::vector<uint32> anchors;
    anchors.reserve(std::max<size_t>(2, 2*mesh_anchors.size()));
    for(const auto* mesh : mesh_anchors) {
        const double anchor=depth(*mesh);
        if(!std::isfinite(float(anchor))) return fallback();
        const uint32 key=OrderedDepth(float(anchor));
        anchors.push_back(anchors.empty() ? key : std::max(key,anchors[anchors.size()-2]));
        uint32 endpoint=UINT32_MAX;
        if(objects.size()==1) {
            const double cloud=depth(*first_object);
            endpoint=(cloud>anchor || (cloud==anchor && first_object<mesh)) ? uint32(count) : 0;
        }
        if(endpoint<=count && anchors.size()>1) endpoint=std::max(endpoint,anchors[anchors.size()-2]);
        anchors.push_back(endpoint);
    }
    if(anchors.empty()) anchors={0,0}; // A bound descriptor for the no-anchor dispatch.

    const bool new_atlas=gpu.atlas!=atlas;
    if(new_atlas) {
        const std::shared_ptr<const void> identity=atlas->sources.size()==1 ?
            std::shared_ptr<const void>(atlas->sources.front()) : std::shared_ptr<const void>(atlas);
        GpuBufferId replacement; bool uploaded=false;
        const auto acquired=device.AcquireImmutableBuffer(identity, SplatStorageVersion, GpuBufferUsage_Storage, source_bytes,
            [&](void *data,int64,String&) {
                auto* at=static_cast<byte*>(data);
                for(const auto& source : atlas->sources) {
                    FillSplatStorage(*source,at);
                    at+=source->GetSplats().size()*SplatStorageStride;
                }
                return true;
            },replacement,uploaded);
        if(acquired==GpuResult::Unsupported) return fallback();
        if(!Result(device,acquired,"acquire global Gaussian source atlas",error)) return false;
        if(gpu.source.IsValid()) device.DestroyBuffer(gpu.source);
        gpu.source=replacement;
        gpu.atlas=atlas;
        state->stats.splat_uploads+=uploaded ? 1 : 0;
        if(uploaded) state->stats.uploaded_bytes+=source_bytes;
        gpu.sort_pending=true;
    }
    if(!splat_resources.empty()) PruneSplats(device,{});
    auto capacity=[&](GpuBufferId& id,int64 bytes,int usage,GpuBufferMemory memory) {
        GpuBufferId replacement;
        GpuBufferDesc desc;
        desc.size=bytes;desc.usage=usage;desc.memory=memory;desc.label="Global Gaussian order";
        if(!Result(device,device.CreateBuffer(desc,replacement),"allocate global Gaussian order buffer",error))
            return false;
        if(id.IsValid()) device.DestroyBuffer(id);
        id=replacement;
        return true;
    };
    if(gpu.key_bytes<key_bytes || !gpu.a.IsValid() || !gpu.b.IsValid()) {
        gpu.sort_pending=true;
        if(!capacity(gpu.a,key_bytes,GpuBufferUsage_Storage,GpuBufferMemory::DeviceLocal) ||
           !capacity(gpu.b,key_bytes,GpuBufferUsage_Storage,GpuBufferMemory::DeviceLocal)) {
            ReleaseGlobalSplatBuffers(device);
            return false;
        }
        gpu.key_bytes=key_bytes;
    }
    if(gpu.histogram_bytes<histogram_bytes || !gpu.histogram.IsValid()) {
        gpu.sort_pending=true;
        if(!capacity(gpu.histogram,histogram_bytes,GpuBufferUsage_Storage,GpuBufferMemory::DeviceLocal)) {
            ReleaseGlobalSplatBuffers(device);
            return false;
        }
        gpu.histogram_bytes=histogram_bytes;
    }
    const bool new_instances=gpu.instance_bytes<instance_bytes || !gpu.instances.IsValid();
    if(new_instances) {
        if(!capacity(gpu.instances,instance_bytes,GpuBufferUsage_Storage,GpuBufferMemory::HostVisible)) return false;
        gpu.instance_bytes=instance_bytes;
    }
    if(!gpu.anchors.IsValid() &&
       !capacity(gpu.anchors,AnchorLimit*2*sizeof(uint32),GpuBufferUsage_Storage,GpuBufferMemory::HostVisible))
        return false;
    if(!gpu.runs.IsValid()) {
        if(!capacity(gpu.runs,(AnchorLimit+1)*RunStride,GpuBufferUsage_Storage|GpuBufferUsage_Indirect,
                     GpuBufferMemory::DeviceLocal)) return false;
        gpu.runs_pending=true;
    }
    const bool same_instances=instances.size()==gpu.previous_instances.size() && ids==gpu.instance_ids;
    gpu.sort_pending=gpu.sort_pending || !same_instances || !std::equal(instances.begin(),instances.end(),
                                                                       gpu.previous_instances.begin(),SameOrder);
    if(new_instances || !same_instances || std::memcmp(instances.data(),gpu.previous_instances.data(),size_t(instance_bytes))) {
        if(!Result(device,device.WriteBuffer(gpu.instances,0,instances.data(),instance_bytes),
                   "update Gaussian instance parameters",error)) return false;
        gpu.previous_instances=std::move(instances);
        gpu.instance_ids=pick(ids);
    }
    if(gpu.previous_anchors!=anchors || gpu.run_count!=mesh_anchors.size()+1) {
        if(!Result(device,device.WriteBuffer(gpu.anchors,0,anchors.data(),int64(anchors.size()*sizeof(uint32))),
                   "update transparent surface anchors",error)) return false;
        gpu.previous_anchors=std::move(anchors);
        gpu.runs_pending=true;
    }
    gpu.count=uint32(count);
    gpu.run_count=uint32(mesh_anchors.size()+1);

    auto shader=[&](GpuShaderId& id,GpuShaderStage stage,const uint32_t *code,int bytes) {
        if(id.IsValid()) return true;
        GpuShaderDesc desc;
        desc.stage=stage; desc.format=GpuShaderFormat::SpirV;
        desc.code=String(reinterpret_cast<const char *>(code), bytes);
        desc.label="Global Gaussian order";
        return Result(device,device.CreateShader(desc,id),"create global Gaussian shader",error);
    };
    if(!shader(gpu.sort_shader,GpuShaderStage::Compute,kUi3DSplatOrder,sizeof(kUi3DSplatOrder)) ||
       !shader(gpu.vertex_shader,GpuShaderStage::Vertex,kUi3DSplatGlobalVertex,sizeof(kUi3DSplatGlobalVertex)) ||
       !shader(gpu.fragment_shader,GpuShaderStage::Fragment,kUi3DSplatFragment,sizeof(kUi3DSplatFragment)))
        return false;
    if(!gpu.compute.IsValid()) {
        GpuComputePipelineDesc desc;
        desc.shader=gpu.sort_shader;desc.storage_buffer_count=4;desc.storage_read_only_mask=3;
        desc.parameter_bytes=sizeof(SortParameters);
        if(!Result(device,device.CreateComputePipeline(desc,gpu.compute),"create Gaussian order pipeline",error))
            return false;
    }
    if(gpu.graphics.IsValid() && gpu.format!=format) {
        device.DestroyPipeline(gpu.graphics); gpu.graphics=GpuPipelineId();
    }
    if(!gpu.graphics.IsValid()) {
        GpuPipelineDesc desc;
        desc.color_format=format;desc.depth_format=GpuFormat::D32F;
        desc.vertex_shader=gpu.vertex_shader;desc.fragment_shader=gpu.fragment_shader;
        desc.vertex_layout=GpuVertexLayout::StoragePull;desc.vertex_storage_count=4;
        desc.topology=GpuPrimitiveTopology::TriangleStrip;desc.parameter_bytes=sizeof(DrawParameters);
        desc.blend_mode=GpuBlendMode::SourceOver;desc.depth_test=true;desc.depth_compare=GpuCompareOp::LessEqual;
        if(!Result(device,device.CreatePipeline(desc,gpu.graphics),"create global Gaussian draw pipeline",error))
            return false;
        gpu.format=format;
    }
    state->stats.resident_splat_bytes=source_bytes+2*gpu.key_bytes+gpu.histogram_bytes+
        gpu.instance_bytes+AnchorLimit*2*sizeof(uint32)+(AnchorLimit+1)*RunStride;
    state->stats.resident_splat_resources=int(atlas->sources.size());
    state->stats.resident_splat_instances=int(objects.size());
    gpu.cpu_update_us=std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()-started).count();
    active=true;
    return true;
}

bool ViewContent::RecordGlobalSplatOrder(GpuDevice& device,GpuCommandListId list,String& error)
{
    auto& gpu=global_splats;
    if(!gpu.sort_pending && !gpu.runs_pending) return true;
    const auto started=std::chrono::steady_clock::now();
    if(!Result(device,device.SetComputePipeline(list,gpu.compute),"bind global Gaussian sort pipeline",error))
        return false;
    SortParameters parameters;
    parameters.count=gpu.count;
    parameters.groups=(gpu.count+Workgroup-1)/Workgroup;
    parameters.instances=uint32(gpu.previous_instances.size());
    parameters.anchors=gpu.run_count-1;
    auto dispatch=[&](SortPass pass,uint32 groups,GpuBufferId in,GpuBufferId meta,GpuBufferId out) {
        parameters.mode=uint32(pass);
        return Result(device,device.SetStorageBuffer(list,0,in),"bind Gaussian order input",error) &&
               Result(device,device.SetStorageBuffer(list,1,meta),"bind Gaussian order metadata",error) &&
               Result(device,device.SetStorageBuffer(list,2,out),"bind Gaussian order output",error) &&
               Result(device,device.SetStorageBuffer(list,3,gpu.histogram),"bind Gaussian order histogram",error) &&
               Result(device,device.SetComputeParameters(list,&parameters,sizeof(parameters)),"set Gaussian order pass",error) &&
               Result(device,device.Dispatch(list,groups),"dispatch Gaussian order",error);
    };
    if(gpu.sort_pending) {
        if(gpu.count<=Workgroup) {
            if(!dispatch(SortPass::Small,1,gpu.source,gpu.instances,gpu.a)) return false;
        }
        else {
            if(!dispatch(SortPass::Generate,parameters.groups,gpu.source,gpu.instances,gpu.a)) return false;
            auto in=gpu.a,out=gpu.b;
            for(parameters.shift=0;parameters.shift<32;parameters.shift+=4) {
                if(!dispatch(SortPass::Histogram,parameters.groups,in,gpu.instances,out) ||
                   !dispatch(SortPass::Prefix,Radix,in,gpu.instances,out) ||
                   !dispatch(SortPass::Scatter,parameters.groups,in,gpu.instances,out)) return false;
                std::swap(in,out);
            }
            // Eight passes leave the sorted keys back in a, also used for small clouds.
        }
    }
    if(!dispatch(SortPass::Runs,(gpu.run_count+Workgroup-1)/Workgroup,gpu.a,gpu.anchors,gpu.runs))
        return false;
    gpu.cpu_update_us+=std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()-started).count();
    return true;
}

bool ViewContent::DrawGlobalSplats(GpuDevice& device,GpuCommandListId list,GpuPipelineId& current,
                                  uint32 run,Size viewport,String& error)
{
    auto& gpu=global_splats;
    if(gpu.graphics!=current) {
        if(!Result(device,device.SetPipeline(list,gpu.graphics),"bind global Gaussian draw pipeline",error))
            return false;
        current=gpu.graphics;
    }
    const DrawParameters parameters{float(viewport.cx),float(viewport.cy),run};
    return Result(device,device.SetGraphicsParameters(list,&parameters,sizeof(parameters)),"set Gaussian draw segment",error) &&
        Result(device,device.SetVertexStorageBuffer(list,0,gpu.source),"bind shared Gaussian sources",error) &&
        Result(device,device.SetVertexStorageBuffer(list,1,gpu.a),"bind globally sorted Gaussian references",error) &&
        Result(device,device.SetVertexStorageBuffer(list,2,gpu.instances),"bind Gaussian instances",error) &&
        Result(device,device.SetVertexStorageBuffer(list,3,gpu.runs),"bind Gaussian draw segments",error) &&
        Result(device,device.DrawIndirect(list,gpu.runs,int64(run)*RunStride),"draw global Gaussian segment",error);
}

void ViewContent::CommitGlobalSplatOrder()
{
    auto& gpu=global_splats;
    state->stats.global_splat_order=true;
    if(gpu.sort_pending) {
        ++state->stats.splat_sort_updates;
        state->stats.splat_sort_us_last_frame=uint64(gpu.cpu_update_us);
    }
    gpu.sort_pending=gpu.runs_pending=false;
}

void ViewContent::ReleaseGlobalSplatBuffers(GpuDevice& device)
{
    auto& gpu=global_splats;
    for(auto* id : {&gpu.source,&gpu.instances,&gpu.anchors,&gpu.a,&gpu.b,&gpu.histogram,&gpu.runs}) {
        if(id->IsValid()) device.DestroyBuffer(*id);
        *id=GpuBufferId();
    }
    gpu.atlas.reset();gpu.instance_ids.Clear();gpu.previous_instances.clear();gpu.previous_anchors.clear();
    gpu.instance_bytes=gpu.key_bytes=gpu.histogram_bytes=0;
    gpu.count=gpu.run_count=0;
    gpu.sort_pending=gpu.runs_pending=false;
    if(splat_resources.empty()) {
        state->stats.resident_splat_bytes=0;
        state->stats.resident_splat_resources=state->stats.resident_splat_instances=0;
    }
}

void ViewContent::CloseGlobalSplats(GpuDevice& device)
{
    ReleaseGlobalSplatBuffers(device);
    auto& gpu=global_splats;
    for(auto id : {gpu.compute,gpu.graphics}) if(id.IsValid()) device.DestroyPipeline(id);
    for(auto id : {gpu.sort_shader,gpu.vertex_shader,gpu.fragment_shader}) if(id.IsValid()) device.DestroyShader(id);
    gpu=GlobalSplatGpu();
}

} }
