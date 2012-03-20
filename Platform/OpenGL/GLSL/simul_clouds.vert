// simul_sky.vert - an OGLSL vertex shader
// Copyright 2008 Simul Software Ltd


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
uniform vec3 eyePosition;
uniform float layerDistance;
uniform float maxFadeDistanceMetres;
varying vec2 fade_texc;

varying vec3 view;
void main(void)
{
	vec4 pos			=gl_Vertex;
	//pos.xyz			*=layerDistance;
    wPosition			=pos.xyz-eyePosition.xyz;
    gl_Position			=gl_ModelViewProjectionMatrix*pos;
    eyespacePosition	=(gl_ModelViewMatrix*pos).xyz;
	texCoordDiffuse.xyz	=gl_MultiTexCoord0.xyz;
	texCoordDiffuse.w	=0.5+0.5*clamp(gl_MultiTexCoord0.z,0.0,1.0);// clamp?
	noiseCoord			=gl_MultiTexCoord1.xy;
	layerDensity		=gl_MultiTexCoord2.x;
	sunlight			=gl_MultiTexCoord3.xyz;
	loss				=gl_MultiTexCoord4.xyz;
	insc				=gl_MultiTexCoord5.xyzw;
	
	texCoordLightning	=(wPosition.xzy-illuminationOrigin.xyz)/illuminationScales.xyz;
	view=normalize(wPosition);

	float sine=view.z;
	float depth=length(wPosition)/maxFadeDistanceMetres;
	fade_texc=vec2(sqrt(depth),0.5f*(1.f-sine));
}
