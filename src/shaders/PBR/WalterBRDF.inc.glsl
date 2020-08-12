#include "Fresnel.inc.glsl"

float D_TrowbridgeReitzGGX(float NdotH, float alphaG) {
    float alpha2 = alphaG*alphaG;
    float denom = (NdotH*NdotH) * (alpha2 - 1.0) + 1.0;
    return alpha2 / (PI * denom * denom);
}

float smithG_GGX(float Ndotv, float alphaG) {
    return 2/(1 + sqrt(1 + alphaG*alphaG * (1-Ndotv*Ndotv)/(Ndotv*Ndotv)));
}

vec3 Walter07_specular(float NdotL, float NdotV, float NdotH, float HdotV, float roughness, float ior, vec3 Ks) {
    float roughness2 = roughness*roughness;
    float D = D_TrowbridgeReitzGGX(NdotH, roughness2);
    float G = smithG_GGX(NdotL, roughness2) * smithG_GGX(NdotV, roughness2);

    // fresnel
    float g = sqrt(ior*ior + HdotV*HdotV - 1);
    float F = 0.5 * pow(g-HdotV,2) / pow(g+HdotV,2) * (1 + pow(HdotV*(g+HdotV)-1,2) / pow(HdotV*(g-HdotV)+1,2));

    // ! I removed from the denominator NdotL, because it looks bad
    return (Ks * D * G * F) / (4 * NdotV);
}

vec3 Lambertian_diffuse(vec3 Kd) {
    return Kd / PI;
}