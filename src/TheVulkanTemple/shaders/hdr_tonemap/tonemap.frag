#version 460
#include "../ColorSpaces.inc.glsl"
//#extension GL_KHR_vulkan_glsl: enable

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput input_color;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput global_ao;

layout (location = 0) out vec4 out_color;

float Tonemap_Lottes(float x) {
    // Lottes 2016, "Advanced Techniques and Optimization of HDR Color Pipelines"
    const float a = 1.6;
    const float d = 0.977;
    const float hdrMax = 8.0;
    const float midIn = 0.18;
    const float midOut = 0.267;

    // Can be precomputed
    const float b =
    (-pow(midIn, a) + pow(hdrMax, a) * midOut) /
    ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);
    const float c =
    (pow(hdrMax, a * d) * pow(midIn, a) - pow(hdrMax, a) * pow(midIn, a * d) * midOut) /
    ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);

    return pow(x, a) / (pow(x, a * d) * b + c);
}

float Tonemap_Uchimura(float x, float P, float a, float m, float l, float c, float b) {
    // Uchimura 2017, "HDR theory and practice"
    // Math: https://www.desmos.com/calculator/gslcdxvipg
    // Source: https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp
    float l0 = ((P - m) * l) / a;
    float L0 = m - m / a;
    float L1 = m + (1.0 - m) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float CP = -C2 / P;

    float w0 = 1.0 - smoothstep(0.0, m, x);
    float w2 = step(m + l0, x);
    float w1 = 1.0 - w0 - w2;

    float T = m * pow(x / m, c) + b;
    float S = P - (P - S1) * exp(CP * (x - S0));
    float L = m + a * (x - m);

    return T * w0 + L * w1 + S * w2;
}
float Tonemap_Uchimura(float x) {
    const float P = 1.0;  // max display brightness
    const float a = 1.0;  // contrast
    const float m = 0.22; // linear section start
    const float l = 0.4;  // linear section length
    const float c = 1.33; // black
    const float b = 0.0;  // pedestal
    return Tonemap_Uchimura(x, P, a, m, l, c, b);
}

vec3 rtt_and_odt_fit(vec3 v) {
    vec3 a = v * (v + 0.0245786f) - 0.000090537f;
    vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}
vec3 aces_fitted(vec3 v) {
    mat3 aces_input_matrix = mat3(
    0.59719f, 0.35458f, 0.04823f,
    0.07600f, 0.90834f, 0.01566f,
    0.02840f, 0.13383f, 0.83777f
    );

    mat3 aces_output_matrix = mat3(
    1.60475f, -0.53108f, -0.07367f,
    -0.10208f,  1.10813f, -0.00605f,
    -0.00327f, -0.07276f,  1.07602f
    );

    v = aces_input_matrix * v;
    v = rtt_and_odt_fit(v);
    v = aces_output_matrix * v;
    return v;
}

float ACESFilm(float x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp(((x*(a*x+b))/(x*(c*x+d)+e)),0.0,1.0);
}

void main() {
    float ao = subpassLoad(global_ao).r;
    vec3 color = vec3(subpassLoad(input_color)) * ao;

    vec3 xyY = rgb_to_xyY(color);

    //xyY.z = ACESFilm(xyY.z);
    //xyY.z = aces_fitted(vec3(xyY.z)).b;
    xyY.z = Tonemap_Uchimura(xyY.z);
    //xyY.z =  Tonemap_Lottes(xyY.z);

    // Using the new luminance, convert back to RGB and to sRGB
    out_color = vec4(rgb_to_srgb_approx(xyY_to_rgb(xyY)), 1.0f);

    // debug mode!!!
    //out_color = subpassLoad(global_ao);
}