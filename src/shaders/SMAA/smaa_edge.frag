#version 460
//#extension GL_KHR_vulkan_glsl: enable
#define SMAA_PRESET_ULTRA 1
#define SMAA_INCLUDE_PS 1
#define SMAA_GLSL_4 1
#define SMAA_PREDICATION 1

layout (set = 1, binding = 0) uniform uniform_buffer1 {
	vec4 SMAA_RT_METRICS;
};
#include "SMAA.h"

layout (set = 0, binding = 0) uniform sampler2D textures[2];
layout (location = 0) in VS_OUT {
	vec2 tex_coord;
    vec4 offset[3];
} fs_in;

layout (location = 0) out vec2 frag_color;

void main() {
    frag_color = SMAAColorEdgeDetectionPS(fs_in.tex_coord, fs_in.offset, textures[0], textures[1]);
}