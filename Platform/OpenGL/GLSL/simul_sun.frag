// simul_sky.frag - a GLSL fragment shader
// Copyright 2011 Simul Software Ltd
uniform vec4 sunlight;
// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec2 texcoord;

float saturate(float a)
{
	return clamp(a,0.0,1.0);
}

void main(void)
{
	vec4 result;
	// IN.tex is +- 1.
	vec2 rad=2.0*(texcoord-vec2(0.5,0.5));
	float r=length(rad);
	if(r>1.0)
		discard;
	float u=(1.0-r)/0.1;
	float brightness=sunlight.a*pow(u,6.0);
	result.rgb=sunlight.rgb;
	result.a=brightness;
    gl_FragColor=result;
}

 