// Internal retained GPU resources for Ui3DView. The host owns scene values;
// these allocations are local to one device/content lifetime.
#ifndef _Ui3DView_Renderer_h_
#define _Ui3DView_Renderer_h_
#include "Ui3DViewContent.h"
#include <RenderGpu2D/RenderGpu2D.h>
#include <map>
#include <tuple>

namespace Upp { namespace Ui3DInternal {
struct MeshVertex {
    float x,y,z,nx,ny,nz,u,v,r,g,b,a;
};
static_assert(sizeof(MeshVertex)==48, "sampled RHI 3D vertex stride");
struct MeshParameters {
    float mvp[16];
    // Padded normal columns. w: shading policy, independent opacity, alpha mode.
    float normal[12];
    float tint[4];
};
static_assert(sizeof(MeshParameters)==128, "portable minimum graphics push payload");
struct MeshGpu {
    std::shared_ptr<const Scene3DMesh> source;
    GpuBufferId vertices,indices,edges;
    int index_count=0,edge_count=0;
};
struct SplatVertex {
    float x,y,z,xx,xy,xz,yy,yz,zz,corner_x,corner_y,r,g,b,a;
};
static_assert(sizeof(SplatVertex)==60, "native Gaussian vertex stride");
struct SplatParameters {
    float mvp[16],tint[4],viewport_opacity[4],highlight[4];
};
static_assert(sizeof(SplatParameters)==112, "bounded Gaussian graphics parameters");
struct SplatGpu {
    std::shared_ptr<const Scene3DSplats> source;
    GpuBufferId vertices;
};
struct SplatOrderGpu {
    std::shared_ptr<const Scene3DSplats> source;
    GpuBufferId indices;
    Scene3DVec3 local_forward;
    bool valid=false;
};
struct TextureGpu {
    std::shared_ptr<const Scene3DTexture> source;
    GpuTextureId texture;
};
float Linear(byte value);
bool IsSrgb(GpuFormat format);
GpuClearColor ClearColor(Rgba8 color);

class ViewContent final : public GpuSurfaceContent {
public:
    explicit ViewContent(std::shared_ptr<Ui3DViewState> value):state(std::move(value)) {}
    bool Render(GpuDevice&,const GpuFrameInfo&,String&) override;
    void Close(GpuDevice&) override;
private:
    std::shared_ptr<Ui3DViewState> state;
    std::shared_ptr<const Scene3DSnapshot> cached_scene;
    std::map<const Scene3DMesh*,MeshGpu> meshes;
    std::map<const Scene3DTexture*,TextureGpu> textures;
    std::map<const Scene3DSplats*,SplatGpu> splat_resources;
    std::map<String,SplatOrderGpu> splat_orders;
    GpuShaderId splat_vertex_shader,splat_fragment_shader;
    GpuPipelineId splat_pipeline;
    GpuFormat splat_format=GpuFormat::Unknown;
    MeshGpu grid,axes;
    uint64 decoration_revision=0,selection_revision=0;
    Index<String> selected;
    GpuShaderId vertex_shader,surface_shader,edge_shader,resolve_vertex_shader,resolve_fragment_shader;
    // kind: 0 filled surface, 1 primary wire, 2 overlay; followed by blend/cull/repeat.
    std::map<std::tuple<int,bool,bool,bool>,GpuPipelineId> pipelines;
    GpuPipelineId resolve_pipeline;
    GpuBufferId resolve_vertices;
    GpuTextureId depth,working_color,white_texture;
    Size depth_size=Size(0,0),working_size=Size(0,0);
    GpuFormat target_format=GpuFormat::Unknown,resolve_format=GpuFormat::Unknown;
    Ui3DViewGridLayout grid_layout;
    bool have_grid_layout=false;
    std::unique_ptr<UiRenderer2D> gate_renderer;

    bool Result(GpuDevice&,GpuResult,const char*,String&);
    bool Buffer(GpuDevice&,int,const void*,int64,GpuBufferId&,String&);
    void ReleaseMesh(GpuDevice&,MeshGpu&);
    bool EnsureMesh(GpuDevice&,std::shared_ptr<const Scene3DMesh>,String&);
    bool EnsureTexture(GpuDevice&,std::shared_ptr<const Scene3DTexture>,GpuTextureId&,String&);
    bool EnsureDecorations(GpuDevice&,Size,String&);
    bool EnsureShaders(GpuDevice&,String&);
    bool EnsureSplats(GpuDevice&,const Scene3DObject&,const Scene3DVec3&,String&);
    bool EnsureSplatPipeline(GpuDevice&,GpuFormat,GpuPipelineId&,String&);
    void PruneSplats(GpuDevice&,const std::shared_ptr<const Scene3DSnapshot>&);
    void CloseSplats(GpuDevice&);
    bool EnsurePipeline(GpuDevice&,GpuFormat,int,bool,bool,bool,GpuPipelineId&,String&);
    bool EnsureDepth(GpuDevice&,Size,String&);
    bool EnsureTarget(GpuDevice&,const GpuFrameInfo&,GpuFrameInfo&,String&);
    bool Resolve(GpuDevice&,const GpuFrameInfo&,String&);
    bool DrawGate(GpuDevice&,const GpuFrameInfo&,String&);
    MeshParameters Parameters(const Scene3DMatrix&,const Scene3DMatrix&,const Scene3DMatrix&,
                              float,float,float,float,float,const Scene3DMaterial&,bool) const;
};
} }
#endif
