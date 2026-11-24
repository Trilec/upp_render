#version 450
// Only UNORM output uses this resolve; the input sRGB texture is decoded to
// linear by its image view, then encoded exactly once for the display bytes.
layout(set=0,binding=0) uniform sampler2D color_texture;
layout(location=0) in vec2 texcoord;
layout(location=1) in vec4 vertex_color;
layout(location=0) out vec4 fragment_color;
vec3 encode_srgb(vec3 value)
{
    vec3 low=value*12.92;
    vec3 high=1.055*pow(max(value,vec3(0.0)),vec3(1.0/2.4))-0.055;
    return mix(high,low,lessThanEqual(value,vec3(0.0031308)));
}
void main()
{
    vec4 color=texture(color_texture,texcoord)*vertex_color;
    fragment_color=vec4(encode_srgb(color.rgb),color.a);
}
