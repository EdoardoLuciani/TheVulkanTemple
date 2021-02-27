#version 460
//#extension GL_KHR_vulkan_glsl: enable
#define SMAA_PRESET_ULTRA 1
#define SMAA_INCLUDE_PS 1
#define SMAA_GLSL_4 1
#define SMAA_PREDICATION 1

layout (set = 0, binding = 0) uniform sampler2D textures[2];

vec4 SMAA_RT_METRICS = vec4(1.0f/textureSize(textures[0], 0), textureSize(textures[0], 0));
#include "SMAA.h"

layout (location = 0) in VS_OUT {
	vec2 tex_coord;
    vec4 offset;
} fs_in;

layout (location = 0) out vec4 frag_color;

void main() {
    vec4 color = SMAANeighborhoodBlendingPS(fs_in.tex_coord, fs_in.offset, textures[0], textures[1]);
	frag_color = vec4(pow(vec3(color), vec3(2.2)),1.0f);
}