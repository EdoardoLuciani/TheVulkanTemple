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
	vec3 tangent;
	vec3 bitangent;
} fs_in;
layout (location = 0) out vec4 frag_color;

const float PI = 3.14159265358979323846;

float AshikhminShirley_specular(float nu, float nv, float NdotH, float HdotV, float NdotL, float NdotV, float HdotT, float HdotB) {
    //float n = (nu*(HdotT*HdotT) + nv*(HdotB*HdotB))/(1-(NdotH*NdotH));
    bool isotropic = true;
    float n = isotropic ? nu :(nu*(HdotT*HdotT) + nv*(HdotB*HdotB)) /(1-(NdotH*NdotH));
    float term_1 = sqrt((nu+1)*(nv+1)) / (8*PI);
    float term_2 = pow(NdotH,n) / (HdotV*max(NdotL, NdotV));
    return term_1 * term_2;
}

vec3 AshikhminShirley_diffuse(vec3 Rd, vec3 Rs, float NdotL, float NdotV) {
    return 28/(23*PI) * Rd * (1-Rs) * (1-pow(1-NdotV/2, 5)) * (1-pow(1-NdotL/2, 5));
}

// Fresnel function
vec3 F_Schlick(float HdotV, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - HdotV, 5.0);
}

float F_Schlick(float HdotV, float F0) {
    return F0 + (1.0 - F0) * pow(1.0 - HdotV, 5.0);
}

// Utility function(s)
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

// Shadow Helping functions
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

    // TODO: port this here with also the getNormalFromMap function in pbr.vert
    vec3 T  = fs_in.tangent;
    vec3 B  = fs_in.bitangent;

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    vec3 rho = vec3(0.0);

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
        
        float NdotH = max(dot(N, H),0.0);
        float NdotV = max(dot(N, V),0.0);
        float NdotL = max(dot(N, L),0.0);

        float HdotV = max(dot(H, V),0.0);
        float HdotT = max(dot(H, T),0.0);
        float HdotB = max(dot(H, B),0.0);

        // Ashikhman Shirley BRDF
        vec3 rho_s = AshikhminShirley_specular(10.0f, 100.0f, NdotH, HdotV, NdotL, NdotV, HdotT, HdotB) * F_Schlick(HdotV, F0);
        vec3 rho_d = AshikhminShirley_diffuse(albedo, F0, NdotL, NdotV);
        
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
        rho = (rho_d + rho_s);
    }

    //note: default for ambient is 0.03
    vec3 ambient = vec3(0.03) * albedo * ao;
    
    vec3 color = ambient + rho + vec3(texture(image[3],fs_in.tex_coord));
    color = pow(color, vec3(1.0/2.2)); 

    frag_color = vec4(color, 1.0);
}
