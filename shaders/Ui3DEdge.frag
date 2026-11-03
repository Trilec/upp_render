#version 450

layout(push_constant) uniform MeshParameters {
	mat4 mvp;
	vec4 normal0; // w: <0 vertex-color helpers, 0 uniform edges, >0 lit uniform color
	vec4 normal1; // w: encode linear RGB for an UNORM target
	vec4 normal2; // w: bounded normalized depth bias, applied after clipping
	vec4 tint;
} parameters;

layout(location = 0) in vec4 vertex_color;
layout(location = 1) in vec3 world_normal;
layout(location = 0) out vec4 fragment_color;

vec3 encode_srgb(vec3 value)
{
	vec3 low = value * 12.92;
	vec3 high = 1.055 * pow(max(value, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055;
	return mix(high, low, lessThanEqual(value, vec3(0.0031308)));
}

void main()
{
	vec3 rgb = parameters.normal0.w < -0.5 ? vertex_color.rgb : parameters.tint.rgb;
	if(parameters.normal0.w > 0.5) {
		vec3 normal = normalize(world_normal);
		float key = max(dot(normal, normalize(vec3(-0.45, 0.8, 0.35))), 0.0);
		float fill = max(dot(normal, normalize(vec3(0.8, 0.2, -0.2))), 0.0);
		rgb *= 0.24 + 0.66 * key + 0.10 * fill;
	}
	if(parameters.normal1.w > 0.5) rgb = encode_srgb(rgb);
	// Fragment-depth bias preserves primitive near/far clipping. Overlays bind a
	// pipeline with depth writes disabled; solid depth is never pulled forwards.
	gl_FragDepth = max(gl_FragCoord.z - clamp(parameters.normal2.w, 0.0, 0.0000002), 0.0);
	fragment_color = vec4(rgb, vertex_color.a);
}
