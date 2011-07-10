// simul_sky.frag - an OGLSL fragment shader
// Copyright 2008 Simul Software Ltd

uniform sampler3D fadeTexture1;
uniform sampler3D fadeTexture2;
uniform float skyInterp;
uniform float altitudeTexCoord;

// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec2 texc;

void main()
{
	vec3 texcoord=vec3(texc,altitudeTexCoord);
	vec4 texel1=texture3D(fadeTexture1,texcoord);
	vec4 texel2=texture3D(fadeTexture2,texcoord);
	vec4 texel=mix(texel1,texel2,skyInterp);
    gl_FragColor=texel;
}
