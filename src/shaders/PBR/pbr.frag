#version 440

layout (set = 0, binding = 0) uniform uniform_buffer1 {
	mat4 model;
	mat4 normal_model;
};
layout (set = 0, binding = 1) uniform sampler2D image[4];

layout (set = 1, binding = 0) uniform uniform_buffer2 {
	mat4 view;
	mat4 inverse_view;
	mat4 projection;
	vec4 camera_pos;
};

layout (set = 2, binding = 0) uniform uniform_buffer3 {
	vec4 light_pos[1];
	vec4 light_color;
};

layout (location = 0) in VS_OUT {
	vec3 position;
	vec2 tex_coord;
	vec3 normal;
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


float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
// ----------------------------------------------------------------------------
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
        float D = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
           
        vec3 nominator    = D * G * F; 
        float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001; // 0.001 to prevent divide by zero.
        vec3 specular = nominator / denominator;
        
        // kS is equal to Fresnel
        vec3 kS = F;
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

        // add to outgoing radiance Lo
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again   
    }
    // ambient lighting (note that the next IBL tutorial will replace 
    // this ambient lighting with environment lighting).

    //note: default for ambient is 0.03
    vec3 ambient = vec3(0.03) * albedo * ao;
    
    vec3 color = ambient + Lo + vec3(texture(image[3],fs_in.tex_coord));
    color = pow(color, vec3(1.0/2.2)); 

    frag_color = vec4(color, 1.0);
}
