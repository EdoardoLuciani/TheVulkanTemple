// File used to define the common inputs of all the FSR shaders

#define A_GPU 1
#define A_GLSL 1

#include "../../layers/amd_fsr/ffx_a.h"

layout(set=0,binding=0) uniform const_buffer {
    uvec4 Const0;
    uvec4 Const1;
    uvec4 Const2;
    uvec4 Const3;
    uvec4 rcas_const;
};

layout(set=0,binding=1) uniform texture2D InputTexture;

#ifdef A_HALF
    layout(set=0,binding=2,rgba16f) uniform image2D OutputTexture;
#else
    layout(set=0,binding=2,rgba32f) uniform image2D OutputTexture;
#endif
layout(set=0,binding=3) uniform sampler InputSampler;

#include "../../layers/amd_fsr/ffx_fsr1.h"