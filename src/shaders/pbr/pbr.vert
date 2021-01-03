#version 460
#extension GL_EXT_nonuniform_qualifier : enable
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex_coord;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec4 tangent;

layout (set = 0, binding = 0) uniform uniform_buffer1 {
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


layout (set = 2, binding = 0) uniform uniform_buffer3 {
    mat4 view;
    mat4 projection;
	vec4 camera_pos;
};

#define MAX_LIGHT_DATA 8
layout (location = 0) out VS_OUT {
	vec3 position;
	vec2 tex_coord;
	vec3 V;
	vec3 L[MAX_LIGHT_DATA];
	vec4 shadow_coord[MAX_LIGHT_DATA];
} vs_out;

void main() {
	vs_out.tex_coord = tex_coord;
	vs_out.position = vec3(model * vec4(position,1.0f));

	mat4 shadow_bias = mat4(0.5f,0.0f,0.0f,0.0f,
                        	0.0f,0.5f,0.0f,0.0f,
                        	0.0f,0.0f,0.5f,0.0f,
                        	0.5f,0.5f,0.5f,1.0f);

	for (int i=0; i<lights.length(); i++) {
		vs_out.shadow_coord[i] = shadow_bias * lights[i].proj * lights[i].view * model * vec4(position,1.0f);
	}

	vec3 N = normalize(mat3(normal_model) * normal);
	vec3 T = normalize(mat3(normal_model) * tangent.xyz);
	T = normalize(T - dot(T, N) * N);
    vec3 B = normalize(cross(N, T)) * tangent.w;
	mat3 tbn = mat3( 	T.x, B.x, N.x,
						T.y, B.y, N.y,
						T.z, B.z, N.z);

    vs_out.V = tbn * (vec3(camera_pos) - vs_out.position);
	
	for(int i=0; i<lights.length(); i++) {
		if (lights[i].pos.w == 0.0) {
			vs_out.L[i] = tbn * normalize(vec3(lights[i].pos));
		}
		else {
			vs_out.L[i] = tbn * (vec3(lights[i].pos) - vs_out.position);
		}
	}
	gl_Position = projection * view * model * vec4(position,1.0f);
}