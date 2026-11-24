#include "Ui3DViewRenderer.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace Upp { namespace Ui3DInternal {

MeshParameters ViewContent::Parameters(const Scene3DMatrix& view_projection,
    const Scene3DMatrix& world,const Scene3DMatrix& inverse,
    float r,float g,float b,float a,float opacity,const Scene3DMaterial& material,bool lighting) const
{
    MeshParameters parameters{};
    const auto mvp=view_projection*world;
    for(int i=0;i<16;++i) parameters.mvp[i]=(float)mvp.m[i];
    for(int column=0;column<3;++column) for(int row=0;row<3;++row)
        parameters.normal[column*4+row]=(float)inverse.m[row*4+column];
    parameters.normal[3]=lighting && !material.unlit?1.f:0.f;
    parameters.normal[7]=opacity;
    parameters.normal[11]=material.alpha_mode==Scene3DAlphaMode::Opaque?-2.f:
        material.alpha_mode==Scene3DAlphaMode::Blend?-1.f:material.alpha_cutoff;
    parameters.tint[0]=r; parameters.tint[1]=g; parameters.tint[2]=b; parameters.tint[3]=a;
    return parameters;
}

bool ViewContent::Render(GpuDevice& device,const GpuFrameInfo& frame,String& error)
{
    state->stats.splat_sort_us_last_frame=0;
    state->stats.splat_draws_last_frame=state->stats.splats_last_frame=0;
    if(!EnsureDepth(device,frame.size,error) || !EnsureShaders(device,error)) return false;
    GpuFrameInfo target;
    if(!EnsureTarget(device,frame,target,error) || !EnsureDecorations(device,frame.size,error)) return false;
    const auto scene=state->GetRenderScene();
    if(cached_scene!=scene) {
        PruneSplats(device,scene);
        std::set<const Scene3DMesh*> used_meshes;
        std::set<const Scene3DTexture*> used_textures;
        if(scene) for(const auto& object:scene->GetObjects()) if(object.visible && object.mesh && object.opacity>0) {
            used_meshes.insert(object.mesh.get());
            if(object.material.color_texture) used_textures.insert(object.material.color_texture.get());
        }
        for(auto it=meshes.begin();it!=meshes.end();) {
            if(used_meshes.find(it->first)==used_meshes.end()) {
                ReleaseMesh(device,it->second); it=meshes.erase(it);
            }
            else ++it;
        }
        for(auto it=textures.begin();it!=textures.end();) {
            if(used_textures.find(it->first)==used_textures.end()) {
                device.DestroyTexture(it->second.texture);
                state->stats.resident_texture_bytes-=it->second.source->GetPixels().size();
                it=textures.erase(it);
            }
            else ++it;
        }
        cached_scene=scene;
        state->stats.resident_meshes=(int)meshes.size();
        state->stats.resident_textures=(int)textures.size();
    }
    GpuTextureId neutral;
    if(!EnsureTexture(device,{},neutral,error)) return false;
    struct Entry {
        const Scene3DObject* object;
        GpuTextureId texture;
        GpuPipelineId pipeline;
        double depth;
    };
    std::vector<Entry> opaque,blended;
    Scene3DVec3 right,up,forward;
    state->camera.GetBasis(right,up,forward);
    const auto eye=state->camera.GetEye();
    const int kind=state->options.wireframe?1:0;
    if(scene) for(const auto& object:scene->GetObjects()) {
        if(!object.visible || object.opacity<=0) continue;
        if(object.splats) {
            GpuPipelineId pipeline;
            if(!EnsureSplats(device,object,forward,error) ||
               !EnsureSplatPipeline(device,target.color_format,pipeline,error)) return false;
            blended.push_back({&object,{},pipeline,(object.bounds.GetCenter()-eye).Dot(forward)});
            continue;
        }
        if(!object.mesh) continue;
        if(!EnsureMesh(device,object.mesh,error)) return false;
        GpuTextureId texture; GpuPipelineId pipeline;
        const bool blend=object.material.alpha_mode==Scene3DAlphaMode::Blend || object.opacity<1;
        if(!EnsureTexture(device,object.material.color_texture,texture,error) ||
           !EnsurePipeline(device,target.color_format,kind,blend,!object.material.double_sided,
                           object.material.repeat_uv,pipeline,error)) return false;
        Entry entry{&object,texture,pipeline,(object.bounds.GetCenter()-eye).Dot(forward)};
        (blend?blended:opaque).push_back(entry);
    }
    // Conventional surface compositing: stable far-to-near object order.
    // Intersecting transparent meshes require host splitting; this is not OIT.
    std::stable_sort(blended.begin(),blended.end(),[](const Entry& a,const Entry& b) { return a.depth>b.depth; });
    if(selection_revision!=state->selection_revision) {
        selected.Clear(); for(const String& id:state->selection) selected.FindAdd(id);
        selection_revision=state->selection_revision;
    }
    const auto view_projection=state->camera.GetViewProjection(frame.size);
    GpuCommandListId command;
    if(!Result(device,device.BeginCommands(command),"begin scene commands",error)) return false;
    auto fail=[&] { device.DiscardCommands(command); return false; };
    GpuRenderPassDesc pass;
    pass.flip_viewport_y=false; // Projection already applies Vulkan Y orientation.
    pass.color_target=target.color_target; pass.color_format=target.color_format;
    pass.color_load=GpuLoadOp::Clear; pass.color_store=GpuStoreOp::Store;
    pass.clear_color=ClearColor(state->options.background);
    pass.depth_target=depth; pass.depth_format=GpuFormat::D32F;
    pass.depth_load=GpuLoadOp::Clear; pass.depth_store=GpuStoreOp::DontCare;
    pass.clear_depth=1; pass.label="Ui3DView linear material composition";
    if(!Result(device,device.BeginRenderPass(command,pass),"begin scene pass",error)) return fail();
    int draws=0,textured_draws=0;
    GpuPipelineId current;
    auto draw=[&](const MeshGpu& mesh,GpuPipelineId pipeline,GpuTextureId texture,
                  bool edges,const MeshParameters& parameters) {
        if(pipeline!=current) {
            if(!Result(device,device.SetPipeline(command,pipeline),"bind scene pipeline",error)) return false;
            current=pipeline;
        }
        const auto index=edges && mesh.edges.IsValid()?mesh.edges:mesh.indices;
        const int count=edges && mesh.edges.IsValid()?mesh.edge_count:mesh.index_count;
        if(!Result(device,device.SetVertexBuffer(command,mesh.vertices),"bind scene vertices",error) ||
           !Result(device,device.SetIndexBuffer(command,index,GpuIndexType::UInt32),"bind scene indices",error) ||
           !Result(device,device.SetSampledTexture(command,0,texture),"bind material texture",error) ||
           !Result(device,device.SetGraphicsParameters(command,&parameters,sizeof(parameters)),"bind scene parameters",error) ||
           !Result(device,device.DrawIndexed(command,count),"draw material geometry",error)) return false;
        ++draws; return true;
    };
    auto surface=[&](const Entry& entry) {
        const auto& object=*entry.object;
        if(object.splats) {
            if(entry.pipeline!=current) {
                if(!Result(device,device.SetPipeline(command,entry.pipeline),"bind Gaussian pipeline",error)) return false;
                current=entry.pipeline;
            }
            SplatParameters parameters{};
            const auto mvp=view_projection*object.world;
            for(int i=0;i<16;++i) parameters.mvp[i]=float(mvp.m[i]);
            parameters.tint[0]=object.r;parameters.tint[1]=object.g;
            parameters.tint[2]=object.b;parameters.tint[3]=object.a;
            parameters.viewport_opacity[0]=float(frame.size.cx);
            parameters.viewport_opacity[1]=float(frame.size.cy);
            parameters.viewport_opacity[2]=object.opacity;
            if(state->options.show_selection && object.selection_outline && selected.Find(object.id)>=0) {
                parameters.highlight[0]=Linear(state->options.selection_color.r);
                parameters.highlight[1]=Linear(state->options.selection_color.g);
                parameters.highlight[2]=Linear(state->options.selection_color.b);
                parameters.highlight[3]=1;
            }
            const auto& resource=splat_resources.find(object.splats.get())->second;
            const auto& order=splat_orders.find(object.id)->second;
            const int points=int(object.splats->GetSplats().size());
            if(!Result(device,device.SetVertexBuffer(command,resource.vertices),"bind Gaussian vertices",error) ||
               !Result(device,device.SetIndexBuffer(command,order.indices,GpuIndexType::UInt32),"bind Gaussian order",error) ||
               !Result(device,device.SetGraphicsParameters(command,&parameters,sizeof(parameters)),"bind Gaussian parameters",error) ||
               !Result(device,device.DrawIndexed(command,points*6),"draw Gaussian densities",error)) return false;
            ++draws;++state->stats.splat_draws_last_frame;
            state->stats.splats_last_frame+=points;
            return true;
        }
        const auto parameters=Parameters(view_projection,object.world,object.inverse_world,
            object.r,object.g,object.b,object.a,object.opacity,object.material,state->options.lighting);
        if(object.material.color_texture) ++textured_draws;
        return draw(meshes.find(object.mesh.get())->second,entry.pipeline,entry.texture,kind==1,parameters);
    };
    for(const auto& entry:opaque) if(!surface(entry)) return fail();

    // Helpers test opaque depth but never modify it; transparent surfaces then
    // composite over the helpers and the complete opaque/masked scene.
    if(state->options.show_grid || state->options.show_axes) {
        GpuPipelineId helper;
        if(!EnsurePipeline(device,target.color_format,2,false,false,false,helper,error)) return fail();
        const auto identity=Scene3DMatrix::Identity();
        auto flat=Parameters(view_projection,identity,identity,1,1,1,1,1,Scene3DMaterial(),false);
        flat.normal[3]=-1; // Edge shader uses authored helper vertex colors.
        if(state->options.show_grid && !draw(grid,helper,neutral,false,flat)) return fail();
        if(state->options.show_axes && !draw(axes,helper,neutral,false,flat)) return fail();
    }
    for(const auto& entry:blended) if(!surface(entry)) return fail();

    auto outline=[&](const Scene3DObject& object,Rgba8 color) {
        GpuPipelineId pipeline;
        if(!EnsurePipeline(device,target.color_format,2,true,false,object.material.repeat_uv,pipeline,error)) return false;
        auto parameters=Parameters(view_projection,object.world,object.inverse_world,
            Linear(color.r),Linear(color.g),Linear(color.b),object.a,object.opacity,object.material,false);
        parameters.normal[3]=2; // Uniform overlay with bounded fragment-depth bias.
        const auto texture=object.material.color_texture?
            textures.find(object.material.color_texture.get())->second.texture:neutral;
        return draw(meshes.find(object.mesh.get())->second,pipeline,texture,true,parameters);
    };
    if(!state->options.wireframe && state->options.solid_edges && scene)
        for(const auto& object:scene->GetObjects())
            if(object.visible && object.mesh && object.opacity>0 && !outline(object,state->options.edge_color)) return fail();
    if(state->options.show_selection && scene)
        for(const auto& object:scene->GetObjects())
            if(object.visible && object.mesh && object.opacity>0 && object.selection_outline &&
               selected.Find(object.id)>=0 && !outline(object,state->options.selection_color)) return fail();
    if(!Result(device,device.EndRenderPass(command),"end scene pass",error) ||
       !Result(device,device.EndCommands(command),"end scene commands",error) ||
       !Result(device,device.Submit(command),"submit scene commands",error)) return fail();
    if(!DrawGate(device,target,error) || !Resolve(device,frame,error)) return false;
    ++state->stats.rendered_frames;
    state->stats.target_size=frame.size; state->stats.draws_last_frame=draws;
    state->stats.blended_draws_last_frame=(int)blended.size();
    state->stats.textured_draws_last_frame=textured_draws;
    state->stats.depth_active=true; state->error.Clear(); error.Clear(); return true;
}

void ViewContent::Close(GpuDevice& device)
{
    gate_renderer.reset();
    CloseSplats(device);
    for(auto& entry:meshes) ReleaseMesh(device,entry.second);
    meshes.clear(); cached_scene.reset();
    for(auto& entry:textures) device.DestroyTexture(entry.second.texture);
    textures.clear();
    ReleaseMesh(device,grid); ReleaseMesh(device,axes);
    for(const auto& entry:pipelines) device.DestroyPipeline(entry.second);
    pipelines.clear();
    for(auto* texture:{&depth,&working_color,&white_texture}) {
        if(texture->IsValid()) device.DestroyTexture(*texture);
        *texture=GpuTextureId();
    }
    if(resolve_pipeline.IsValid()) device.DestroyPipeline(resolve_pipeline);
    if(resolve_vertices.IsValid()) device.DestroyBuffer(resolve_vertices);
    for(auto* shader:{&vertex_shader,&surface_shader,&edge_shader,&resolve_vertex_shader,&resolve_fragment_shader}) {
        if(shader->IsValid()) device.DestroyShader(*shader);
        *shader=GpuShaderId();
    }
    resolve_pipeline=GpuPipelineId(); resolve_vertices=GpuBufferId();
    depth_size=working_size=Size(0,0); target_format=resolve_format=GpuFormat::Unknown;
    decoration_revision=selection_revision=0; selected.Clear(); have_grid_layout=false;
    state->stats.resident_meshes=state->stats.resident_textures=0;
    state->stats.resident_texture_bytes=0; state->stats.depth_active=false;
}

} // namespace Ui3DInternal

std::unique_ptr<GpuSurfaceContent> CreateUi3DViewContent(const std::shared_ptr<Ui3DViewState>& state)
{
    return std::unique_ptr<GpuSurfaceContent>(new Ui3DInternal::ViewContent(state));
}
}
