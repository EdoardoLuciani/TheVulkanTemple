#version 440

layout (set = 1, binding = 0) uniform uniform_buffer1 {
	vec4 light_pos[1];
	vec4 light_color;
};

layout (location = 0) out vec4 frag_color;
void main() {
    frag_color = light_color;
}