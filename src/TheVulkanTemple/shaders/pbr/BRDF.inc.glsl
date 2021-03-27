float D_GGX(float NdotH, float alphaG) {
    float alpha2 = alphaG*alphaG;
    float denom = (NdotH*NdotH) * (alpha2 - 1.0) + 1.0;
    return alpha2 / (PI * denom * denom);
}

float G_GGX(float NdotV, float alphaG) {
    return 2/(1 + sqrt(1 + alphaG*alphaG * (1-NdotV*NdotV)/(NdotV*NdotV)));
}

float F_Walter(float HdotV, float ior) {
    float g = sqrt(ior*ior -1 + HdotV*HdotV);
    return 0.5 * pow(g-HdotV,2) / pow(g+HdotV,2) * (1 + pow(HdotV*(g+HdotV)-1,2) / pow(HdotV*(g-HdotV)+1,2));
}

vec3 F_Schlick(float HdotV, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - HdotV, 5.0);
}

vec3 CookTorrance_specular(float NdotL, float NdotV, float NdotH, float HdotV, float roughness, vec3 F) {
    float roughness2 = roughness*roughness;
    float D = D_GGX(NdotH, roughness2);
    float G = G_GGX(NdotL, roughness2) * G_GGX(NdotV, roughness2);

    // I removed from the denominator NdotL!, because it looks bad
    return (D * G * F) / max(4 * NdotV, 0.001);
    //return vec3(ior,HdotV,F);
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

  return Kd * NdotL * (A + B * s / t) / PI;
}