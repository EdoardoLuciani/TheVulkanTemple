#version 440

layout (set = 0, binding = 0) uniform uniform_buffer1 {
	mat4 model;
	mat4 normal_model;
};
layout (set = 0, binding = 1) uniform sampler2D image[4];
layout (set = 0, binding = 2) uniform sampler2D shadow_map;

layout (set = 1, binding = 0) uniform uniform_buffer2 {
	mat4 view;
	mat4 inverse_view;
	mat4 projection;
	vec4 camera_pos;
};

layout (set = 2, binding = 0) uniform uniform_buffer3 {
    mat4 camera_v;
    mat4 camera_p;
	vec4 light_pos[1];
	vec4 light_color;
};

layout (location = 0) in VS_OUT {
	vec3 position;
	vec2 tex_coord;
	vec3 normal;
    vec4 shadow_coord;
} fs_in;


layout (location = 0) out vec4 frag_color;

const float PI = 3.14159265358979323846;

vec3 getNormalFromMap() {
    vec3 tangentNormal = texture(image[2], fs_in.tex_coord).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(fs_in.position);
    vec3 Q2  = dFdy(fs_in.position);
    vec2 st1 = dFdx(fs_in.tex_coord);
    vec2 st2 = dFdy(fs_in.tex_coord);

    vec3 N = normalize(fs_in.normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

float DistributionGGX(float NdotH, float roughness) {
    float alpha2 = roughness*roughness*roughness*roughness;
    float denom = (NdotH*NdotH) * (alpha2 - 1.0) + 1.0;
    return alpha2 / (PI * denom * denom);
}
// ----------------------------------------------------------------------------
float GeometrySmith(float NdotV, float roughness) {
    float k = ((roughness + 1.0)*(roughness + 1.0)) / 8.0;
    float denom = NdotV * (1.0 - k) + k;

    return NdotV / denom;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float HdotV, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - HdotV, 5.0);
}
// ----------------------------------------------------------------------------
float ChebyshevUpperBound(vec2 Moments, float t) {
    // One-tailed inequality valid if t > Moments.x 
    if (t <= Moments.x) {
	    return 1.0;
    }
    // Compute variance.
    float Variance = Moments.y - (Moments.x*Moments.x);
    Variance = max(Variance, 0.002);
    // Compute probabilistic upper bound.    
    float d = t - Moments.x;
    float p_max = Variance / (Variance + d*d);
    return p_max;
}

float linstep(float min, float max, float v) {   
    return clamp((v - min) / (max - min), 0, 1); 
} 

float ReduceLightBleeding(float p_max, float Amount) {
    // Remove the [0, Amount] tail and linearly rescale (Amount, 1].
    return linstep(Amount, 1, p_max); 
}

void main() {		
    vec3 albedo     = texture(image[0], fs_in.tex_coord).rgb;
    float metallic  = texture(image[1], fs_in.tex_coord).b;
    float roughness = texture(image[1], fs_in.tex_coord).g;
    float ao        = texture(image[1], fs_in.tex_coord).r;

    vec3 N = getNormalFromMap();
    vec3 V = normalize(vec3(camera_pos) - fs_in.position);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0);

    for(int i=0;i<1;i++) {

        float attenuation = 1.0;
        vec3 L = vec3(0.0);
        if (light_pos[i].w == 0.0) {
            L = normalize(vec3(light_pos[i]));
        }
        else {
            L = normalize(vec3(light_pos[i]) - fs_in.position);
            float dist = length(vec3(light_pos[i]) - fs_in.position);
            attenuation = 1.0 / (dist * dist);
        }
        vec3 radiance = light_color.rgb * attenuation;
        vec3 H = normalize(V + L);

        // Cook-Torrance BRDF        
        vec3 nominator =    DistributionGGX(max(dot(N, H),0.0), roughness) * fresnelSchlick(max(dot(H, V), 0.0), F0) *
                            GeometrySmith(max(dot(N, L),0.0), roughness) * GeometrySmith(max(dot(N, V),0.0), roughness);

        float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001; // 0.001 to prevent divide by zero.
        vec3 specular = nominator / denominator;
        
        // kS is equal to Fresnel
        vec3 kS = fresnelSchlick(max(dot(H, V), 0.0), F0);
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
        vec3 kD = vec3(1.0) - kS;
        // multiply kD by the inverse metalness such that only non-metals 
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kD *= 1.0 - metallic;	  

        // scale light by NdotL
        float NdotL = max(dot(N, L), 0.0);
        
        // calculate if fragment is in shadow
        float shadow = 1.0f;
        vec4 modified_shadow_coords = fs_in.shadow_coord;
        modified_shadow_coords.z -= 0.05;
        vec4 normalized_shadow_coords = modified_shadow_coords / modified_shadow_coords.w;
        if (modified_shadow_coords.z >= 0) {
            vec2 moments = texture(shadow_map,normalized_shadow_coords.xy).rg;
	        float p_max = ChebyshevUpperBound(moments, normalized_shadow_coords.z);
            shadow = ReduceLightBleeding(p_max, 0.99999);
        }

        // add to outgoing radiance Lo
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * shadow;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again   
    }

    //note: default for ambient is 0.03
    vec3 ambient = vec3(0.03) * albedo * ao;
    
    vec3 color = ambient + Lo + vec3(texture(image[3],fs_in.tex_coord));
    color = pow(color, vec3(1.0/2.2)); 

    frag_color = vec4(color, 1.0);
}
