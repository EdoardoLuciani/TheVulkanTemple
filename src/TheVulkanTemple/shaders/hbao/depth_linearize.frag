/* Copyright (c) 2014-2018, NVIDIA CORPORATION. All rights reserved.
 * https://github.com/nvpro-samples/gl_ssao/blob/master/depthlinearize.frag.glsl
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

layout (push_constant) uniform uniform_buffer {
  vec4 clip_info; // z_n * z_f,  z_n - z_f,  z_f, perspective = 1 : 0
};

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput depth_buffer;

layout(location = 0) out float out_color;

float reconstructCSZ(float d, vec4 clip_info) {
  /*
    if (in_perspective == 1.0) { // perspective
        ze = (zNear * zFar) / (zFar - zb * (zFar - zNear));
    }
    else { // orthographic proj
        ze  = zNear + zb  * (zFar - zNear);
    }
  */
  if (clip_info[3] != 0) {
    return (clip_info[0] / (clip_info[1] * d + clip_info[2]));
  }
  else {
    return (clip_info[1]+clip_info[2] - d * clip_info[1]);
  }
}

void main() {
  float depth = subpassLoad(depth_buffer).r;
  out_color = reconstructCSZ(depth, clip_info);
}
