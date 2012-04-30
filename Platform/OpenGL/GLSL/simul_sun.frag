// simul_sky.frag - a GLSL fragment shader
// Copyright 2011 Simul Software Ltd
uniform vec3 sunlight;
// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec2 texcoord;

void main(void)
{
	vec4 result;
	// IN.tex is +- 1.
	vec2 rad=2.0*(texcoord-vec2(0.5,0.5));
	float l=length(rad);
	result.rgb=sunlight;
	result.a=clamp((1.0-l)/0.01,0.0,1.0);
    gl_FragColor=vec4(1.0,0.0,1.0,1.0);
}

 