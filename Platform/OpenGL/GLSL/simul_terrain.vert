// simul_terrain.vert - an OGLSL vertex shader
// Copyright 2012 Simul Software Ltd

// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec2 texcoord;
varying vec3 wPosition;

void main(void)
{
    gl_Position		= ftransform();
    texcoord		= gl_MultiTexCoord0.xy;
	wPosition		= gl_Vertex.xyz;
}

