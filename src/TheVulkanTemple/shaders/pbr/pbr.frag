#version 460
#extension GL_EXT_nonuniform_qualifier : require
#include "../BRDF.inc.glsl"
#include "../light.inc.glsl"
#include "../lighting_helper.inc.glsl"

layout (set = 0, binding = 1) uniform sampler2DArray images;

layout (set = 1, binding = 0) readonly buffer uniform_buffer2 {
    LightParams lights[];
};
layout (set = 1, binding = 1) uniform sampler2D shadow_maps[];

#define MAX_LIGHT_DATA 8
layout (location = 0) in VS_OUT {
    vec3 position;
    vec2 tex_coord;
    vec3 N_g;
    vec3 V;
    vec3 L[MAX_LIGHT_DATA];
    vec3 L_world[MAX_LIGHT_DATA];
    vec4 shadow_coord[MAX_LIGHT_DATA];
} fs_in;

layout (location = 0) out vec4 frag_color;
layout (location = 1) out vec4 normal_g_image;

void main() {
    vec3 albedo     = pow(texture(images, vec3(fs_in.tex_coord, 0)).rgb, vec3(2.2));
    float roughness = texture(images, vec3(fs_in.tex_coord, 1)).g;
    float metallic  = texture(images, vec3(fs_in.tex_coord, 1)).b;

    vec3 N = normalize(texture(images, vec3(fs_in.tex_coord, 2)).xyz * 2.0 - 1.0);
    vec3 V = normalize(fs_in.V);

    // For the normal incidence, if it is a diaelectric use a F0 of 0.04, otherwise use albedo
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float corrected_roughness = roughness * roughness;

    // NdotV does not depend on the light's position
    float nc_NdotV = dot(N,V);
    float NdotV = clamp(nc_NdotV, 1e-5, 1.0);
    
    // color without ambient
    vec3 rho = vec3(0.0);
    for(int i=0; i<lights.length(); i++) {
        vec3 L = normalize(fs_in.L[i]);
        vec3 H = normalize(V + L);

        float nc_NdotL = dot(N, L);
        float NdotL = clamp(nc_NdotL, 0.0, 1.0);
        float NdotH = clamp(dot(N, H), 0.0, 1.0);
        float LdotV = clamp(dot(L, V), 0.0, 1.0);
        float LdotH = clamp(dot(L, H), 0.0, 1.0);

        vec3 Ks = F_Schlick(F0, LdotH);
        vec3 Kd = (1.0 - metallic)*albedo;

        vec3 rho_s = CookTorrance_specular(NdotL, NdotV, NdotH, corrected_roughness, Ks);
        vec3 rho_d = Kd * Burley_diffuse_local_sss(corrected_roughness, NdotV, nc_NdotV, nc_NdotL, LdotH, 0.4);

        float shadow = 1.0;
        if (bool(lights[i].shadow_map_index + 1)) {
            shadow = get_shadow_component(lights[i], shadow_maps[nonuniformEXT(lights[i].shadow_map_index)],
                                                fs_in.shadow_coord[i].xy/fs_in.shadow_coord[i].w, fs_in.shadow_coord[i].z);
        }

        vec3 radiance = get_light_radiance(lights[i], fs_in.position, normalize(fs_in.L_world[i]));
        rho += (rho_s + rho_d) * radiance * NdotL * shadow;
    }

    // ambient value is 0.02
    vec3 ambient = vec3(0.02) * albedo;

    // final color is composed of ambient, diffuse, specular and emissive
    vec3 color = ambient + rho + vec3(texture(images,vec3(fs_in.tex_coord, 3)));

    // gamma correction is applied in the tonemap stage
    frag_color = vec4(color, 1.0);
    //frag_color = vec4(ambient * vec3(30.0f), 1.0);

    vec3 normal_to_write = (abs(normalize(fs_in.N_g)) + 1) / 2;
    normal_g_image = vec4(normal_to_write, 0.0);
}
