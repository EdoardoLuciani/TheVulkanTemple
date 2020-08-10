#include "Fresnel.inc.glsl"

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

vec3 CookTorrance_specular(float NdotH, float HdotV, float NdotL, float roughness, float F0) {
    vec3 nominator =    D_TrowbridgeReitzGGX(NdotH, roughness) * F_CookTorrance(HdotV, 0.0), F0) *
                        G_SchlickBeckmann(NdotL, HdotV, roughness);

    float denominator = 4 * HdotV * NdotL + 0.001; // 0.001 to prevent divide by zero.
    return nominator / denominator;
}

vec3 Lambertian_diffuse(float HdotV, float F0, float metallic, vec3 albedo) {
    vec3 kS = F_CookTorrance(HdotV, 0.0), F0);
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;
    return kD * albedo / PI;
}