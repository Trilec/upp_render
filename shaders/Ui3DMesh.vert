#version 450

// Scene colors are linear. Matrices are column-major / column vectors.
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
// The untextured renderer binds exactly the three attributes consumed here.
layout(location = 2) in vec4 color;

layout(push_constant) uniform MeshParameters {
	mat4 mvp;
	vec4 normal0;
	vec4 normal1;
	vec4 normal2;
	vec4 tint;
} parameters;

layout(location = 0) out vec4 vertex_color;
layout(location = 1) out vec3 world_normal;

void main()
{
	gl_Position = parameters.mvp * vec4(position, 1.0);
	mat3 normal_matrix = mat3(parameters.normal0.xyz,
	                         parameters.normal1.xyz,
	                         parameters.normal2.xyz);
	world_normal = normal_matrix * normal;
	vertex_color = color * parameters.tint;
}
