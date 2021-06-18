#define PI 3.14159265359
#define MEDIUMP_FLT_MAX 65504.0
#define MEDIUMP_FLT_MIN 0.00006103515625
#define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)

float D_GGX(float roughness, float NdotH) {
    // Walter et al. 2007, "Microfacet Models for Refraction through Rough Surfaces"
    float oneMinusNoHSquared = 1.0 - NdotH * NdotH;

    float a = NdotH * roughness;
    float k = roughness / (oneMinusNoHSquared + a * a);
    float d = k * k * (1.0 / PI);
    return d;
}

float V_SmithGGXCorrelated(float roughness, float NdotV, float NdotL) {
    // Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
    float a2 = roughness * roughness;
    float lambdaV = NdotL * sqrt((NdotV - a2 * NdotV) * NdotV + a2);
    float lambdaL = NdotV * sqrt((NdotL - a2 * NdotL) * NdotL + a2);
    float v = 0.5 / (lambdaV + lambdaL);
    return v;
}

float V_SmithGGXCorrelated_fast(float roughness, float NdotV, float NdotL) {
    // Hammon, 2017, "PBR Diffuse Lighting for GGX+Smith Microsurfaces"
    float v = 0.5 / mix(2*NdotL * NdotV, NdotL + NdotV, roughness);
    return v;
}

vec3 F_Schlick(vec3 F0, float HdotV) {
    return F0 + (1.0 - F0) * pow(1.0 - HdotV, 5.0);
}

vec3 F_Schlick(vec3 F0, float F90, float HdotV) {
    // Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"
    return F0 + (F90 - F0) * pow(1.0 - HdotV, 5.0);
}

float F_Schlick(float F0, float F90, float VdotH) {
    return F0 + (F90 - F0) * pow(1.0 - VdotH, 5.0);
}

vec3 CookTorrance_specular(float NdotL, float NdotV, float NdotH, float roughness, vec3 F) {
    float D = D_GGX(roughness, NdotH);
    float G = V_SmithGGXCorrelated_fast(roughness, NdotV, NdotL);

    return (D * G) * F;
}

/*
The MIT License (MIT)

Copyright (c) 2014 Mikola Lysenko

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
vec3 OrenNayar_diffuse(float LdotV, float NdotL, float NdotV, float roughness, vec3 Kd) {
  float s = LdotV - NdotL * NdotV;
  float t = mix(1.0, max(NdotL, NdotV), step(0.0, s));

  float sigma2 = roughness * roughness;
  vec3 A = 1.0 + sigma2 * (Kd / (sigma2 + 0.13) + 0.5 / (sigma2 + 0.33));
  float B = 0.45 * sigma2 / (sigma2 + 0.09);

  return NdotL * (A + B * s / t) / PI;
}

float Burley_diffuse(float roughness, float NdotV, float NdotL, float LdotH) {
    // Burley 2012, "Physically-Based Shading at Disney"
    float f90 = 0.5 + 2.0 * roughness * LdotH * LdotH;
    float lightScatter = F_Schlick(1.0, f90, NdotL);
    float viewScatter  = F_Schlick(1.0, f90, NdotV);
    return lightScatter * viewScatter * (1.0 / PI);
}

float Lambertian_diffuse() {
    return 1.0 / PI;
}