
// simul_fade_3d_to_2d.vert - an OGLSL vertex shader
// Copyright 2008 Simul Software Ltd
varying vec2 texCoords;

void main()
{
    gl_Position		=ftransform();
    texCoords		=gl_MultiTexCoord0.xy;
}

