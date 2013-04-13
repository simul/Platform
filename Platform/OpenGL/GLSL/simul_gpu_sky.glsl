#ifndef SIMUL_GPU_SKY_GLSL
#define SIMUL_GPU_SKY_GLSL
#ifndef __cplusplus
#include "simul_inscatter_fns.glsl"
#endif
#include "../Glsl.h"
#ifndef __cplusplus
uniform sampler2D optical_depth_texture;
#endif
#define texture_clamp texture
#include "saturate.glsl"
#include "../../CrossPlatform/simul_gpu_sky.sl"
#endif