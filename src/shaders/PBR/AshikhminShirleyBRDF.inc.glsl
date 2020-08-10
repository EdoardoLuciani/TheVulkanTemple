/*
float AshikhminShirley_specular(float nu, float nv, float NdotH, float HdotV, float NdotL, float NdotV, float HdotT, float HdotB) {
    //float n = (nu*(HdotT*HdotT) + nv*(HdotB*HdotB))/(1-(NdotH*NdotH));
    bool isotropic = true;
    float n = isotropic ? nu :(nu*(HdotT*HdotT) + nv*(HdotB*HdotB)) /(1-(NdotH*NdotH));
    float term_1 = sqrt((nu+1)*(nv+1)) / (8*PI);
    float term_2 = pow(NdotH,n) / (HdotV*max(NdotL, NdotV));
    return term_1 * term_2;
}

vec3 AshikhminShirley_diffuse(vec3 Rd, vec3 Rs, float NdotL, float NdotV) {
    return ((28*Rd)/(23*PI)) * (1-Rs) * (1-pow(1-NdotV/2, 5)) * (1-pow(1-NdotL/2, 5));
}
*/
#include "Fresnel.inc.glsl"

float sqr( float x )
{
    return x*x;
}

float AshikhminShirley_specular(float nu, float nv, float NdotH, float HdotV, float NdotL, float NdotV, float HdotT, float HdotB) {
    bool isotropic = true;
    float norm_s = sqrt((nu+1)*((isotropic?nu:nv)+1))/(8*PI);
    float n = isotropic ? nu :(nu*sqr(HdotT) + nv*sqr(HdotB))/(1-sqr(NdotH));
    float rho_s = norm_s * pow(max(NdotH,0), n) / (HdotV * max(NdotV, NdotL));
    return rho_s;
}

vec3 AshikhminShirley_diffuse(vec3 Rd, vec3 Rs, float NdotL, float NdotV) {
    vec3 rho_d = 28/(23*PI) * Rd * (1-pow(1-(NdotV/2), 5)) * (1-pow(1-(NdotL/2), 5));
    rho_d *= (1-Rs);
    return rho_d;
}