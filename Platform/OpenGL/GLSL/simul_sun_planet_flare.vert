// simul_sky.vert - an OGLSL vertex shader
// Copyright 2008 Simul Software Ltd

// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec2 texcoord;
varying vec3 pos;

void main(void)
{
	pos=gl_Vertex.xyz;
    gl_Position		= gl_ModelViewProjectionMatrix * gl_Vertex;
#ifdef REVERSE_DEPTH
	gl_Position.z	=0.0;
#else
	gl_Position.z	=gl_Position.w;
#endif
	texcoord		= gl_MultiTexCoord0.xy;
}

