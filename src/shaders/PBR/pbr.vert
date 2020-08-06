#version 440
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex_coord;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec4 tangent;

layout (set = 0, binding = 0) uniform uniform_buffer1 {
	mat4 model;
	mat4 normal_model;
};

layout (set = 1, binding = 0) uniform uniform_buffer2 {
	mat4 view;
	mat4 inverse_view;
	mat4 projection;
	vec4 camera_pos;
};

layout (set = 2, binding = 0) uniform uniform_buffer3 {
    mat4 camera_v;
    mat4 camera_p;
	vec4 light_pos[1];
	vec4 light_color;
};

layout (location = 0) out VS_OUT {
	vec3 position;
	vec2 tex_coord;
	vec3 normal;
	vec4 shadow_coord;
	vec3 tangent;
	vec3 bitangent;
} vs_out;

void main() {

	mat4 shadow_bias = mat4(0.5f,0.0f,0.0f,0.0f,
                        	0.0f,0.5f,0.0f,0.0f,
                        	0.0f,0.0f,0.5f,0.0f,
                        	0.5f,0.5f,0.5f,1.0f);

	vs_out.tex_coord = tex_coord;
	vs_out.position = vec3(model * vec4(position,1.0f));
	vs_out.normal = mat3(normal_model) * normal;

	vs_out.shadow_coord = shadow_bias * camera_p * camera_v * model * vec4(position,1.0f);

	vs_out.tangent = normalize(mat3(normal_model) * tangent.xyz);
    vs_out.bitangent = cross(vs_out.normal, vs_out.tangent);

	gl_Position = projection * view * model * vec4(position,1.0f);
}