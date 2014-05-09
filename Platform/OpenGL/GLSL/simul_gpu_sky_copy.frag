#version 330

uniform sampler3D source_texture;

#include "CppGlsl.hs"
#include "saturate.glsl"
#include "../../CrossPlatform/SL/simul_inscatter_fns.sl"
#include "../../CrossPlatform/SL/simul_gpu_sky.sl"

uniform float tz;
in vec2 texCoords;
out vec4 outColor;

void main(void)
{
	outColor			=texture(source_texture,vec3(texCoords.xy,tz));

}
