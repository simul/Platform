// simul_sky.frag - an OGLSL fragment shader
// Copyright 2008 Simul Software Ltd
uniform sampler2D planetTexture;
uniform vec3 lightDir;
// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec2 texcoord;

void main(void)
{
	vec4 result=texture2D(planetTexture,texcoord);

	// IN.tex is +- 1.
	vec3 normal;
	normal.xy=2.0*(texcoord-vec2(0.5,0.5));
	//normal.x=-texcoord.x;
	normal.y=-normal.y;
	float l=length(normal.xy);
	normal.z=sqrt(1.0-l*l);
	float light=clamp(dot(normal.xyz,lightDir.xyz),0.0,1.0);
//	result.rgb*=colour.rgb;
	
	result.rgb*=light;
	result.a=clamp((.5-l)/0.01,0.0,1.0);
	
    gl_FragColor=result;
}

