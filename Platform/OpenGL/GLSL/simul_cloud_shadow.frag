// simul_sky.frag - an OGLSL fragment shader
// Copyright 2008 Simul Software Ltd

uniform sampler3D cloudTexture1;
uniform sampler3D cloudTexture2;
uniform float interp;

// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec2 texCoords;

void main()
{
	vec3 texcoord=vec3(texCoords.xy,0.0);
	vec4 texel1=texture3D(cloudTexture1,texcoord);
	vec4 texel2=texture3D(cloudTexture2,texcoord);
	vec4 texel=mix(texel1,texel2,interp);
    gl_FragColor=texel;
}
