#version 460
#include "../light.inc.glsl"

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex_coord;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec4 tangent;

layout (set = 0, binding = 0) uniform uniform_buffer {
	mat4 model;
	mat4 normal_model;
};

layout (set = 1, binding = 0) readonly buffer uniform_buffer2 {
	LightParams lights[];
};

layout (push_constant) uniform uniform_buffer3 {
	uint light_index;
};

layout (location = 0) out VS_OUT {
	vec4 position;
} vs_out;

void main() {
	vs_out.position = lights[light_index].view * model * vec4(position, 1.0f);
    gl_Position = lights[light_index].proj * vs_out.position;
}