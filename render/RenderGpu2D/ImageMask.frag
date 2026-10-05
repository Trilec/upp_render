#version 450
// Reference source for kMaskFragmentShader in RenderGpu2DBase.inc.
// Embedded SPIR-V is shipped with the renderer; no runtime shader compiler is needed.
layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 colour;
layout(set = 0, binding = 0) uniform sampler2D image;
layout(location = 0) out vec4 result;
void main() {
    result = vec4(colour.rgb, texture(image, uv).a * colour.a);
}
