#version 130
// simul_sky.vert - an OGLSL vertex shader
// Copyright 2008 Simul Software Ltd

// varyings are written by vert shader, interpolated, and read by frag shader.
out vec2 texCoords;
out vec3 pos;

void main()
{
	pos=gl_Vertex.xyz;
    gl_Position		= gl_ModelViewProjectionMatrix * gl_Vertex;
#ifdef REVERSE_DEPTH
	gl_Position.z	=0.0;
#else
	gl_Position.z	=gl_Position.w;
#endif
	texCoords		= gl_MultiTexCoord0.xy;
}

