// simul_sky.vert - an OGLSL vertex shader
// Copyright 2008 Simul Software Ltd

// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec2 texcoord;

void main(void)
{
    gl_Position		= ftransform();
    texcoord		= gl_MultiTexCoord0.xy;
}

