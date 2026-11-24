#version 450
// Attachment is sRGB: hardware blends in linear space and encodes once.
layout(push_constant) uniform MeshParameters {
    mat4 mvp;
    vec4 normal0; // w: 0 unlit / 1 lit; edge shader also -1 helper / 2 overlay.
    vec4 normal1; // w: independent object opacity.
    vec4 normal2; // w: -2 opaque / -1 blend / otherwise mask cutoff.
    vec4 tint;    // linear base-color factor, including material alpha.
} parameters;
layout(set=0,binding=0) uniform sampler2D color_texture;
layout(location=0) in vec4 vertex_color;
layout(location=1) in vec3 world_normal;
layout(location=2) in vec2 texcoord;
layout(location=0) out vec4 fragment_color;

float coverage(float material_alpha)
{
    float mode=parameters.normal2.w;
    if(mode>=0.0 && material_alpha<mode) discard;
    float resolved=mode>-1.5 && mode<0.0?material_alpha:1.0;
    float result=clamp(resolved*parameters.normal1.w,0.0,1.0);
    if(result<=0.0) discard;
    return result;
}
vec3 lit_color(vec3 rgb)
{
    vec3 n=world_normal*inversesqrt(max(dot(world_normal,world_normal),1e-20));
    if(!gl_FrontFacing) n=-n;
    float key=max(dot(n,normalize(vec3(-0.45,0.8,0.35))),0.0);
    float fill=max(dot(n,normalize(vec3(0.8,0.2,-0.2))),0.0);
    return rgb*(0.24+0.66*key+0.10*fill);
}

void main()
{
    vec4 sampled=texture(color_texture,vec2(texcoord.x,1.0-texcoord.y));
    vec4 base=sampled*vertex_color;
    float alpha=coverage(base.a);
    vec3 rgb=parameters.normal0.w>0.5?lit_color(base.rgb):base.rgb;
    fragment_color=vec4(rgb,alpha);
}
