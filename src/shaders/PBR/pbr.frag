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

// Distribution function(s)
float D_TrowbridgeReitzGGX(float NdotH, float roughness) {
    float alpha2 = roughness*roughness*roughness*roughness;
    float denom = (NdotH*NdotH) * (alpha2 - 1.0) + 1.0;
    return alpha2 / (PI * denom * denom);
}

float D_Beckmann(float x, float roughness) {
  float NdotH = max(x, 0.0001);
  float cos2Alpha = NdotH * NdotH;
  float tan2Alpha = (cos2Alpha - 1.0) / cos2Alpha;
  float roughness2 = roughness * roughness;
  float denom = PI * roughness2 * cos2Alpha * cos2Alpha;
  return exp(tan2Alpha / roughness2) / denom;
}

float D_BlinnPhong(float NdotH, float roughness) {
    float alpha2 = roughness*roughness*roughness*roughness;
    float nom = pow(NdotH, (2 / alpha2) - 2);
    return nom / (PI * alpha2);
}


// Geometry function(s)
float G_Neumann(float NdotL,float NdotV, float roughness) {
    return (NdotL*NdotV)/max(NdotL,NdotV);
}

float G_Kelemen(float NdotL,float NdotV, float HdotV, float roughness) {
    return (NdotL*NdotV)/pow(HdotV,2);
}

float G_Smith_c(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}
float G_Smith(float NdotL, float NdotV, float roughness) {
    float ggx2 = G_Smith_c(NdotV, roughness);
    float ggx1 = G_Smith_c(NdotL, roughness);
    return ggx1 * ggx2;
}

float G_Beckmann_c(float NdotV, float alpha) {
    float c = NdotV / (alpha*sqrt(1-(NdotV*NdotV)));
    float result = 1;
    if (c < 1.6) {
        result = (3.535*c+2.181*c*c)/(1+2.276*c+2.577*c*c);
    }
    return result;
}
float G_Beckmann(float NdotL, float NdotV, float roughness) {
    float alpha = roughness*roughness;
    float ggx2 = G_Beckmann_c(NdotV, alpha);
    float ggx1 = G_Beckmann_c(NdotL, alpha);
    return ggx1 * ggx2;
}


float G_GGX_c(float NdotV, float alpha2) {
    float denom = NdotV + sqrt(alpha2+(1-alpha2)*NdotV*NdotV);
    return (2*NdotV) / denom;
}
float G_GGX(float NdotL, float NdotV, float roughness) {
    float alpha2 = roughness*roughness*roughness*roughness;
    float ggx2 = G_GGX_c(NdotV, alpha2);
    float ggx1 = G_GGX_c(NdotL, alpha2);
    return ggx1 * ggx2;
}

float G_SchlickBeckmann_c(float NdotV, float k) {
    return NdotV / ( NdotV * (1-k) + k);
}
float G_SchlickBeckmann(float NdotL, float NdotV, float roughness) {
    float alpha = roughness*roughness;
    float k = alpha*sqrt(2.0/PI);
    float ggx2 = G_SchlickBeckmann_c(NdotV, k);
    float ggx1 = G_SchlickBeckmann_c(NdotL, k);
    return ggx1 * ggx2;
}


// Fresnel function(s)
vec3 F_Schlick(float HdotV, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - HdotV, 5.0);
}

vec3 F_CookTorrance(float HdotV, vec3 F0) {
    vec3 n = (1 + sqrt(F0)) / (1 - sqrt(F0));
    vec3 g = sqrt( (n*n) + pow(HdotV, 2) - 1);
    return 0.5 * ((g-HdotV)/(g+HdotV)*(g-HdotV)/(g+HdotV)) *
                (1 + ( ((g+HdotV)*HdotV-1) / ((g-HdotV)*HdotV+1)*((g+HdotV)*HdotV-1) / ((g-HdotV)*HdotV+1)) );
}


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
        vec3 nominator =    D_TrowbridgeReitzGGX(max(dot(N, H),0.0), roughness) * F_CookTorrance(max(dot(H, V), 0.0), F0) *
                            G_SchlickBeckmann(max(dot(N, L),0.0),max(dot(N, V),0.0),roughness);
                            //G_Kelemen(max(dot(N, L),0.0),max(dot(N, V),0.0),max(dot(H, V),0.0), roughness);

        float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001; // 0.001 to prevent divide by zero.
        vec3 specular = nominator / denominator;
        
        // kS is equal to Fresnel
        vec3 kS = F_CookTorrance(max(dot(H, V), 0.0), F0);
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
