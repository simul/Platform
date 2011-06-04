// simul_sky.vert - an OGLSL vertex shader
// Copyright 2008 Simul Software Ltd
uniform float hazeEccentricity;
uniform float altitudeTexCoord;
uniform vec3 mieRayleighRatio;
uniform vec3 eyePosition;
uniform vec3 lightDir;
uniform float skyInterp;

// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec3 dir;

void main(void)
{
    gl_Position		= ftransform();
    dir				= normalize(gl_Vertex.xyz);
}

