// simul_sky.vert - an OGLSL vertex shader
// Copyright 2008 Simul Software Ltd

// varyings are written by vert shader, interpolated, and read by frag shader.

uniform vec3 illuminationOrigin;
uniform vec3 illuminationScales;
uniform vec3 eyePosition;
uniform float layerDistance;
uniform float maxFadeDistanceMetres;

varying float layerDensity;
varying vec4 texCoordDiffuse;
varying vec2 noiseCoord;
varying vec3 wPosition;
varying vec3 sunlight;
varying vec3 loss;
varying vec4 insc;
varying vec3 texCoordLightning;
varying vec2 fade_texc;
varying vec3 view;
varying vec4 transformed_pos;

void main(void)
{
	vec4 pos			=gl_Vertex;
	//pos.xyz			*=layerDistance;
    wPosition			=pos.xyz;
    transformed_pos		=gl_ModelViewProjectionMatrix*gl_Vertex;
    gl_Position			=transformed_pos;
	texCoordDiffuse.xyz	=gl_MultiTexCoord0.xyz;
	texCoordDiffuse.w	=0.5+0.5*clamp(gl_MultiTexCoord0.z,0.0,1.0);// clamp?
	noiseCoord			=gl_MultiTexCoord1.xy;
	layerDensity		=gl_MultiTexCoord2.x;
	sunlight			=gl_MultiTexCoord3.xyz;
	loss				=gl_MultiTexCoord4.xyz;
	insc				=gl_MultiTexCoord5.xyzw;
	
	texCoordLightning	=(wPosition.xyz-illuminationOrigin.xyz)/illuminationScales.xyz;

	view=(wPosition-eyePosition.xyz);
	float depth=length(view)/maxFadeDistanceMetres;
	view=normalize(view);
	float sine=view.z;
	fade_texc=vec2(sqrt(depth),0.5*(1.0-sine));
}
