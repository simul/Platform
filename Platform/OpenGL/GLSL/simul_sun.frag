#version 140
// simul_sun.frag - a GLSL fragment shader
// Copyright 2013 Simul Software Ltd
#include "CppGlsl.hs"
#include "../../CrossPlatform/sky_constants.sl"
uniform vec4 sunlight;
// varyings are written by vert shader, interpolated, and read by frag shader.
in vec2 texCoords;
out vec4 gl_FragColor;

void main(void)
{
	vec4 result;
	// IN.tex is +- 1.
	vec2 rad		=2.0*(texCoords-vec2(0.5,0.5));
	float r=length(rad);
	if(r>1.0)
		discard;
	float u=saturate((1.0-r)/0.1);
	float brightness=sunlight.a*pow(u,6.0);
	result.rgb		=colour.rgb;
	result.a=u;
    gl_FragColor=result;
}

 