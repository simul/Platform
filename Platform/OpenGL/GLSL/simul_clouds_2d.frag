#version 140
#include "saturate.glsl"

#define cbuffer layout(std140) uniform
#define R0
#include "CppGlsl.hs"

uniform sampler2D imageTexture;
uniform sampler2D coverageTexture1;
uniform sampler2D coverageTexture2;
uniform sampler2D lossTexture;
uniform sampler2D inscTexture;
uniform sampler2D skylTexture;

in vec2 texc_global;
in vec2 texc_detail;
in vec3 wPosition;

#include "simul_inscatter_fns.glsl"
#include "simul_earthshadow_uniforms.glsl"

#include "../../CrossPlatform/simul_2d_clouds.hs"
#include "../../CrossPlatform/simul_2d_clouds.sl"

void main()
{
	gl_FragColor		=Clouds2DPS(texc_global,texc_detail,wPosition);
}
