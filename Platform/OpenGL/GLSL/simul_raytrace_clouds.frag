#version 140

#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "CppGlsl.hs"
#include "../../CrossPlatform/simul_cloud_constants.sl"
#include "saturate.glsl"

varying vec2 texc;

void main()
{
	vec4 colour=vec4(1.0,0.0,0.0,0.001);
	gl_FragColor=colour;
}
