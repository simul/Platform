// simul_sky.frag - an OGLSL fragment shader
// Copyright 2008-2012 Simul Software Ltd
#version 140
#include "simul_inscatter_fns.glsl"

uniform sampler2D inscTexture;
uniform sampler2D skylightTexture;
uniform vec3 lightDir;

#include "simul_earthshadow_uniforms.glsl"

// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec3 dir;

void main()
{
	vec3 view=normalize(dir);
	float sine=view.z;
	// Full brightness:
	vec2 texc2=vec2(1.0,0.5*(1.0-sine));
	vec4 insc=EarthShadowFunction(texc2,view);
	vec4 skyl=texture2D(skylightTexture,texc2);
	float cos0=dot(view,lightDir);
	vec3 colour=InscatterFunction(insc,cos0);
	colour+=skyl.rgb;
    gl_FragColor=vec4(colour,1.0);
}
