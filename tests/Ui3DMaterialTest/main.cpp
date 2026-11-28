// Native production-renderer pixel qualification. The targets are owned Vulkan
// textures, not a claim about the separate window-system presentation path.
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <mmsystem.h> // Native timer declarations used by the installed U++ profiling API.
#include <Ui3DView/Ui3DViewContent.h>
#include <RenderVulkan/RenderVulkanRhi.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include <plugin/png/png.h>
#include <algorithm>
#include <cmath>
#include <chrono>

using namespace Upp;

namespace {
int checks=0,failures=0,pixel_checks=0,rendered=0;
int gaussian_pixel_checks=0;
ValueArray gaussian_measurements;
ValueArray gaussian_precision;
String output_folder;

bool Check(bool okay,const String& message)
{
    ++checks;
    if(!okay) { ++failures; Cout()<<"FAIL: "<<message<<EOL; }
    return okay;
}
int Encode(double linear)
{
    linear=std::max(0.0,std::min(1.0,linear));
    const double encoded=linear<=.0031308?12.92*linear:1.055*std::pow(linear,1.0/2.4)-.055;
    return (int)std::lround(encoded*255);
}
double Decode(int encoded)
{
    const double value=encoded/255.0;
    return value<=.04045?value/12.92:std::pow((value+.055)/1.055,2.4);
}

std::shared_ptr<const Scene3DTexture> Texture(int r,int g,int b,int a,
    Scene3DTextureColorSpace space=Scene3DTextureColorSpace::Srgb)
{
    String error;
    auto texture=Scene3DTexture::Create(Size(1,1),{(byte)r,(byte)g,(byte)b,(byte)a},space,error);
    Check(bool(texture),"create declared immutable texture: "+error);
    return texture;
}
std::shared_ptr<const Scene3DTexture> Corners()
{
    String error;
    auto texture=Scene3DTexture::Create(Size(2,2),
        {255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255},
        Scene3DTextureColorSpace::Srgb,error);
    Check(bool(texture),"create top-down corner texture: "+error);
    return texture;
}
std::shared_ptr<const Scene3DMesh> Quad(float alpha=1,float uv_offset=0,bool reverse=false)
{
    std::vector<Scene3DVertex> vertices(4);
    const Scene3DVec3 positions[]={{-.9,-.9,0},{.9,-.9,0},{.9,.9,0},{-.9,.9,0}};
    const Pointf uvs[]={{0,0},{1,0},{1,1},{0,1}};
    for(int i=0;i<4;++i) {
        vertices[i].position=positions[i]; vertices[i].normal={0,0,1};
        vertices[i].u=(float)uvs[i].x+uv_offset;
        vertices[i].v=(float)uvs[i].y+uv_offset;
        vertices[i].a=alpha;
    }
    String error;
    const std::vector<uint32> front={0,1,2,0,2,3},back={0,2,1,0,3,2};
    auto mesh=Scene3DMesh::Create(vertices,reverse?back:front,error);
    Check(bool(mesh),"create real indexed quad: "+error);
    return mesh;
}
Scene3DObjectDesc Object(const String& id,std::shared_ptr<const Scene3DMesh> mesh,
                       std::shared_ptr<const Scene3DTexture> texture={})
{
    Scene3DObjectDesc object; object.id=id; object.mesh=mesh;
    object.material.color_texture=texture; object.material.unlit=true;
    return object;
}

class Fixture {
public:
    explicit Fixture(VulkanGpuDevice& device):device(device),state(std::make_shared<Ui3DViewState>())
    {
        state->options.show_grid=state->options.show_axes=state->options.show_selection=false;
        state->options.lighting=false;
        state->options.background=Rgba8(0,0,255,255);
        state->camera.orthographic=true; state->camera.ortho_height=2;
        String error;
        Check(state->camera.SetLookAt({0,0,5},{0,0,0},{0,1,0},error),"set actual camera: "+error);
        content=CreateUi3DViewContent(state);
    }
    ~Fixture()
    {
        content->Close(device);
        for(auto target:targets) device.DestroyTexture(target);
    }
    VulkanGpuDevice& device;
    std::shared_ptr<Ui3DViewState> state;
    std::unique_ptr<GpuSurfaceContent> content;
    Vector<GpuTextureId> targets;
    Vector<byte> pixels;
    GpuFormat current_format=GpuFormat::Unknown;
    GpuTextureId current_target;
    uint64 revision=0;
    Size size=Size(128,128);

    bool SetScene(const std::vector<Scene3DObjectDesc>& objects)
    {
        Scene3DSceneDesc desc; desc.revision=++revision; desc.objects=objects;
        String error; auto scene=Scene3DSnapshot::Create(desc,error);
        if(!Check(bool(scene),"publish immutable material scene: "+error)) return false;
        state->scene=std::move(scene); return true;
    }
    bool Render(GpuFormat format=GpuFormat::RGBA8Srgb,const String& proof=String())
    {
        if(format!=current_format) {
            GpuTextureDesc desc; desc.size=size; desc.format=format;
            desc.usage=GpuTextureUsage_ColorAttachment|GpuTextureUsage_TransferSrc;
            desc.label="Ui3DMaterialTest bounded owned target";
            GpuTextureId target;
            if(!Check(device.CreateTexture(desc,target)==GpuResult::Ok,
                      "create bounded material target: "+device.GetLastError())) return false;
            targets.Add(target); current_target=target; current_format=format;
        }
        GpuFrameInfo frame; frame.color_target=current_target; frame.size=size; frame.color_format=format;
        String error;
        if(!Check(content->Render(device,frame,error),"production view renderer: "+error)) return false;
        ++rendered;
        pixels.Clear();
        if(!Check(device.ReadTexturePixels(current_target,pixels) &&
                  pixels.GetCount()==size.cx*size.cy*4,"read exact owned target pixels")) return false;
        if(!proof.IsEmpty() && !output_folder.IsEmpty()) SaveProof(proof);
        return true;
    }
    bool SubmitOnly()
    {
        if(!Check(current_target.IsValid(),"steady Gaussian target is already allocated")) return false;
        GpuFrameInfo frame; frame.color_target=current_target; frame.size=size; frame.color_format=current_format;
        String error;
        if(!Check(content->Render(device,frame,error),"native steady Gaussian submission: "+error)) return false;
        ++rendered;
        return true;
    }
    Rgba8 Pixel(int x,int y) const
    {
        if(x<0 || y<0 || x>=size.cx || y>=size.cy || pixels.GetCount()!=size.cx*size.cy*4)
            return Rgba8(0,0,0,0);
        const int p=4*(y*size.cx+x);
        const bool bgra=current_format==GpuFormat::BGRA8 || current_format==GpuFormat::BGRA8Srgb;
        return Rgba8(pixels[p+(bgra?2:0)],pixels[p+1],pixels[p+(bgra?0:2)],pixels[p+3]);
    }
    bool PixelIs(int x,int y,int r,int g,int b,const String& message,int tolerance=2)
    {
        ++pixel_checks; const auto pixel=Pixel(x,y);
        const bool okay=std::abs((int)pixel.r-r)<=tolerance &&
                        std::abs((int)pixel.g-g)<=tolerance &&
                        std::abs((int)pixel.b-b)<=tolerance && pixel.a>=253;
        if(!okay) Cout()<<"  pixel("<<x<<","<<y<<")="<<(int)pixel.r<<","<<(int)pixel.g<<","<<(int)pixel.b
                        <<","<<(int)pixel.a<<" expected "<<r<<","<<g<<","<<b<<EOL;
        return Check(okay,message);
    }
    bool Pick(int x,int y,const String& expected)
    {
        Scene3DRay ray;
        if(!Check(state->camera.WorldRay(Pointf(x+.5,y+.5),size,ray),"exact pixel-centre camera ray")) return false;
        const auto hit=state->scene->Pick(ray,state->camera.revision);
        return Check(expected.IsEmpty()?!hit.hit:hit.hit && hit.id==expected,
                     "CPU material pick matches declared visible coverage");
    }
    void SaveProof(const String& name)
    {
        ImageBuffer image(size);
        for(int y=0;y<size.cy;++y) for(int x=0;x<size.cx;++x) {
            const auto p=Pixel(x,y); auto& out=image[y][x];
            out.r=p.r; out.g=p.g; out.b=p.b; out.a=p.a;
        }
        Check(PNGEncoder().SaveFile(AppendFileName(output_folder,name+".png"),Image(image)),
              "save bounded owned-target proof");
    }
};

// A repeating two-texel image blends across the seam. These expectations come
// from the pixel-centre ray through the fixed [-.9,.9] orthographic quad, then
// the standard bilinear four-texel weights in linear color space.
Rgba8 RepeatCornerAt(int x,int y)
{
    const double u=((x+.5)/128.0*2.0-.1)/1.8;
    const double v=((y+.5)/128.0*2.0-.1)/1.8; // Top-down source row.
    const double tx=u*2-.5,ty=v*2-.5;
    const int ix=(int)std::floor(tx),iy=(int)std::floor(ty);
    const double fx=tx-ix,fy=ty-iy;
    const int colors[4][3]={{1,0,0},{0,1,0},{0,0,1},{1,1,1}};
    double channels[3]={0,0,0};
    for(int dy=0;dy<2;++dy) for(int dx=0;dx<2;++dx) {
        const int sx=((ix+dx)%2+2)%2,sy=((iy+dy)%2+2)%2;
        const double weight=(dx?fx:1-fx)*(dy?fy:1-fy);
        for(int c=0;c<3;++c) channels[c]+=weight*colors[sy*2+sx][c];
    }
    return Rgba8(Encode(channels[0]),Encode(channels[1]),Encode(channels[2]),255);
}

void TestTextureAndColor(Fixture& fixture)
{
    const auto mesh=Quad();
    const auto corners=Corners();
    Check(fixture.SetScene({Object("corners",mesh,corners)}),"corner scene accepted");
    const GpuFormat formats[]={GpuFormat::RGBA8,GpuFormat::BGRA8,GpuFormat::RGBA8Srgb,GpuFormat::BGRA8Srgb};
    for(int i=0;i<4;++i) if(fixture.Render(formats[i],Format("corners-%d",i))) {
        fixture.PixelIs(20,20,255,0,0,"top-left top-down texture is red");
        fixture.PixelIs(108,20,0,255,0,"top-right top-down texture is green");
        fixture.PixelIs(20,108,0,0,255,"bottom-left V-up texture is blue");
        fixture.PixelIs(108,108,255,255,255,"bottom-right texture is white");
    }
    auto object=Object("srgb",mesh,Texture(128,128,128,255)); object.r=object.g=object.b=.5f;
    fixture.SetScene({object});
    for(auto format:formats) if(fixture.Render(format)) {
        const int expected=Encode(Decode(128)*.5);
        fixture.PixelIs(64,64,expected,expected,expected,"sRGB texture decodes before linear material factor");
    }
    object.material.color_texture=Texture(128,128,128,255,Scene3DTextureColorSpace::Linear);
    fixture.SetScene({object});
    if(fixture.Render()) {
        const int expected=Encode((128.0/255)*.5);
        fixture.PixelIs(64,64,expected,expected,expected,"linear texture is not sRGB-decoded");
    }
    object=Object("repeat",Quad(1,1),corners); object.material.repeat_uv=true;
    fixture.SetScene({object});
    if(fixture.Render()) {
        const auto top_left=RepeatCornerAt(20,20),bottom_right=RepeatCornerAt(108,108);
        fixture.PixelIs(20,20,top_left.r,top_left.g,top_left.b,"repeat blends translated U/V across the top-left seam");
        fixture.PixelIs(108,108,bottom_right.r,bottom_right.g,bottom_right.b,"repeat blends translated U/V across the bottom-right seam");
    }
    object.material.repeat_uv=false; fixture.SetScene({object});
    if(fixture.Render()) fixture.PixelIs(20,108,0,255,0,"clamped U/V samples upper-right source edge");
}

void TestCoverage(Fixture& fixture)
{
    const auto mesh=Quad(0);
    const auto red_zero=Texture(255,0,0,0);
    auto object=Object("opaque",mesh,red_zero); object.a=0; object.opacity=.5f;
    fixture.SetScene({object});
    for(auto format:{GpuFormat::RGBA8,GpuFormat::RGBA8Srgb}) if(fixture.Render(format)) {
        fixture.PixelIs(64,64,Encode(.5),0,Encode(.5),"opaque ignores all material alpha but respects object fade");
        fixture.Pick(64,64,"opaque");
    }
    object.opacity=0; fixture.SetScene({object});
    if(fixture.Render()) { fixture.PixelIs(64,64,0,0,255,"zero object opacity leaves background"); fixture.Pick(64,64,""); }
    object=Object("mask",Quad(),Texture(255,0,0,127)); object.material.alpha_mode=Scene3DAlphaMode::Mask;
    fixture.SetScene({object});
    if(fixture.Render()) { fixture.PixelIs(64,64,0,0,255,"mask below cutoff discards"); fixture.Pick(64,64,""); }
    object.material.color_texture=Texture(255,0,0,128); fixture.SetScene({object});
    if(fixture.Render()) { fixture.PixelIs(64,64,255,0,0,"mask above cutoff resolves full coverage"); fixture.Pick(64,64,"mask"); }
    object.opacity=.5f; fixture.SetScene({object});
    if(fixture.Render()) fixture.PixelIs(64,64,Encode(.5),0,Encode(.5),"mask coverage resolves before object fade");
    object.material.color_texture=red_zero; object.material.alpha_cutoff=0; object.opacity=1;
    fixture.SetScene({object});
    if(fixture.Render()) { fixture.PixelIs(64,64,255,0,0,"zero mask cutoff includes zero material alpha"); fixture.Pick(64,64,"mask"); }

    object=Object("blend",Quad(.5f),Texture(255,0,0,128));
    object.material.alpha_mode=Scene3DAlphaMode::Blend; object.a=.5f; object.opacity=.5f;
    fixture.SetScene({object});
    if(fixture.Render(GpuFormat::BGRA8,"blend-linear")) {
        const double coverage=(128.0/255)*.5*.5*.5;
        fixture.PixelIs(64,64,Encode(coverage),0,Encode(1-coverage),"blend multiplies texture vertex base and independent opacity once");
        fixture.Pick(64,64,"blend");
    }
    object.material.color_texture=red_zero; fixture.SetScene({object});
    if(fixture.Render()) { fixture.PixelIs(64,64,0,0,255,"zero blended coverage leaves depth and color untouched"); fixture.Pick(64,64,""); }

    // Spatial cutouts must not write depth: the red mask's left half is clear.
    String error;
    auto holes=Scene3DTexture::Create(Size(2,1),{255,0,0,0,255,0,0,255},Scene3DTextureColorSpace::Srgb,error);
    Check(bool(holes),"create actual cutout pixels: "+error);
    auto mask=Object("cutout",Quad(),holes); mask.material.alpha_mode=Scene3DAlphaMode::Mask;
    mask.local=Scene3DMatrix::Translation({0,0,.5});
    auto behind=Object("behind",Quad(),Texture(0,255,0,255));
    fixture.SetScene({mask,behind});
    if(fixture.Render(GpuFormat::RGBA8Srgb,"cutout-depth")) {
        fixture.PixelIs(20,64,0,255,0,"mask hole retains rear geometry depth");
        fixture.PixelIs(108,64,255,0,0,"mask solid occludes rear geometry");
        fixture.Pick(20,64,"behind"); fixture.Pick(108,64,"cutout");
    }
}
void TestOrderingAndCull(Fixture& fixture)
{
    const auto mesh=Quad();
    auto front_object=Object("front_object",mesh,Texture(255,0,0,255));
    auto rear_object=Object("rear_object",mesh,Texture(0,255,0,255));
    front_object.material.alpha_mode=rear_object.material.alpha_mode=Scene3DAlphaMode::Blend;
    front_object.opacity=rear_object.opacity=.5f; front_object.local=Scene3DMatrix::Translation({0,0,.5});
    for(bool reverse:{false,true}) {
        fixture.SetScene(reverse?std::vector<Scene3DObjectDesc>{rear_object,front_object}:std::vector<Scene3DObjectDesc>{front_object,rear_object});
        if(fixture.Render()) fixture.PixelIs(64,64,Encode(.5),Encode(.25),Encode(.25),
                                            "transparent surfaces sort far to near independently of descriptor order");
    }
    auto opaque=Object("opaque",mesh,Texture(255,255,0,255));
    opaque.local=Scene3DMatrix::Translation({0,0,.75});
    fixture.SetScene({front_object,opaque,rear_object});
    if(fixture.Render()) fixture.PixelIs(64,64,255,255,0,"transparent surfaces obey opaque depth");

    auto single=Object("single",mesh,Texture(255,0,0,255)); single.material.double_sided=false;
    fixture.SetScene({single});
    if(fixture.Render()) { fixture.PixelIs(64,64,255,0,0,"CCW front face remains visible"); fixture.Pick(64,64,"single"); }
    single.mesh=Quad(1,0,true); fixture.SetScene({single});
    if(fixture.Render()) { fixture.PixelIs(64,64,0,0,255,"single-sided back face is culled"); fixture.Pick(64,64,""); }
    single.material.double_sided=true; fixture.SetScene({single});
    if(fixture.Render()) { fixture.PixelIs(64,64,255,0,0,"double-sided back face remains visible"); fixture.Pick(64,64,"single"); }
}
void TestCameraAndGuides(Fixture& fixture)
{
    fixture.SetScene({Object("rolled",Quad(),Corners())});
    fixture.state->camera.roll=1.57079632679489661923;
    ++fixture.state->camera.revision;
    if(fixture.Render(GpuFormat::RGBA8Srgb,"authored-roll")) {
        fixture.PixelIs(20,20,0,255,0,"camera roll maps original upper-right to upper-left");
        fixture.PixelIs(108,20,255,255,255,"camera roll maps lower-right to upper-right");
        fixture.PixelIs(20,108,255,0,0,"camera roll maps upper-left to lower-left");
        fixture.PixelIs(108,108,0,0,255,"camera roll maps lower-left to lower-right");
    }
    fixture.state->camera.roll=0; fixture.state->camera.gate_aspect=2;
    ++fixture.state->camera.revision;
    fixture.SetScene({});
    auto& options=fixture.state->options; options.show_gate=true;
    options.gate_color=Rgba8(255,0,0,255); options.safe_color=Rgba8(0,255,0,255);
    Ui3DViewGateLayout layout;
    Check(ComputeUi3DViewGateLayout(fixture.state->camera,options,fixture.size,layout) &&
          layout.gate==Rect(0,32,128,96),"wide authored film gate fits centred square viewport");
    if(fixture.Render()) {
        fixture.PixelIs(64,10,0,0,0,"film matte covers viewport overscan");
        fixture.PixelIs(64,64,0,0,255,"film matte preserves authored gate interior");
        fixture.PixelIs(64,32,255,0,0,"film guide uses actual gate boundary");
    }
    options.show_safe=true;
    if(fixture.Render(GpuFormat::RGBA8,"film-safe-guides")) {
        fixture.PixelIs(64,layout.action_safe.top,0,255,0,"action-safe guide at physical gate fraction");
        fixture.PixelIs(64,layout.title_safe.top,0,255,0,"title-safe guide at physical gate fraction");
    }
    options.show_safe=options.show_gate=false; fixture.state->camera.gate_aspect=0;
    ++fixture.state->camera.revision;
}
void TestRetention(Fixture& fixture)
{
    fixture.SetScene({Object("retained",Quad(),Corners())});
    if(!fixture.Render()) return;
    const auto before=fixture.state->stats;
    const auto bytes=fixture.device.GetLiveAllocationBytes();
    for(int i=0;i<5;++i) if(fixture.Render()) fixture.PixelIs(20,20,255,0,0,"retained frame preserves true texture pixels");
    const auto& after=fixture.state->stats;
    Check(after.mesh_uploads==before.mesh_uploads && after.texture_uploads==before.texture_uploads &&
          after.depth_rebuilds==before.depth_rebuilds && after.working_target_rebuilds==before.working_target_rebuilds,
          "unchanged scene and target do not upload or allocate again");
    Check(fixture.device.GetLiveAllocationBytes()==bytes && fixture.device.GetLiveCommandCount()==0,
          "steady-state render releases every command and descriptor allocation");
    Check(after.resident_textures==1 && after.resident_texture_bytes==16,"immutable texture accounting is explicit");
    fixture.SetScene({});
    if(fixture.Render()) Check(fixture.state->stats.resident_textures==0 &&
        fixture.state->stats.resident_texture_bytes==0 && fixture.state->stats.resident_meshes==0,
        "clearing scene releases unused asset geometry and texture resources");
}
void TestGaussianBlendPrecision(Fixture& fixture)
{
    // Distinguish shader geometry/interpolation from the native attachment's
    // blend precision using a uniform indexed mesh at the same analytical alpha.
    const double xx=7.04936,xy=-1.0944512,yy=1.93820736;
    const double near_peak=std::exp(-.5*(yy*.01-2*xy*.006+xx*.0036)/(xx*yy-xy*xy));
    int index=0;
    for(double alpha:{std::exp(-5.0),near_peak}) {
        auto object=Object("blend:precision-reference",Quad(),Texture(255,0,0,255));
        object.opacity=(float)alpha;
        fixture.SetScene({object});
        if(fixture.Render(GpuFormat::RGBA8Srgb,Format("uniform-blend-precision-%d",index))) {
            const auto pixel=fixture.Pixel(64,64);
            const double quantized=std::round(alpha*255)/255;
            ValueMap sample; sample.Add("linear_alpha",alpha);
            sample.Add("red",(int)pixel.r); sample.Add("green",(int)pixel.g); sample.Add("blue",(int)pixel.b);
            sample.Add("ideal_red",Encode(alpha)); sample.Add("ideal_blue",Encode(1-alpha));
            sample.Add("unorm8_factor_red",Encode(quantized)); sample.Add("unorm8_factor_blue",Encode(1-quantized));
            sample.Add("attachment","Actual RGBA8 sRGB color attachment; no intermediate linear UNORM target.");
            gaussian_precision.Add(sample);
            Cout()<<"Uniform blend precision: "<<AsJSON(sample)<<EOL;
        }
        ++index;
    }
}
std::shared_ptr<const Scene3DSplats> GaussianCloud(const std::vector<Scene3DSplat>& points)
{
    String error;
    auto source=Scene3DSplats::Create(points,error);
    Check(bool(source),"create immutable native Gaussian resource: "+error);
    return source;
}
Scene3DObjectDesc GaussianObject(const String& id,std::shared_ptr<const Scene3DSplats> source)
{
    Scene3DObjectDesc object; object.id=id; object.splats=std::move(source);
    return object;
}
Scene3DSplat RedGaussian()
{
    Scene3DSplat point;
    point.covariance[0]=.04; point.covariance[1]=.012; point.covariance[2]=0;
    point.covariance[3]=.01; point.covariance[4]=0; point.covariance[5]=.01;
    point.r=1; point.g=point.b=0; point.a=1;
    return point;
}
void GaussianPixel(Fixture& fixture,int x,int y,double cx,double cy,
                   double xx,double xy,double yy,double peak,double red,double green,double blue,
                   const String& message)
{
    // Independent analytic oracle: each covariance below is derived from a
    // known camera in pixels/metre, not Scene3DProjectSplat or renderer state.
    const double dx=x+.5-cx,dy=y+.5-cy;
    const double q=(yy*dx*dx-2*xy*dx*dy+xx*dy*dy)/(xx*yy-xy*xy);
    double alpha=peak*std::exp(-.5*q);
    if(q>2*std::log(255.0) || alpha<1.0/255) alpha=0;
    ++gaussian_pixel_checks; ++pixel_checks;
    const auto pixel=fixture.Pixel(x,y);
    auto encoded=[&](double factor) {
        return Rgba8(Encode(factor*red),Encode(factor*green),Encode(factor*blue+1-factor),255);
    };
    const auto ideal=encoded(alpha);
    // Vulkan basic blending permits destination-component precision:
    // https://docs.vulkan.org/spec/latest/chapters/framebuffer.html#framebuffer-blending
    // The retained uniform-mesh discriminator observes the same low-alpha
    // result on this device. Keep the two-code-value tolerance unchanged;
    // explicitly evaluate either full precision or an 8-bit blend factor.
    const auto fixed_factor=encoded(std::round(alpha*255)/255);
    auto matches=[&](Rgba8 expected) {
        return std::abs(int(pixel.r)-int(expected.r))<=2 &&
               std::abs(int(pixel.g)-int(expected.g))<=2 &&
               std::abs(int(pixel.b)-int(expected.b))<=2 && pixel.a>=253;
    };
    const bool okay=matches(ideal) || matches(fixed_factor);
    if(!okay) Cout()<<"  Gaussian pixel("<<x<<","<<y<<")="<<(int)pixel.r<<","<<(int)pixel.g<<","<<(int)pixel.b
                    <<" ideal="<<(int)ideal.r<<","<<(int)ideal.g<<","<<(int)ideal.b
                    <<" factor8="<<(int)fixed_factor.r<<","<<(int)fixed_factor.g<<","<<(int)fixed_factor.b<<EOL;
    Check(okay,message);
}
bool SamePixels(const Vector<byte>& a,const Vector<byte>& b)
{
    if(a.GetCount()!=b.GetCount()) return false;
    for(int i=0;i<a.GetCount();++i) if(a[i]!=b[i]) return false;
    return true;
}
void TestGaussianPixels(Fixture& fixture)
{
    auto& camera=fixture.state->camera;
    camera.orthographic=true; camera.ortho_height=2; camera.roll=0; camera.gate_aspect=0;
    camera.fov_y=1.57079632679489661923; camera.near_plane=.1; camera.far_plane=100;
    String error; Check(camera.SetLookAt({0,0,5},{0,0,0},{0,1,0},error),"set analytical Gaussian camera");
    auto point=RedGaussian(); point.g=.2f; point.a=.8f;
    auto object=GaussianObject("density:oblique",GaussianCloud({point}));
    object.r=.5f; object.a=.75f; object.opacity=.5f;
    fixture.SetScene({object});
    for(auto format:{GpuFormat::RGBA8,GpuFormat::BGRA8,GpuFormat::RGBA8Srgb,GpuFormat::BGRA8Srgb})
        if(fixture.Render(format,Format("gaussian-oblique-%d",(int)format))) {
            for(const Point p:{Point(64,64),Point(76,60),Point(52,68),Point(76,76),Point(91,91)})
                GaussianPixel(fixture,p.x,p.y,64,64,164.09,-49.152,41.21,.3,.5,.2,0,
                              "Gaussian oblique density and independent linear RGBA match pixels");
            fixture.PixelIs(91,91,0,0,255,"Gaussian quad corners do not substitute a visible rectangular surface");
        }
    auto parent=Object("density:parent",{});
    parent.local=Scene3DMatrix::Translation({.25,-.125,0})*Scene3DMatrix::Scale({2,3,1});
    object.parent_id=parent.id;
    fixture.SetScene({object,parent});
    if(fixture.Render(GpuFormat::RGBA8Srgb,"gaussian-parent-scale")) {
        for(const Point p:{Point(80,72),Point(96,63),Point(61,84),Point(102,96)})
            GaussianPixel(fixture,p.x,p.y,80,72,655.61,-294.912,368.89,.3,.5,.2,0,
                          "Gaussian parent translation and nonuniform covariance are applied once");
    }
    camera.roll=1.57079632679489661923; ++camera.revision;
    if(fixture.Render(GpuFormat::RGBA8Srgb,"gaussian-parent-roll")) {
        for(const Point p:{Point(72,48),Point(82,64),Point(58,31),Point(95,29)})
            GaussianPixel(fixture,p.x,p.y,72,48,368.89,294.912,655.61,.3,.5,.2,0,
                          "Gaussian actual rolled view rotates centre and covariance together");
    }
    camera.roll=0; ++camera.revision; object.parent_id.Clear();
    point=RedGaussian(); point.position={.5,.2,0};
    point.covariance[1]=.006; point.covariance[2]=.003; point.covariance[4]=.002; point.covariance[5]=.09;
    object=GaussianObject("density:perspective",GaussianCloud({point}));
    camera.orthographic=false; ++camera.revision;
    fixture.SetScene({object});
    // At 128 square, ninety-degree perspective: jx=(12.8,0,1.28),
    // jy=(0,-12.8,-.512). Off-axis depth covariance contributes to all entries.
    if(fixture.Render(GpuFormat::RGBA8Srgb,"gaussian-perspective")) {
        for(const Point p:{Point(70,61),Point(72,60),Point(68,62),Point(75,66)})
            GaussianPixel(fixture,p.x,p.y,70.4,61.44,7.04936,-1.0944512,1.93820736,1,1,0,0,
                          "Gaussian perspective pixels include XZ YZ and depth covariance");
    }
    camera.orthographic=true; ++camera.revision;
    point=RedGaussian(); for(double& covariance:point.covariance) covariance=0;
    fixture.SetScene({GaussianObject("density:zero-covariance",GaussianCloud({point}))});
    if(fixture.Render(GpuFormat::RGBA8Srgb,"gaussian-zero-covariance"))
        for(const Point p:{Point(64,64),Point(65,64),Point(66,64)})
            GaussianPixel(fixture,p.x,p.y,64,64,.25,0,.25,1,1,0,0,
                          "zero source covariance uses the actual quarter-pixel filter");
    point=RedGaussian(); point.covariance[0]=point.covariance[1]=point.covariance[3]=.04;
    point.covariance[2]=point.covariance[4]=point.covariance[5]=0;
    fixture.SetScene({GaussianObject("density:rank-one",GaussianCloud({point}))});
    if(fixture.Render(GpuFormat::RGBA8Srgb,"gaussian-rank-one"))
        for(const Point p:{Point(64,64),Point(68,60),Point(68,68)})
            GaussianPixel(fixture,p.x,p.y,64,64,164.09,-163.84,164.09,1,1,0,0,
                          "rank-one Gaussian keeps its oblique density and filtered minor axis");
    point=RedGaussian(); point.covariance[0]=point.covariance[3]=point.covariance[5]=1e18;
    point.covariance[1]=point.covariance[2]=point.covariance[4]=0;
    fixture.SetScene({GaussianObject("density:large-admitted",GaussianCloud({point}))});
    if(fixture.Render(GpuFormat::RGBA8Srgb,"gaussian-large-covariance")) {
        fixture.PixelIs(64,64,255,0,0,"largest admitted isotropic covariance remains visible at its centre");
        fixture.PixelIs(5,5,255,0,0,"largest admitted covariance retains finite screen density");
    }
}
void TestGaussianDepthAndPicking(Fixture& fixture)
{
    String error; auto& camera=fixture.state->camera;
    camera.orthographic=true; camera.ortho_height=2; camera.roll=0; camera.gate_aspect=0;
    camera.SetLookAt({0,0,5},{0,0,0},{0,1,0},error);
    auto rear=RedGaussian(),front_point=RedGaussian();
    rear.covariance[1]=front_point.covariance[1]=0;
    rear.covariance[3]=front_point.covariance[3]=.04;
    rear.position.z=-.5; rear.r=0; rear.g=1; rear.a=.5f;
    front_point.a=.5f;
    Vector<byte> sorted_pixels;
    for(bool reverse:{false,true}) {
        auto object=GaussianObject("density:sorted",GaussianCloud(reverse?
            std::vector<Scene3DSplat>{front_point,rear}:std::vector<Scene3DSplat>{rear,front_point}));
        fixture.SetScene({object});
        if(fixture.Render(GpuFormat::RGBA8Srgb,reverse?"gaussian-order-reversed":"gaussian-order-forward")) {
            const double alpha=.5*std::exp(-.5*(.5/164.09));
            fixture.PixelIs(64,64,Encode(alpha),Encode(alpha*(1-alpha)),Encode((1-alpha)*(1-alpha)),
                            "Gaussian points sort far-to-near inside one cloud");
            if(!reverse) sorted_pixels=clone(fixture.pixels);
            else Check(SamePixels(sorted_pixels,fixture.pixels),"reversing source point order preserves every differently-depth-sorted pixel");
            const auto hit=fixture.state->scene->PickScreen(Pointf(64.5,64.5),fixture.size,camera);
            Check(hit.hit && hit.id=="density:sorted" && hit.splat==(reverse?0:1),
                  "Gaussian picking returns the original nearest point index after GPU sorting");
        }
    }
    auto object=GaussianObject("density:occluded",GaussianCloud({front_point}));
    auto opaque=Object("opaque:front",Quad(),Texture(255,255,0,255));
    opaque.local=Scene3DMatrix::Translation({0,0,1});
    fixture.SetScene({object,opaque});
    if(fixture.Render(GpuFormat::RGBA8Srgb,"gaussian-opaque-depth")) {
        fixture.PixelIs(64,64,255,255,0,"opaque mesh depth occludes Gaussian fragments");
        Check(fixture.state->scene->PickScreen(Pointf(64.5,64.5),fixture.size,camera).id=="opaque:front",
              "Gaussian screen picking agrees with actual opaque depth pixels");
    }
    object.opacity=0; fixture.SetScene({object});
    if(fixture.Render()) fixture.PixelIs(64,64,0,0,255,"zero object fade removes Gaussian color and allocation demand");
    front_point=RedGaussian(); front_point.position={0,1.25,0}; front_point.covariance[1]=0;
    front_point.covariance[0]=front_point.covariance[3]=.01;
    object=GaussianObject("density:overscan",GaussianCloud({front_point}));
    fixture.SetScene({object});
    camera.gate_aspect=2; ++camera.revision;
    auto& options=fixture.state->options; options.show_gate=true; options.matte_opacity=1;
    if(fixture.Render(GpuFormat::RGBA8Srgb,"gaussian-film-matte")) {
        fixture.PixelIs(64,24,0,0,0,"physical film matte hides the actual Gaussian overscan");
        fixture.PixelIs(64,64,0,0,255,"film gate interior remains unaffected by an outside density");
    }
    Ui3DView view; view.SetRect(0,0,128,128);
    Check(view.SetScene(fixture.state->scene,error) && view.SetCamera(camera,error) &&
          view.SetOptions(options,error),"bind actual reusable view's Gaussian picking state");
    Check(!view.Pick(Pointf(64,24)).hit,"reusable native view rejects a Gaussian hidden by opaque matte");
    options.show_gate=false; Check(view.SetOptions(options,error),"remove view-only film matte");
    const auto hit=view.Pick(Pointf(64,24));
    Check(hit.hit && hit.splat==0 && view.IsCurrentPick(hit),"view-only matte removal exposes the exact Gaussian source identity");
    ++camera.revision; camera.roll=.1;
    Check(view.SetCamera(camera,error) && !view.IsCurrentPick(hit),"changed camera invalidates prior Gaussian view picks");
    camera.roll=0; camera.gate_aspect=0; ++camera.revision;
}
void TestGlobalGaussianOrder(Fixture& fixture)
{
    auto& camera=fixture.state->camera;
    camera.orthographic=true; camera.ortho_height=2; camera.roll=0; camera.gate_aspect=0;
    String error;
    Check(camera.SetLookAt({0,0,5},{0,0,0},{0,1,0},error),"set global Gaussian test camera");
    auto rear_point=RedGaussian(), front_point=rear_point, middle=rear_point;
    for(auto* point : {&rear_point,&front_point,&middle}) {
        point->covariance[1]=0;
        point->covariance[3]=.04;
        point->a=.5f;
    }
    rear_point.position.z=-.75; front_point.position.z=.75;
    front_point.r=.1f; front_point.b=.7f;
    middle.r=0; middle.g=1;
    auto glass=Object("mesh:glass",Quad(),Texture(225,184,70,100));
    glass.material.alpha_mode=Scene3DAlphaMode::Blend;
    glass.local=Scene3DMatrix::Translation({0,0,.25});

    const auto translate=Scene3DMatrix::Translation({0,0,.125});
    auto reference_a=GaussianObject("test:rear_point",GaussianCloud({rear_point}));
    auto reference_b=GaussianObject("test:front_point",GaussianCloud({front_point}));
    reference_a.local=reference_b.local=translate;
    auto other=GaussianObject("test:middle",GaussianCloud({middle}));
    fixture.state->options.gpu_splat_sort=false;
    if(!fixture.SetScene({reference_a,glass,other,reference_b}) || !fixture.Render()) return;
    const Vector<byte> reference=clone(fixture.pixels);
    fixture.state->options.gpu_splat_sort=true;
    for(bool large : {false,true}) {
        std::vector<Scene3DSplat> red={front_point,rear_point};
        if(large) {
            auto off=rear_point;
            off.position={4,5,.25};
            off.covariance[0]=off.covariance[3]=off.covariance[5]=1e-6;
            red.insert(red.end(),300,off);
        }
        auto joined=GaussianObject("density:straddles another cloud and the mesh",GaussianCloud(red));
        joined.local=translate;
        std::vector<Scene3DObjectDesc> scene={joined,glass,other};
        if(!fixture.SetScene(scene) || !fixture.Render(GpuFormat::RGBA8Srgb,
                                                large?"gaussian-global-radix":"gaussian-global-small")) return;
        const auto stats=fixture.state->stats;
        Check(stats.global_splat_order && stats.resident_splat_resources==2 &&
              stats.resident_splat_instances==2 && stats.splats_last_frame==int(red.size()+1),
              "one GPU sequence spans two different Gaussian source buffers");
        Check(SamePixels(reference,fixture.pixels),
              "global points on both sides of another cloud and mesh match the analytic painter order");
        std::reverse(scene.begin(),scene.end());
        if(!fixture.SetScene(scene) || !fixture.Render()) return;
        Check(SamePixels(reference,fixture.pixels) &&
              fixture.state->stats.splat_sort_updates==stats.splat_sort_updates &&
              fixture.state->stats.splat_uploads==stats.splat_uploads,
              "reordering unchanged scene rows keeps global pixels, sort and source residency");
    }

    // An exact depth tie is resolved by stable instance ID and source ordinal.
    rear_point.position.z=front_point.position.z=0;
    auto tie1=GaussianObject("test:a",GaussianCloud({rear_point,front_point}));
    auto tie2=GaussianObject("test:z",GaussianCloud({middle}));
    auto tie_copy=tie1; tie_copy.id="test:b"; tie_copy.local=Scene3DMatrix::Translation({.7,0,0});
    if(!fixture.SetScene({tie2,tie1,tie_copy}) || !fixture.Render()) return;
    const auto tied=clone(fixture.pixels);
    if(!fixture.SetScene({tie_copy,tie1,tie2}) || !fixture.Render()) return;
    Check(SamePixels(tied,fixture.pixels) && fixture.state->stats.resident_splat_resources==2 &&
          fixture.state->stats.resident_splat_instances==3,
          "equal-depth order is deterministic and repeated instances share source rows");
    fixture.SetScene({});
    fixture.Render();
    Check(fixture.state->stats.resident_splat_bytes==0 &&
          fixture.device.GetSharedImmutableBufferBytes()==0,"clearing the scene releases global source and sort buffers");
}
void TestGaussianRetentionAndBudget(Fixture& fixture)
{
    // Preserve the exact retained-byte assertions for the compatibility implementation.
    fixture.state->options.gpu_splat_sort=false;
    auto& camera=fixture.state->camera; String error;
    camera.orthographic=true; camera.ortho_height=2; camera.roll=0; camera.gate_aspect=0;
    camera.SetLookAt({0,0,5},{0,0,0},{0,1,0},error);
    auto point=RedGaussian(); auto source=GaussianCloud({point});
    auto object=GaussianObject("density:retained",source);
    fixture.SetScene({object}); if(!fixture.Render()) return;
    auto baseline=fixture.state->stats;
    const uint64 allocations=fixture.device.GetLiveAllocationBytes();
    Check(baseline.resident_splat_resources==1 && baseline.resident_splat_instances==1 &&
          baseline.resident_splat_bytes==56,"one Gaussian retains a 52-byte source record and one 32-bit ordered reference");
    for(int i=0;i<3;++i) fixture.Render();
    Check(fixture.state->stats.splat_uploads==baseline.splat_uploads &&
          fixture.state->stats.splat_sort_updates==baseline.splat_sort_updates &&
          fixture.device.GetLiveAllocationBytes()==allocations &&
          fixture.device.GetLiveCommandCount()==0,"unchanged Gaussian frames neither upload sort nor leak transient commands");
    camera.target.x+=.2; ++camera.revision; fixture.Render();
    camera.fov_y=1.1; ++camera.revision; fixture.Render();
    camera.roll=.3; ++camera.revision; fixture.Render();
    object.local=Scene3DMatrix::Translation({.3,.1,0}); fixture.SetScene({object}); fixture.Render();
    Check(fixture.state->stats.splat_sort_updates==baseline.splat_sort_updates,
          "camera translation field of view roll and cloud translation do not re-sort local depth");
    camera.yaw+=.2; ++camera.revision; fixture.Render();
    Check(fixture.state->stats.splat_sort_updates==baseline.splat_sort_updates+1 &&
          fixture.state->stats.splat_uploads==baseline.splat_uploads,
          "changed view direction updates only retained Gaussian index order");
    auto second=object; second.id="density:second"; second.local=Scene3DMatrix::Translation({-.3,0,0});
    fixture.SetScene({object,second}); fixture.Render();
    Check(fixture.state->stats.resident_splat_resources==1 && fixture.state->stats.resident_splat_instances==2 &&
          fixture.state->stats.resident_splat_bytes==60,"shared Gaussian source uploads once with separate per-view instance order");
    object.visible=false; second.visible=false; fixture.SetScene({object,second}); fixture.Render();
    Check(fixture.state->stats.resident_splat_resources==0 && fixture.state->stats.resident_splat_instances==0 &&
          fixture.state->stats.resident_splat_bytes==0,"hidden Gaussian instances release GPU buffers while CPU owners remain alive");
    object.visible=true; fixture.SetScene({object}); fixture.Render();
    auto replacement=point; replacement.b=1;
    object.splats=GaussianCloud({replacement}); fixture.SetScene({object}); fixture.Render();
    Check(fixture.state->stats.resident_splat_resources==1 && fixture.state->stats.resident_splat_instances==1 &&
          fixture.state->stats.resident_splat_bytes==56,"replacing a retained source releases the superseded Gaussian allocation");
    fixture.SetScene({}); fixture.Render();
    Check(fixture.state->stats.resident_splat_bytes==0,"deleting Gaussian objects releases all retained cloud allocations");

    // Retain the former workload and qualify the expanded ceiling separately.
    // These are bounded measurements, not a promised interactive frame rate.
    for(const int point_count:{150000,Scene3DMaxSplats}) {
        camera.roll=0; camera.fov_y=1.57079632679489661923;
        camera.SetLookAt({0,0,5},{0,0,0},{0,1,0},error);
        const int columns=point_count==150000?400:1000;
        const int rows=(point_count+columns-1)/columns;
        std::vector<Scene3DSplat> points(point_count);
        for(int i=0;i<point_count;++i) {
            auto& p=points[i];
            p.position={((i%columns)+.5)/columns*1.8-.9,
                        ((i/columns)+.5)/rows*1.8-.9,0};
            p.covariance[0]=p.covariance[3]=p.covariance[5]=1e-6;
            p.r=.7f; p.g=.2f; p.b=.1f; p.a=.03f;
        }
        object=GaussianObject("density:bounded-"+AsString(point_count),GaussianCloud(points));
        const auto start=std::chrono::steady_clock::now();
        if(!fixture.SetScene({object})) return;
        const bool first=fixture.Render(GpuFormat::RGBA8Srgb,"gaussian-"+AsString(point_count));
        const double first_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        if(first) {
            const auto before=fixture.state->stats;
            Check(before.splats_last_frame==point_count && before.splat_draws_last_frame==1 &&
                  before.resident_splat_resources==1 && before.resident_splat_instances==1 &&
                  before.resident_splat_bytes==uint64(point_count)*56,
                  "bounded cloud uses actual Gaussian draws and exact retained buffer bytes");
            double maximum_ms=0; const auto steady_start=std::chrono::steady_clock::now();
            for(int i=0;i<12;++i) {
                const auto frame_start=std::chrono::steady_clock::now();
                fixture.SubmitOnly();
                maximum_ms=std::max(maximum_ms,std::chrono::duration<double,std::milli>(
                    std::chrono::steady_clock::now()-frame_start).count());
            }
            const double steady_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-steady_start).count();
            const auto after=fixture.state->stats;
            Check(after.splat_uploads==before.splat_uploads && after.splat_sort_updates==before.splat_sort_updates &&
                  after.resident_splat_bytes==before.resident_splat_bytes && fixture.device.GetLiveCommandCount()==0,
                  "bounded steady frames retain uploads order and ownership");

            camera.yaw+=.2; ++camera.revision;
            const auto rotation_start=std::chrono::steady_clock::now();
            if(!fixture.SubmitOnly()) return;
            const double rotation_ms=std::chrono::duration<double,std::milli>(
                std::chrono::steady_clock::now()-rotation_start).count();
            const auto rotated=fixture.state->stats;
            Check(rotated.splat_sort_updates==after.splat_sort_updates+1 &&
                  rotated.splat_uploads==after.splat_uploads &&
                  rotated.resident_splat_bytes==before.resident_splat_bytes,
                  "bounded rotation updates the retained depth order without vertex re-upload");

            const Pointf centre(fixture.size.cx*.5,fixture.size.cy*.5);
            const auto pick_start=std::chrono::steady_clock::now();
            const auto hit=fixture.state->scene->PickScreen(centre,fixture.size,camera);
            const double pick_ms=std::chrono::duration<double,std::milli>(
                std::chrono::steady_clock::now()-pick_start).count();
            const auto sample_start=std::chrono::steady_clock::now();
            const auto sample=fixture.state->scene->SampleSplatSurface(centre,8,fixture.size,camera,object.id);
            const double sample_ms=std::chrono::duration<double,std::milli>(
                std::chrono::steady_clock::now()-sample_start).count();
            Check(hit.hit && hit.id==object.id && sample.pick.hit && sample.pick.id==object.id &&
                  sample.support.sample_count==25 && sample.support.support_count>0 &&
                  !sample.support.normal_valid,
                  "expanded working cloud retains native click and bounded patch sampling");

            ValueMap measurement; measurement.Add("points",point_count);
            measurement.Add("retained_bytes",(int64)before.resident_splat_bytes);
            measurement.Add("first_frame_including_publish_readback_ms",first_ms);
            measurement.Add("sort_microseconds",(int64)before.splat_sort_us_last_frame);
            measurement.Add("steady_frames",12); measurement.Add("steady_cpu_submit_total_ms",steady_ms);
            measurement.Add("steady_cpu_submit_max_ms",maximum_ms);
            measurement.Add("rotation_cpu_submit_ms",rotation_ms);
            measurement.Add("rotation_sort_microseconds",(int64)rotated.splat_sort_us_last_frame);
            measurement.Add("screen_pick_ms",pick_ms); measurement.Add("patch_sample_ms",sample_ms);
            measurement.Add("patch_support",sample.support.support_count);
            measurement.Add("timing_scope","Native CPU submission and picking wall time; no separate GPU timestamp or scan-out claim.");
            gaussian_measurements.Add(measurement);
        }
        fixture.SetScene({}); fixture.Render();
        Check(fixture.state->stats.resident_splat_bytes==0 && fixture.state->stats.resident_splat_resources==0 &&
              fixture.state->stats.resident_splat_instances==0,"bounded cloud cleanup releases all source and instance GPU buffers");
    }
}

}

GUI_APP_MAIN
{
    for(int i=0;i<CommandLine().GetCount();++i)
        if(CommandLine()[i]=="--output" && i+1<CommandLine().GetCount()) output_folder=CommandLine()[++i];
    if(!output_folder.IsEmpty()) Check(RealizeDirectory(output_folder),"create requested proof directory");
    using namespace VulkanTestHooks;
    ClearVulkanRuntimeDeviceDiagnostics();
    HWND window=CreateWindowExW(0,L"STATIC",L"Ui3DMaterialTest owned Vulkan target",WS_POPUP,
                               0,0,128,128,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    VulkanSurfaceSession session;
    if(Check(window!=nullptr,"create bounded hidden native fixture")) {
        GpuNativeWindowDesc native; native.kind=GpuNativeWindowKind::Win32; native.handle=(uintptr_t)window;
        if(Check(session.Open(true,native) && session.IsReady(),"open actual validated Vulkan device")) {
            Check(session.GetReport().validation_available && session.GetReport().debug_utils_available &&
                  GetVulkanRuntimeDeviceDiagnostics().debug_messenger_live_count>0,"validation is actually enabled");
            {
                VulkanGpuDevice device(session);
                if(Check(device.IsReady(),"native GPU device ready")) {
                    {
                        Fixture fixture(device);
                        TestTextureAndColor(fixture); TestCoverage(fixture);
                        TestOrderingAndCull(fixture); TestCameraAndGuides(fixture); TestRetention(fixture);
                        TestGaussianBlendPrecision(fixture);
                        TestGaussianPixels(fixture); TestGaussianDepthAndPicking(fixture);
                        TestGlobalGaussianOrder(fixture); TestGaussianRetentionAndBudget(fixture);
                    }
                    Check(device.GetLiveBufferCount()==0 && device.GetLiveTextureCount()==0 &&
                          device.GetLiveShaderCount()==0 && device.GetLivePipelineCount()==0 &&
                          device.GetLiveCommandCount()==0,"view Close releases every owned GPU resource");
                }
            }
        }
    }
    session.Close();
    Check(session.GetReport().validation_error_count==0,"zero Vulkan validation errors");
    Check(session.GetReport().validation_warning_count==0,"zero Vulkan validation warnings");
    if(session.GetReport().validation_error_count || session.GetReport().validation_warning_count)
        for(const auto& message:session.GetReport().validation_messages) Cout()<<message<<EOL;
    if(window) DestroyWindow(window);
    const auto live=GetVulkanRuntimeDeviceDiagnostics();
    Check(live.runtime_live_count==0 && live.instance_live_count==0 && live.debug_messenger_live_count==0 &&
          live.surface_live_count==0 && live.device_live_count==0 && live.swapchain_live_count==0,
          "finite fixture releases Vulkan session ownership");
    Check(rendered>=37 && pixel_checks>=50,"actual renderer and pixel oracles executed");
    Check(gaussian_pixel_checks>=32 && gaussian_measurements.GetCount()==2,
          "Gaussian analytical pixels and both 150000-point and maximum-budget native measurements executed");
    Cout()<<"Ui3DMaterialTest: "<<checks<<" checks, "<<pixel_checks<<" pixel checks, "<<rendered
          <<" actual GPU frames, "<<failures<<" failures"<<EOL;
    if(!output_folder.IsEmpty()) {
        ValueMap report; report.Add("checks",checks); report.Add("pixel_checks",pixel_checks);
        report.Add("gpu_frames",rendered); report.Add("failures",failures);
        report.Add("gaussian_pixel_checks",gaussian_pixel_checks);
        report.Add("gaussian_measurements",gaussian_measurements);
        report.Add("uniform_blend_precision",gaussian_precision);
        report.Add("evidence","Owned production-renderer Vulkan targets; not final-screen presentation.");
        if(!SaveFile(AppendFileName(output_folder,"report.json"),AsJSON(report,true))) SetExitCode(2);
    }
    if(failures) SetExitCode(1);
}
