#ifndef SIMUL_GPU_SKY_GLSL
#define SIMUL_GPU_SKY_GLSL
#ifndef __cplusplus
#include "../../CrossPlatform/simul_inscatter_fns.sl"
#endif
#include "CppGlsl.hs"
#ifndef __cplusplus
uniform sampler2D optical_depth_texture;
#endif
#include "saturate.glsl"
#include "../../CrossPlatform/simul_gpu_sky.sl"
#endif