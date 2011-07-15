// simul_sky.frag - a GLSL fragment shader
// Copyright 2011 Simul Software Ltd
uniform sampler2D flareTexture;
uniform vec3 flareColour;
// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec2 texcoord;

void main(void)
{
	vec4 result=texture2D(flareTexture,texcoord);
	result.rgb*=flareColour;
	result.a=1.0;
    gl_FragColor=result;
}

 