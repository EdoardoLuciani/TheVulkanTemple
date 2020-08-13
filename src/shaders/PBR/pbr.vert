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
	vec4 shadow_coord;
	vec3 V;
	vec3 L[1];
} vs_out;

void main() {
	vs_out.tex_coord = tex_coord;
	vs_out.position = vec3(model * vec4(position,1.0f));

	mat4 shadow_bias = mat4(0.5f,0.0f,0.0f,0.0f,
                        	0.0f,0.5f,0.0f,0.0f,
                        	0.0f,0.0f,0.5f,0.0f,
                        	0.5f,0.5f,0.5f,1.0f);
	vs_out.shadow_coord = shadow_bias * camera_p * camera_v * model * vec4(position,1.0f);

	vec3 N = normalize(mat3(normal_model) * normal);
	vec3 T = normalize(mat3(normal_model) * tangent.xyz);
	T = normalize(T - dot(T, N) * N);
    vec3 B = normalize(cross(N, T)) * tangent.w;
	mat3 tbn = mat3( 	T.x, B.x, N.x,
						T.y, B.y, N.y,
						T.z, B.z, N.z);

    vs_out.V = tbn * (vec3(camera_pos) - vs_out.position);
	
	for(int i=0; i<1; i++) {
		if (light_pos[0].w == 0.0) {
			vs_out.L[0] = tbn * normalize(vec3(light_pos[0]));
		}
		else {
			vs_out.L[0] = tbn * (vec3(light_pos[0]) - vs_out.position);
		}
	}
	gl_Position = projection * view * model * vec4(position,1.0f);
}