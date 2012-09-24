#version 140
#extension GL_EXT_geometry_shader4 : enable
// simul_terrain.vert - an OGLSL vertex shader
// Copyright 2012 Simul Software Ltd

uniform mat4 worldViewProj;
in vec3 vertex;
// varyings are written by vert shader, interpolated, and read by frag shader.
out vec3 wPosition;
out vec2 texcoord;

void main(void)
{
    gl_Position		= worldViewProj * vec4(vertex, 1.0);
	wPosition		= vertex;
	texcoord		= vec2(wPosition.xy/2000.0);
}

