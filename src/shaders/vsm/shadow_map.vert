#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex_coord;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec4 tangent;

layout (set = 0, binding = 0) uniform uniform_buffer {
	mat4 model;
	mat4 normal_model;
};

layout (set = 1, binding = 0) uniform uniform_buffer1 {
    mat4 light_v;
    mat4 light_p;
	vec4 light_pos;
	vec4 light_color;
};

layout (location = 0) out VS_OUT {
	vec4 position;
} vs_out;


void main() {
    gl_Position = light_p * light_v * model * vec4(position, 1.0f);
	vs_out.position = gl_Position;
}