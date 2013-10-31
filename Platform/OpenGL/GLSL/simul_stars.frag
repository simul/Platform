// simul_stars.frag - a GLSL fragment shader
// Copyright 2011 Simul Software Ltd
#version 140
#include "CppGlsl.hs"
#include "../../CrossPlatform/sky_constants.sl"

in vec2 texCoords;
in vec3 pos;
out vec4 gl_FragColor;

void main(void)
{
	vec3 colour=vec3(1.0,1.0,1.0)*clamp(starBrightness*texCoords.x,0.0,1.0);
    gl_FragColor=vec4(colour,1.0);
}

 