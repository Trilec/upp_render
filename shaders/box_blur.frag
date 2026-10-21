#version 450
layout(location=0) in vec2 uv;
layout(location=1) in vec4 colour;
layout(set=0,binding=0) uniform sampler2D image;
layout(location=0) out vec4 result;
void main() {
    ivec2 size=textureSize(image,0);
    ivec2 at=ivec2(uv*vec2(size));
    vec4 sum=vec4(0);
    for(int y=-1;y<=1;++y)
        for(int x=-1;x<=1;++x)
            sum+=texelFetch(image,clamp(at+ivec2(x,y),ivec2(0),size-1),0);
    result=sum*(colour/9.0);
}
