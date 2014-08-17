#version 330
#include "CppGlsl.hs"
#include "../../CrossPlatform/SL/simul_inscatter_fns.sl"
#include "simul_gpu_sky.glsl"

uniform sampler2D input_insc_texture;
uniform sampler2D density_texture;
uniform sampler3D loss_texture;

in vec2 texCoords;
out vec4 outColor;

void main(void)
{
	vec4 res=Insc(input_insc_texture,loss_texture,density_texture,optical_depth_texture,texCoords);
	outColor=res;
}
