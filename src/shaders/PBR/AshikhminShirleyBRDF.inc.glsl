#include "Fresnel.inc.glsl"

float AshikhminShirley_specular(float nu, float nv, float NdotH, float HdotV, float NdotL, float NdotV, float HdotT, float HdotB) {
    float n = nu;
    float term_1 = sqrt((nu+1)*(nv+1)) / (8*PI);
    float term_2 = pow(NdotH,n) / (HdotV*max(NdotL, NdotV));
    return term_1 * term_2;
}

vec3 AshikhminShirley_diffuse(vec3 Rd, vec3 Rs, float NdotL, float NdotV) {
    return ((28*Rd)/(23*PI)) * (1-Rs) * (1-pow(1-NdotV/2, 5)) * (1-pow(1-NdotL/2, 5));
}

