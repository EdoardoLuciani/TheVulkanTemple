#version 460

layout (location = 0) out VS_OUT {
	vec2 tex_coord;
} vs_out;

void main() {
    vs_out.tex_coord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(vs_out.tex_coord * 2.0f + -1.0f, 0.0f, 1.0f);
}