#version 450
// Linear scene colors; V-up UVs are converted only at sampling.
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;
layout(location=3) in vec4 color;
layout(push_constant) uniform MeshParameters {
    mat4 mvp; vec4 normal0; vec4 normal1; vec4 normal2; vec4 tint;
} parameters;
layout(location=0) out vec4 vertex_color;
layout(location=1) out vec3 world_normal;
layout(location=2) out vec2 texcoord;
void main()
{
    gl_Position=parameters.mvp*vec4(position,1.0);
    world_normal=mat3(parameters.normal0.xyz,parameters.normal1.xyz,parameters.normal2.xyz)*normal;
    vertex_color=color*parameters.tint;
    texcoord=uv;
}
