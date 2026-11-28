#version 450

layout(std430, set = 0, binding = 0) readonly buffer Tints {
    vec4 tint[];
};

layout(location = 0) out vec4 color;

void main()
{
    const vec2 corner[3] = vec2[](vec2(-0.3, -0.4), vec2(0.3, -0.4), vec2(0.0, 0.4));
    int instance = gl_InstanceIndex;
    vec2 position = corner[gl_VertexIndex] + vec2(float(instance) * 0.95 - 0.475, 0.0);
    gl_Position = vec4(position, 0.5, 1.0);
    color = tint[instance];
}
