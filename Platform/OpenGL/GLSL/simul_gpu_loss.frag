#version 330

uniform sampler2D input_loss_texture;
uniform sampler2D density_texture;
uniform sampler2D optical_depth_texture;

#include "CppGlsl.hs"
#include "saturate.glsl"
#include "../../CrossPlatform/SL/simul_inscatter_fns.sl"
#include "../../CrossPlatform/SL/simul_gpu_sky.sl"

in vec2 texCoords;
out vec4 outColor;

void main(void)
{
    outColor			=PSLoss( input_loss_texture, density_texture, texCoords);
}
