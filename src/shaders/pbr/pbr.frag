#version 460
#extension GL_EXT_nonuniform_qualifier : enable
const float PI = 3.14159265358979323846;
#include "BRDF.inc.glsl"

layout (set = 0, binding = 1) uniform sampler2DArray images;

struct Light {
    mat4 view;
    mat4 proj;
    vec4 pos;
    vec4 color;
};
layout (set = 1, binding = 0) readonly buffer uniform_buffer2 {
    Light lights[];
};
layout (set = 1, binding = 1) uniform sampler2D shadow_map[];

#define MAX_LIGHT_DATA 8
layout (location = 0) in VS_OUT {
    vec3 position;
    vec2 tex_coord;
    vec3 V;
    vec3 L[MAX_LIGHT_DATA];
    vec4 shadow_coord[MAX_LIGHT_DATA];
} fs_in;

layout (location = 0) out vec4 frag_color;

// Utility function(s)
float get_sphere_light_attenuation(vec3 light_p, float dist_max) {
    float dist = length(light_p - fs_in.position);
    float d = dist / (1 - pow(dist/dist_max,2));
    return 1/pow(d/2+1,2);
}

// Shadow Helping functions
float ChebyshevUpperBound(vec2 Moments, float t) {
    // One-tailed inequality valid if t > Moments.x
    float p = float(t <= Moments.x);
    // Compute variance.
    float Variance = Moments.y - (Moments.x*Moments.x);
    Variance = max(Variance, 0.000001);
    // Compute probabilistic upper bound.
    float d = t - Moments.x;
    float p_max = Variance / (Variance + d*d);
    return max(p, p_max);
}


float linstep(float min, float max, float v) {   
    return clamp((v - min) / (max - min), 0, 1); 
} 
float ReduceLightBleeding(float p_max, float Amount) {
    // Remove the [0, Amount] tail and linearly rescale (Amount, 1].
    return linstep(Amount, 1, p_max); 
}

void main() {
    vec3 albedo     = pow(texture(images, vec3(fs_in.tex_coord, 0)).rgb, vec3(2.2));
    float metallic  = texture(images, vec3(fs_in.tex_coord, 1)).b;
    float roughness = texture(images, vec3(fs_in.tex_coord, 1)).g;
    float ao        = texture(images, vec3(fs_in.tex_coord, 1)).r;

    vec3 N = normalize(texture(images, vec3(fs_in.tex_coord, 2)).xyz * 2.0 - 1.0);
    vec3 V = normalize(fs_in.V);

    // For the normal incidence, if it is a diaelectric use a F0 of 0.04, otherwise use albedo
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Transforming normal incidence to index of reflectivity

    // Formula to map roughness from [0.125,1],
    // giving a less sharp look to reflection
    float mapped_roughness = 0.125 + roughness/1.14;

    // NdotV does not depend on the light's position
    float NdotV = max(dot(N, V),0.0);
    
    // color without ambient
    vec3 rho = vec3(0.0);
    for(int i=0; i<lights.length(); i++) {
        float attenuation = (lights[i].pos.w == 0.0) ? 1.0 : get_sphere_light_attenuation(vec3(lights[i].pos), 100.0f);
        vec3 radiance = lights[i].color.rgb * attenuation;

        vec3 L = normalize(fs_in.L[i]);
        vec3 H = normalize(V + L);
        
        float NdotH = max(dot(N, H),0.0);
        float NdotL = max(dot(N, L),0.0);
        float HdotV = clamp(dot(H, V), 0.0, 1.0);
        float LdotV = max(dot(L, V),0.0);

        vec3 Ks = F_Schlick(HdotV, F0);
        vec3 Kd = (vec3(1) - Ks)*(1.0 - metallic)*albedo;

        vec3 rho_s = CookTorrance_specular(NdotL, NdotV, NdotH, HdotV, mapped_roughness, Ks);
        vec3 rho_d = OrenNayar_diffuse(LdotV, NdotL, NdotV, mapped_roughness, Kd);

        // get the moments from the texture using the normalized xy coordinatex
        vec2 moments = texture(shadow_map[nonuniformEXT(i)], fs_in.shadow_coord[i].xy/fs_in.shadow_coord[i].w).rg;

        // apply the chebyshev variance equation to give a probability that the fragment is in shadow
	    float p_max = ChebyshevUpperBound(moments, fs_in.shadow_coord[i].z);
        // We apply a fix to the light bleeding problem
        float shadow = ReduceLightBleeding(p_max, 0.6);

        rho += (rho_s + rho_d) * radiance * NdotL * shadow;
    }

    // ambient value is 0.02
    vec3 ambient = vec3(0.02) * albedo * ao;

    // final color is composed of ambient, diffuse, specular and emissive
    vec3 color = ambient + rho + vec3( texture(images,vec3(fs_in.tex_coord, 3)) );

    // gamma correction is applied in the tonemap stage
    frag_color = vec4(color, 1.0);
}
