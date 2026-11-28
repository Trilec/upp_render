#include "Scene3D.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Upp {
namespace {
constexpr double Radius = 3.3290429691304455; // sqrt(-2*log(1/255)).
bool FiniteSplat(const Scene3DSplat& value)
{
    if(!value.position.IsFinite() || std::abs(value.position.x)>1e9 ||
       std::abs(value.position.y)>1e9 || std::abs(value.position.z)>1e9) return false;
    for(float channel:{value.r,value.g,value.b,value.a})
        if(!std::isfinite(channel) || channel<0 || channel>1) return false;
    double scale=0;
    for(double covariance:value.covariance) {
        if(!std::isfinite(covariance) || std::abs(covariance)>1e18) return false;
        scale=std::max(scale,std::abs(covariance));
    }
    if(value.covariance[0]<0 || value.covariance[3]<0 || value.covariance[5]<0) return false;
    if(scale==0) return true;
    // Principal minors characterize a symmetric positive-semidefinite matrix.
    // Scale first to keep the relative rounding tolerance independent of units.
    const double a=value.covariance[0]/scale,b=value.covariance[1]/scale,
        c=value.covariance[2]/scale,d=value.covariance[3]/scale,
        e=value.covariance[4]/scale,f=value.covariance[5]/scale;
    constexpr double tolerance=128*std::numeric_limits<double>::epsilon();
    return a*d-b*b>=-tolerance && a*f-c*c>=-tolerance && d*f-e*e>=-tolerance &&
           a*(d*f-e*e)-b*(b*f-c*e)+c*(b*e-c*d)>=-tolerance;
}
double Product(const double c[6],const Scene3DVec3& a,const Scene3DVec3& b)
{
    return a.x*(c[0]*b.x+c[1]*b.y+c[2]*b.z)+
           a.y*(c[1]*b.x+c[3]*b.y+c[4]*b.z)+
           a.z*(c[2]*b.x+c[4]*b.y+c[5]*b.z);
}
}

std::shared_ptr<const Scene3DSplats> Scene3DSplats::Create(
    const std::vector<Scene3DSplat>& source,String& error)
{
    error.Clear();
    if(source.empty() || source.size()>Scene3DMaxSplats) {
        error=Format("A Gaussian resource requires 1..%d points.",Scene3DMaxSplats);return {};
    }
    std::shared_ptr<Scene3DSplats> result(new Scene3DSplats);
    for(const auto& value:source) {
        if(!FiniteSplat(value)) {
            error="Gaussian centres, linear colours and covariance must be bounded, finite and positive semidefinite.";
            return {};
        }
        const Scene3DVec3 extent(Radius*std::sqrt(value.covariance[0]),
                                Radius*std::sqrt(value.covariance[3]),
                                Radius*std::sqrt(value.covariance[5]));
        result->bounds.Add(value.position-extent);
        result->bounds.Add(value.position+extent);
    }
    result->splats=source; // Preserve all values and detach every mutable input alias.
    return result;
}

bool Scene3DProjectSplat(const Scene3DSplat& value,const Scene3DMatrix& matrix,
                         Size viewport,Scene3DSplatProjection& output)
{
    if(viewport.cx<=0 || viewport.cy<=0 || !FiniteSplat(value)) return false;
    for(double entry:matrix.m) if(!std::isfinite(entry)) return false;
    const auto& p=value.position;
    double clip[4];
    for(int row=0;row<4;++row)
        clip[row]=matrix.m[row]*p.x+matrix.m[4+row]*p.y+
                  matrix.m[8+row]*p.z+matrix.m[12+row];
    if(!std::isfinite(clip[3]) || clip[3]<=0 || !std::isfinite(clip[2]) ||
       clip[2]<0 || clip[2]>clip[3]) return false;
    const double inverse_w=1/clip[3],x=clip[0]*inverse_w,y=clip[1]*inverse_w;
    if(!std::isfinite(x) || !std::isfinite(y)) return false;
    // Derivative of the complete projective transform in screen pixels. This
    // includes nonuniform object scale, every rigid parent, roll and film gate.
    const Scene3DVec3 row_x(matrix.m[0],matrix.m[4],matrix.m[8]);
    const Scene3DVec3 row_y(matrix.m[1],matrix.m[5],matrix.m[9]);
    const Scene3DVec3 row_w(matrix.m[3],matrix.m[7],matrix.m[11]);
    const auto jx=(row_x-row_w*x)*(0.5*viewport.cx*inverse_w);
    const auto jy=(row_y-row_w*y)*(0.5*viewport.cy*inverse_w);
    Scene3DSplatProjection projected;
    projected.center=Pointf((x+1)*0.5*viewport.cx,(y+1)*0.5*viewport.cy);
    projected.covariance_xx=Product(value.covariance,jx,jx)+Scene3DSplatPixelVariance;
    projected.covariance_xy=Product(value.covariance,jx,jy);
    projected.covariance_yy=Product(value.covariance,jy,jy)+Scene3DSplatPixelVariance;
    projected.depth=clip[2]*inverse_w;
    const double determinant=std::fma(projected.covariance_xx,projected.covariance_yy,
                                     -projected.covariance_xy*projected.covariance_xy);
    if(!std::isfinite(projected.center.x) || !std::isfinite(projected.center.y) ||
       !std::isfinite(projected.covariance_xx) || !std::isfinite(projected.covariance_xy) ||
       !std::isfinite(projected.covariance_yy) || !std::isfinite(determinant) || determinant<=0)
        return false;
    if(row_w.Dot(row_w)>0) {
        const auto smoothstep=[](double lo,double hi,double value) {
            const double fraction=std::clamp((value-lo)/(hi-lo),0.0,1.0);
            return fraction*fraction*(3-2*fraction);
        };
        const double span_x=2*Radius*std::sqrt(projected.covariance_xx)/viewport.cx;
        const double span_y=2*Radius*std::sqrt(projected.covariance_yy)/viewport.cy;
        const double wide=smoothstep(.5,1,std::max(span_x,span_y));
        if(wide>0) {
            // Match the viewport shaders when testing picks or calibration against the cloud.
            const double sigma=std::sqrt(std::max(0.0,Product(value.covariance,row_w,row_w)));
            const double depth_quality=smoothstep(1,3,clip[3]/std::max(sigma,1e-8));
            const double footprint=3.141592653589793*Radius*Radius*std::sqrt(determinant)/
                                   (double(viewport.cx)*viewport.cy);
            projected.opacity_scale=(1-wide*(1-depth_quality))*(1-smoothstep(.25,.85,footprint));
        }
    }
    output=projected;
    return true;
}

double Scene3DSplatProjection::Coverage(Pointf point) const
{
    if(!std::isfinite(point.x) || !std::isfinite(point.y)) return 0;
    const double determinant=std::fma(covariance_xx,covariance_yy,-covariance_xy*covariance_xy);
    if(!std::isfinite(determinant) || determinant<=0) return 0;
    const double x=point.x-center.x,y=point.y-center.y;
    const double squared=(covariance_yy*x*x-2*covariance_xy*x*y+covariance_xx*y*y)/determinant;
    if(!std::isfinite(squared) || squared>Radius*Radius) return 0;
    return opacity_scale*std::exp(-0.5*std::max(0.0,squared));
}

Scene3DSplatSurfaceSample Scene3DSnapshot::SampleSplatSurface(Pointf screen, double radius_pixels,
                                                             Size viewport, const Scene3DCamera& camera,
                                                             const String& object_id) const
{
    Scene3DSplatSurfaceSample result;
    result.pick.scene_revision=revision;
    result.pick.camera_revision=camera.revision;
    if(!std::isfinite(screen.x) || !std::isfinite(screen.y) ||
       !std::isfinite(radius_pixels) || radius_pixels<0 ||
       screen.x<0 || screen.y<0 || screen.x>=viewport.cx || screen.y>=viewport.cy)
        return result;
    result.support.radius_pixels=std::min(radius_pixels,128.0);
    const auto *object=FindObject(object_id);
    if(!object || !object->visible || !object->selectable || !object->splats ||
       object->opacity<=0 || object->a<=0)
        return result;

    Scene3DRay centre_ray;
    if(!camera.WorldRay(screen,viewport,centre_ray)) return result;
    Scene3DVec3 right,up,forward; camera.GetBasis(right,up,forward);
    const auto eye=camera.GetEye();
    struct Probe {
        Pointf screen;
        Scene3DRay ray;
        double cosine=0, weight=0, depth=0;
        Scene3DPick pick;
    };
    std::vector<Probe> probes;
    probes.reserve(25);
    auto add_probe=[&](Pointf point,double weight) {
        if(point.x<0 || point.y<0 || point.x>=viewport.cx || point.y>=viewport.cy) return;
        Probe probe; probe.screen=point; probe.weight=weight; probe.pick=result.pick;
        if(!camera.WorldRay(point,viewport,probe.ray)) return;
        probe.cosine=probe.ray.direction.Dot(forward);
        if(probe.cosine<=0) return;
        probes.push_back(probe);
    };
    add_probe(screen,1);
    const double radius=result.support.radius_pixels;
    if(radius>0) {
        constexpr double pi=3.14159265358979323846;
        for(int ring=1;ring<=2;++ring) {
            const int count=ring*8;
            const double ring_radius=radius*ring*.5;
            const double weight=std::exp(-.5*ring*ring);
            for(int i=0;i<count;++i) {
                const double angle=2*pi*i/count;
                add_probe(Pointf(screen.x+std::cos(angle)*ring_radius,
                                 screen.y+std::sin(angle)*ring_radius),weight);
            }
        }
    }
    result.support.sample_count=int(probes.size());
    if(probes.empty()) return result;

    const auto matrix=camera.GetViewProjection(viewport)*object->world;
    const auto& points=object->splats->GetSplats();
    for(size_t i=0;i<points.size();++i) {
        const auto& value=points[i];
        const double peak=value.a*object->a*object->opacity;
        if(peak<Scene3DSplatAlphaCutoff) continue;
        Scene3DSplatProjection projected;
        if(!Scene3DProjectSplat(value,matrix,viewport,projected)) continue;
        // Reject clouds outside the patch before evaluating individual coverage.
        const double extent_x=Radius*std::sqrt(projected.covariance_xx);
        const double extent_y=Radius*std::sqrt(projected.covariance_yy);
        if(std::abs(projected.center.x-screen.x)>radius+extent_x ||
           std::abs(projected.center.y-screen.y)>radius+extent_y) continue;
        const auto centre=object->world.TransformPoint(value.position);
        const double view_depth=(centre-eye).Dot(forward);
        for(auto& probe:probes) {
            const double distance=(centre-probe.ray.origin).Dot(forward)/probe.cosine;
            if(distance<probe.ray.min_distance || distance>probe.ray.max_distance ||
               (probe.pick.hit && distance>=probe.pick.distance) ||
               peak*projected.Coverage(probe.screen)<Scene3DSplatAlphaCutoff) continue;
            probe.depth=view_depth;
            probe.pick.hit=true; probe.pick.id=object->id; probe.pick.distance=distance;
            probe.pick.world_point=probe.ray.origin+probe.ray.direction*distance;
            probe.pick.world_normal=-forward; probe.pick.splat=int(i);
        }
    }
    std::vector<int> hits;
    hits.reserve(probes.size());
    for(size_t i=0;i<probes.size();++i)
        if(probes[i].pick.hit) hits.push_back(int(i));
    result.support.hit_count=int(hits.size());
    if(hits.empty()) return result;
    std::sort(hits.begin(),hits.end(),[&](int a,int b) {
        return probes[a].depth<probes[b].depth ||
               (probes[a].depth==probes[b].depth && a<b);
    });

    // Split separated depth modes before finding a representative. A small
    // pixel-sized floor tolerates raster depth noise; the lower three quarters
    // of adjacent steps tolerate a sloped patch without following a large gap.
    const double middle_depth=probes[hits[hits.size()/2]].depth;
    const double centre_cosine=centre_ray.direction.Dot(forward);
    const auto centre_at_depth=centre_ray.origin+centre_ray.direction*
        ((middle_depth-(centre_ray.origin-eye).Dot(forward))/centre_cosine);
    Scene3DRay pixel_ray;
    if(!camera.WorldRay(Pointf(screen.x+1,screen.y),viewport,pixel_ray)) return result;
    const double pixel_cosine=pixel_ray.direction.Dot(forward);
    const auto pixel_at_depth=pixel_ray.origin+pixel_ray.direction*
        ((middle_depth-(pixel_ray.origin-eye).Dot(forward))/pixel_cosine);
    const double pixel_size=(pixel_at_depth-centre_at_depth).Length();
    std::vector<double> gaps;
    for(size_t i=1;i<hits.size();++i)
        gaps.push_back(probes[hits[i]].depth-probes[hits[i-1]].depth);
    std::sort(gaps.begin(),gaps.end());
    const double regular_step=gaps.size()>=3 ? gaps[(gaps.size()-1)*3/4] : 0;
    const double gap_limit=std::max(std::max(pixel_size*2.5,
                                           std::abs(middle_depth)*1e-9),regular_step*3);

    size_t best_begin=0, best_end=0, begin=0;
    double best_weight=-1;
    for(size_t end=1;end<=hits.size();++end) {
        if(end<hits.size() &&
           probes[hits[end]].depth-probes[hits[end-1]].depth<=gap_limit) continue;
        double weight=0;
        for(size_t i=begin;i<end;++i) weight+=probes[hits[i]].weight;
        // Equal support keeps the front group, since hits are depth-sorted.
        if(weight>best_weight) { best_weight=weight; best_begin=begin; best_end=end; }
        begin=end;
    }

    // A medoid is one of the observed hits. Even an ambiguous or sparse patch
    // can never fabricate a point halfway through two independently hit walls.
    int representative=hits[best_begin];
    double best_cost=std::numeric_limits<double>::infinity();
    for(size_t i=best_begin;i<best_end;++i) {
        const int candidate=hits[i];
        double cost=0;
        for(size_t j=best_begin;j<best_end;++j)
            cost+=(probes[candidate].pick.world_point-probes[hits[j]].pick.world_point).Length()
                  *probes[hits[j]].weight;
        if(cost<best_cost || (cost==best_cost && candidate<representative)) {
            best_cost=cost; representative=candidate;
        }
    }
    result.pick=probes[representative].pick;
    auto& support=result.support;
    support.support_count=int(best_end-best_begin);
    support.depth_spread=probes[hits[best_end-1]].depth-probes[hits[best_begin]].depth;
    Pointf screen_mean(0,0);
    Scene3DVec3 world_mean;
    std::vector<int> distinct;
    for(size_t i=best_begin;i<best_end;++i) {
        const auto& probe=probes[hits[i]];
        screen_mean.x+=probe.screen.x; screen_mean.y+=probe.screen.y;
        world_mean=world_mean+probe.pick.world_point;
        if(std::find(distinct.begin(),distinct.end(),probe.pick.splat)==distinct.end())
            distinct.push_back(probe.pick.splat);
    }
    screen_mean.x/=support.support_count; screen_mean.y/=support.support_count;
    world_mean=world_mean/support.support_count;
    double screen_variance=0, world_variance=0;
    for(size_t i=best_begin;i<best_end;++i) {
        const auto& probe=probes[hits[i]];
        const double dx=probe.screen.x-screen_mean.x, dy=probe.screen.y-screen_mean.y;
        screen_variance+=dx*dx+dy*dy;
        const auto delta=probe.pick.world_point-world_mean;
        world_variance+=delta.Dot(delta);
    }
    support.screen_spread_pixels=std::sqrt(screen_variance/support.support_count);
    support.world_spread=std::sqrt(world_variance/support.support_count);
    support.splat_count=int(distinct.size());
    support.confidence=double(support.support_count)/support.sample_count *
                       std::min(1.0,support.splat_count/4.0);
    return result;
}

Scene3DPick Scene3DSnapshot::PickScreen(Pointf screen,Size viewport,const Scene3DCamera& camera) const
{
    Scene3DPick result;
    result.scene_revision=revision;result.camera_revision=camera.revision;
    if(screen.x<0 || screen.y<0 || screen.x>=viewport.cx || screen.y>=viewport.cy) return result;
    Scene3DRay ray;
    if(!camera.WorldRay(screen,viewport,ray)) return result;
    result=Pick(ray,camera.revision);
    double nearest=result.hit?result.distance:ray.max_distance;
    int nearest_object=result.hit?ids.Find(result.id):INT_MAX;
    Scene3DVec3 right,up,forward;camera.GetBasis(right,up,forward);
    const double cosine=ray.direction.Dot(forward);
    if(cosine<=0) return result;
    const auto view_projection=camera.GetViewProjection(viewport);
    for(size_t object_index=0;object_index<objects.size();++object_index) {
        const auto& object=objects[object_index];
        if(!object.visible || !object.selectable || !object.splats || object.opacity<=0 || object.a<=0)
            continue;
        const auto matrix=view_projection*object.world;
        const auto& points=object.splats->GetSplats();
        for(size_t i=0;i<points.size();++i) {
            const auto& value=points[i];
            const double peak=value.a*object.a*object.opacity;
            if(peak<Scene3DSplatAlphaCutoff) continue;
            Scene3DSplatProjection projected;
            if(!Scene3DProjectSplat(value,matrix,viewport,projected) ||
               peak*projected.Coverage(screen)<Scene3DSplatAlphaCutoff) continue;
            const auto centre=object.world.TransformPoint(value.position);
            // Quads retain their centre's view depth, exactly as the shader.
            const double distance=(centre-ray.origin).Dot(forward)/cosine;
            if(distance<ray.min_distance || distance>nearest ||
               (result.hit && distance==nearest && int(object_index)>=nearest_object)) continue;
            nearest=distance;nearest_object=int(object_index);
            result.hit=true;result.id=object.id;result.distance=distance;
            result.world_point=ray.origin+ray.direction*distance;
            result.world_normal=-forward;result.triangle=-1;result.splat=int(i);
        }
    }
    return result;
}
}
