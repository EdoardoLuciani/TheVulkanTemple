#version 440
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

layout (set = 0, binding = 0) uniform uniform_buffer {
	mat4 view;
	mat4 inverse_view;
	mat4 projection;
	vec4 camera_pos;
};

layout (set = 1, binding = 0) uniform uniform_buffer1 {
    mat4 camera_v;
    mat4 camera_p;
	vec4 light_pos[1];
	vec4 light_color;
};

void main() {

	mat4 model_s = mat4(	0.05f, 0, 0, 0,
							0, 0.05f, 0, 0,
							0, 0, 0.05f, 0,
							0, 0, 0, 1
	);
	mat4 model_t = mat4(1.0f, 0, 0, 0,
						0, 1.0f, 0, 0,
						0, 0, 1.0f, 0,
						light_pos[0].x, light_pos[0].y, light_pos[0].z, 1.0f
	);

	mat4 model = model_t * model_s;

	gl_Position = projection * view * model * vec4(position,1.0f);
}