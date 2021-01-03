#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex_coord;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec4 tangent;

layout (set = 0, binding = 0) uniform uniform_buffer {
	mat4 model;
	mat4 normal_model;
};

struct Light {
	mat4 view;
	mat4 proj;
	vec4 pos;
	vec4 color;
};
layout (set = 1, binding = 0) buffer uniform_buffer2 {
	Light lights[];
};

layout (push_constant) uniform uniform_buffer3 {
	uint light_index;
};

layout (location = 0) out VS_OUT {
	vec4 position;
} vs_out;

void main() {
    gl_Position = lights[light_index].proj * lights[light_index].view * model * vec4(position, 1.0f);
	vs_out.position = gl_Position;
}