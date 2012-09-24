#version 140
#extension GL_EXT_gpu_shader4 : enable
// simul_terrain.frag - a GLSL fragment shader
// Copyright 2012 Simul Software Ltd
uniform vec3 eyePosition;
uniform float maxFadeDistanceMetres;
uniform sampler2DArray textures;

// varyings are written by vert shader, interpolated, and read by frag shader.
in vec3 wPosition;
in vec2 texcoord;

out vec4 fragmentColour;

void main()
{
	vec4 result;
	vec4 layer1=texture2DArray(textures,vec3(texcoord,1.0));
	vec4 layer2=texture2DArray(textures,vec3(texcoord,0.0));
	vec4 texel=mix(layer1,layer2,clamp(wPosition.z/100.0,0.0,1.0));
	result.rgb=texel.rgb;
	// Distance
	result.a=length(wPosition-eyePosition)/maxFadeDistanceMetres;
	result.rgb=result.rgb;
    fragmentColour=result;
}
