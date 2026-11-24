#include "Scene3D.h"
#include <algorithm>
#include <cmath>

namespace Upp {

std::shared_ptr<const Scene3DTexture> Scene3DTexture::Create(
    Size size, const std::vector<byte>& straight_rgba,
    Scene3DTextureColorSpace color_space, String& error)
{
    error.Clear();
    const int64 count=int64(size.cx)*size.cy;
    if(size.cx<1 || size.cy<1 || size.cx>16384 || size.cy>16384 ||
       count>32*1024*1024 || straight_rgba.size()!=size_t(count)*4) {
        error="Texture requires exact RGBA8 pixels, dimensions 1..16384 and at most 32 million pixels";
        return {};
    }
    if(color_space!=Scene3DTextureColorSpace::Srgb && color_space!=Scene3DTextureColorSpace::Linear) {
        error="Texture colour space must be sRGB or linear"; return {};
    }
    std::shared_ptr<Scene3DTexture> texture(new Scene3DTexture);
    texture->size=size; texture->pixels=straight_rgba; texture->color_space=color_space;
    return texture;
}

double Scene3DTexture::SampleAlpha(double u, double v, bool repeat_uv) const
{
    if(!std::isfinite(u) || !std::isfinite(v)) return 0;
    auto address=[&](double value) {
        return repeat_uv?value-std::floor(value):std::max(0.0,std::min(1.0,value));
    };
    const double x=address(u)*size.cx-0.5, y=address(1-v)*size.cy-0.5;
    const int left=(int)std::floor(x), top=(int)std::floor(y);
    const double fx=x-left, fy=y-top;
    auto index=[&](int value,int limit) {
        if(repeat_uv) { value%=limit; return value<0?value+limit:value; }
        return std::max(0,std::min(limit-1,value));
    };
    auto alpha=[&](int px,int py) {
        const size_t offset=(size_t(index(py,size.cy))*size.cx+index(px,size.cx))*4+3;
        return pixels[offset]/255.0;
    };
    return (alpha(left,top)*(1-fx)+alpha(left+1,top)*fx)*(1-fy) +
           (alpha(left,top+1)*(1-fx)+alpha(left+1,top+1)*fx)*fy;
}

bool Scene3DMaterial::Validate(String& error) const
{
    error.Clear();
    if(alpha_mode!=Scene3DAlphaMode::Opaque && alpha_mode!=Scene3DAlphaMode::Mask &&
       alpha_mode!=Scene3DAlphaMode::Blend) {
        error="Material alpha mode must be Opaque, Mask or Blend"; return false;
    }
    if(!std::isfinite(alpha_cutoff) || alpha_cutoff<0 || alpha_cutoff>1) {
        error="Material alpha cutoff must be finite in 0..1"; return false;
    }
    return true;
}

}
