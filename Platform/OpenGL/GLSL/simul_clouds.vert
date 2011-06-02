// simul_sky.vert - an OGLSL vertex shader
// Copyright 2008 Simul Software Ltd

uniform vec4 eyePosition;

// varyings are written by vert shader, interpolated, and read by frag shader.

varying float layerDensity;

varying vec4 texCoordDiffuse;
varying vec2 noiseCoord;
varying vec3 eyespacePosition;
varying vec3 wPosition;
varying vec3 sunlight;

varying vec3 loss;
varying vec4 insc;
varying vec3 texCoordLightning;
uniform vec3 illuminationOrigin;
uniform vec3 illuminationScales;

void main(void)
{
    gl_Position			=gl_ModelViewProjectionMatrix*gl_Vertex;
    eyespacePosition	=(gl_ModelViewMatrix*gl_Vertex).xyz;
    wPosition			=gl_Vertex.xyz;
	texCoordDiffuse.xyz	=gl_MultiTexCoord0.xyz;
	texCoordDiffuse.w	=0.5+0.5*(gl_MultiTexCoord0.z);// clamp?
	noiseCoord			=gl_MultiTexCoord1.xy;
	layerDensity		=gl_MultiTexCoord2.x;
	sunlight			=gl_MultiTexCoord3.xyz;
	loss				=gl_MultiTexCoord4.xyz;
	insc				=gl_MultiTexCoord5.xyzw;
	
	texCoordLightning	=(wPosition.xzy-illuminationOrigin.xyz)/illuminationScales.xyz;
}
