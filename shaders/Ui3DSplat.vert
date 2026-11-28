#version 450
// Complete local covariance is projected analytically, never replaced by a box.
layout(location=0) in vec3 in_position;
layout(location=1) in vec3 in_covariance_x;
layout(location=2) in vec3 in_covariance_yz;
layout(location=3) in vec2 in_corner;
layout(location=4) in vec4 in_color;
layout(push_constant) uniform Parameters {
    mat4 clip_from_local;
    vec4 tint;
    vec4 viewport_opacity; // xy pixels, z independent object opacity.
    vec4 highlight;       // RGB linear selection decoration, A enabled.
} parameters;
layout(location=0) out vec2 gaussian;
layout(location=1) out vec4 color;
layout(location=2) flat out vec4 highlight;
const float radius=3.3290429691304455;
void main()
{
    vec4 clip=parameters.clip_from_local*vec4(in_position,1.0);
    gaussian=vec2(0.0);color=vec4(0.0);highlight=parameters.highlight;
    if(clip.w<=0.0 || clip.z<0.0 || clip.z>clip.w ||
       any(isnan(clip)) || any(isinf(clip))) {
        gl_Position=vec4(2.0,2.0,2.0,1.0);return;
    }
    vec2 ndc=clip.xy/clip.w;
    vec3 row_x=vec3(parameters.clip_from_local[0][0],parameters.clip_from_local[1][0],parameters.clip_from_local[2][0]);
    vec3 row_y=vec3(parameters.clip_from_local[0][1],parameters.clip_from_local[1][1],parameters.clip_from_local[2][1]);
    vec3 row_w=vec3(parameters.clip_from_local[0][3],parameters.clip_from_local[1][3],parameters.clip_from_local[2][3]);
    vec3 jx=(row_x-ndc.x*row_w)*(0.5*parameters.viewport_opacity.x/clip.w);
    vec3 jy=(row_y-ndc.y*row_w)*(0.5*parameters.viewport_opacity.y/clip.w);
    mat3 covariance=mat3(in_covariance_x.x,in_covariance_x.y,in_covariance_x.z,
        in_covariance_x.y,in_covariance_yz.x,in_covariance_yz.y,
        in_covariance_x.z,in_covariance_yz.y,in_covariance_yz.z);
    float xx=dot(jx,covariance*jx)+0.25;
    float xy=dot(jx,covariance*jy);
    float yy=dot(jy,covariance*jy)+0.25;
    // Admitted large covariances can have finite entries whose squared
    // determinant/eigenvalue intermediates overflow float. Solve in normalized
    // units, including the eigenvector, then restore the original variances.
    float variance_scale=max(max(xx,yy),abs(xy));
    vec3 unit_covariance=vec3(xx,xy,yy)/variance_scale;
    float difference=unit_covariance.x-unit_covariance.z;
    float major_unit=0.5*(unit_covariance.x+unit_covariance.z+
        sqrt(max(0.0,difference*difference+4.0*unit_covariance.y*unit_covariance.y)));
    float major_variance=variance_scale*major_unit;
    float minor_variance=max(0.25,variance_scale*
        max(0.0,fma(unit_covariance.x,unit_covariance.z,-unit_covariance.y*unit_covariance.y))/major_unit);
    vec2 axis=abs(unit_covariance.y)>1e-12
        ? normalize(vec2(unit_covariance.y,major_unit-unit_covariance.x))
        : (xx>=yy?vec2(1.0,0.0):vec2(0.0,1.0));
    vec2 offset=radius*(in_corner.x*sqrt(major_variance)*axis+
                       in_corner.y*sqrt(minor_variance)*vec2(-axis.y,axis.x));
    if(any(isnan(offset)) || any(isinf(offset))) {
        gl_Position=vec4(2.0,2.0,2.0,1.0);return;
    }
    gl_Position=vec4((ndc+2.0*offset/parameters.viewport_opacity.xy)*clip.w,clip.z,clip.w);
    gaussian=in_corner*radius;
    color=in_color*parameters.tint;
    color.a*=parameters.viewport_opacity.z;
    // The screen-space ellipse is a first-order perspective approximation.
    // Fade near-eye streaks and ellipses large enough to veil the frame.
    if(dot(row_w,row_w)>0.0) {
        vec2 span=2.0*radius*sqrt(vec2(xx,yy))/parameters.viewport_opacity.xy;
        float wide=smoothstep(0.5,1.0,max(span.x,span.y));
        if(wide>0.0) {
            float forward_sigma=sqrt(max(0.0,dot(row_w,covariance*row_w)));
            float depth_quality=smoothstep(1.0,3.0,clip.w/max(forward_sigma,1e-8));
            float footprint=3.14159265*(radius*sqrt(major_variance)/parameters.viewport_opacity.x)*
                                       (radius*sqrt(minor_variance)/parameters.viewport_opacity.y);
            color.a*= (1.0-wide*(1.0-depth_quality)) * (1.0-smoothstep(0.25,0.85,footprint));
        }
    }
    if(color.a<1.0/255.0) gl_Position=vec4(2.0,2.0,2.0,1.0);
}
