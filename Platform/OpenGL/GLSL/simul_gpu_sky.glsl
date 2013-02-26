#ifndef SIMUL_GPU_SKY_GLSL
#define SIMUL_GPU_SKY_GLSL
#ifndef __cplusplus
#include "simul_inscatter_fns.glsl"
#endif
#define ALIGN
#define cbuffer layout(std140) uniform
#define R0
#ifndef __cplusplus
uniform sampler2D optical_depth_texture;
#endif
#define texture_clamp texture
#include "../../CrossPlatform/simul_gpu_sky.sl"
#endif