// simul_sky.frag - a GLSL fragment shader
// Copyright 2008 Simul Software Ltd
uniform sampler2D planetTexture;
uniform vec3 lightDir;
uniform vec3 colour;
// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec2 texcoord;

void main()
{
	vec4 result=texture2D(planetTexture,texcoord);
	vec3 normal;
	normal.xy=2.0*(texcoord-vec2(0.5,0.5));
	float l=length(normal.xy);
	normal.z=-sqrt(1.0-l*l);
	float light=clamp(dot(normal.xyz,lightDir.xyz),0.0,1.0);
	result.rgb*=colour.rgb;
	result.rgb*=light;
    gl_FragColor=result;
}

