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
        error="A Gaussian resource requires 1..150000 points.";return {};
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
    return std::exp(-0.5*std::max(0.0,squared));
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
