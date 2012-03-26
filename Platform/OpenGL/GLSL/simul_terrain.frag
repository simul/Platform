// simul_terrain.frag - a GLSL fragment shader
// Copyright 2012 Simul Software Ltd
uniform vec3 eyePosition;
uniform float maxFadeDistanceMetres;
// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec2 texcoord;
varying vec3 wPosition;

void main(void)
{
	vec4 result;
	result.rgb=vec3(0.4,0.5,0.3);
	// Distance
	result.a=length(wPosition-eyePosition)/maxFadeDistanceMetres;
	result.rgb=result.rgb;
    gl_FragColor=result;
}
