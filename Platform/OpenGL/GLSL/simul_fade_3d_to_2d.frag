#version 140
#include "CppGlsl.hs"
#include "../../CrossPlatform/SL/sky_constants.sl"
// GLSL fragment shader
// Copyright 2014 Simul Software Ltd

uniform sampler3D fadeTexture1;
uniform sampler3D fadeTexture2;

// varyings are written by vert shader, interpolated, and read by frag shader.
in vec2 texCoords;
out vec4 gl_FragColor;

void main()
{
	vec3 texcoord	=vec3(altitudeTexCoord,texCoords.yx);
	vec4 texel1=texture3D(fadeTexture1,texcoord);
	vec4 texel2=texture3D(fadeTexture2,texcoord);
	vec4 texel=mix(texel1,texel2,skyInterp);
    gl_FragColor=texel;
}
