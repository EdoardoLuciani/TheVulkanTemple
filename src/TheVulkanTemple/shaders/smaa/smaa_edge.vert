#version 460
//#extension GL_KHR_vulkan_glsl: enable
#define SMAA_PRESET_ULTRA 1
#define SMAA_INCLUDE_VS 1
#define SMAA_GLSL_4 1
#define SMAA_PREDICATION 1

layout (set = 0, binding = 0) uniform sampler2D color_tex;

vec4 SMAA_RT_METRICS = vec4(1.0f/textureSize(color_tex, 0), textureSize(color_tex, 0));
#include "../SMAA.h"

layout (location = 0) out VS_OUT {
	vec2 tex_coord;
    vec4 offset[3];
} vs_out;

void main() {
    vs_out.tex_coord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    SMAAEdgeDetectionVS(vs_out.tex_coord, vs_out.offset);
    gl_Position = vec4(vs_out.tex_coord * 2.0f + -1.0f, 0.0f, 1.0f);
}