#version 460
//#extension GL_KHR_vulkan_glsl: enable
#define SMAA_PRESET_ULTRA 1
#define SMAA_INCLUDE_VS 1
#define SMAA_GLSL_4 1
#define SMAA_PREDICATION 1

layout (set = 1, binding = 0) uniform uniform_buffer1 {
	vec4 SMAA_RT_METRICS;
};
#include "SMAA.h"

layout (location = 0) out VS_OUT {
	vec2 tex_coord;
    vec4 offset;
} vs_out;

void main() {
    vs_out.tex_coord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    SMAANeighborhoodBlendingVS(vs_out.tex_coord, vs_out.offset);
    gl_Position = vec4(vs_out.tex_coord * 2.0f + -1.0f, 0.0f, 1.0f);
}