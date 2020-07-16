#version 440
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex_coord;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec4 tangent;

layout (set = 0, binding = 0) uniform uniform_buffer1 {
	mat4 model;
	mat4 view;
	mat4 projection;
	mat4 normal_model;
	mat4 normal_view;
	vec4 camera_pos;
	vec4 light_pos[4];
};

layout (location = 0) out VS_OUT {
	vec3 position;
	vec2 tex_coord;
	vec3 normal;
} vs_out;

void main() {
	vs_out.tex_coord = tex_coord;
	vs_out.position = vec3(model * vec4(position,1.0f));
	vs_out.normal = mat3(normal_model) * normal;

	gl_Position = projection * view * model * vec4(position,1.0f);
}