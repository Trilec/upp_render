#version 450
// Edges retain the surface's alpha test and independent fade.
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
    float material_alpha=texture(color_texture,vec2(texcoord.x,1.0-texcoord.y)).a*vertex_color.a;
    float alpha=coverage(material_alpha);
    float mode=parameters.normal0.w;
    vec3 rgb=mode<-.5?vertex_color.rgb:parameters.tint.rgb;
    if(mode>.5 && mode<1.5) rgb=lit_color(rgb);
    // Primary wire writes its true depth. Helpers/overlays bias only the fragment,
    // preserving near/far clipping and never pulling the stored solid depth forward.
    float bias=mode<-.5 || mode>1.5?0.0000002:0.0;
    gl_FragDepth=max(gl_FragCoord.z-bias,0.0);
    fragment_color=vec4(rgb,alpha);
}
