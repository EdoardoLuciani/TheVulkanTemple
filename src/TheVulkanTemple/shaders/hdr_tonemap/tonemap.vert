#version 460
//#extension GL_KHR_vulkan_glsl: enable

void main() {
    vec2 tex_coord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(tex_coord * 2.0f + -1.0f, 0.0f, 1.0f);
}