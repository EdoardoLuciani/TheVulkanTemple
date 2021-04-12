/* Copyright (c) 2014-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#version 460

struct HBAOData {
    float   RadiusToScreen;        // radius
    float   R2;     // 1/radius
    float   NegInvR2;     // radius * radius
    float   NDotVBias;

    vec2    InvFullResolution;
    vec2    InvQuarterResolution;

    float   AOMultiplier;
    float   PowExponent;
    vec2    _pad0;

    vec4    projInfo;
    vec2    projScale;
    int     projOrtho;
    int     _pad1;

    vec4    float2Offsets[16];
    vec4    jitters[16];
};

layout(set = 0, binding = 0) uniform controlBuffer {
    HBAOData control;
};

layout(set = 0, binding = 1) uniform sampler2D texLinearDepth;

layout(location = 0) out vec4 out_Color;

//----------------------------------------------------------------------------------

vec3 UVToView(vec2 uv, float eye_z)
{
    return vec3((uv * control.projInfo.xy + control.projInfo.zw) * (control.projOrtho != 0 ? 1. : eye_z), eye_z);
}

vec3 FetchViewPos(vec2 UV)
{
    float ViewDepth = texture(texLinearDepth,UV).x;
    return UVToView(UV, ViewDepth);
}

vec3 MinDiff(vec3 P, vec3 Pr, vec3 Pl)
{
    vec3 V1 = Pr - P;
    vec3 V2 = P - Pl;
    return (dot(V1,V1) < dot(V2,V2)) ? V1 : V2;
}

vec3 ReconstructNormal(vec2 UV, vec3 P)
{
    vec3 Pr = FetchViewPos(UV + vec2(control.InvFullResolution.x, 0));
    vec3 Pl = FetchViewPos(UV + vec2(-control.InvFullResolution.x, 0));
    vec3 Pt = FetchViewPos(UV + vec2(0, control.InvFullResolution.y));
    vec3 Pb = FetchViewPos(UV + vec2(0, -control.InvFullResolution.y));
    return normalize(cross(MinDiff(P, Pr, Pl), MinDiff(P, Pt, Pb)));
}

//----------------------------------------------------------------------------------

void main() {
    vec2 texCoord = gl_FragCoord.xy/textureSize(texLinearDepth, 0);

    vec3 P  = FetchViewPos(texCoord);
    vec3 N  = ReconstructNormal(texCoord, P);

    out_Color = vec4(N*0.5 + 0.5,0);
}
