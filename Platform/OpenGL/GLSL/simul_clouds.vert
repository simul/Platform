#version 140
#include "CppGlsl.hs"
#include "../../CrossPlatform/simul_cloud_constants.sl"

uniform vec3 eyePosition;
in vec4 vertex;
in vec3 multiTexCoord0;
in float multiTexCoord1;

out float layerDensity;
out float rainFade;
out vec4 texCoordDiffuse;
out vec2 noiseCoord;
out vec3 wPosition;
out vec3 texCoordLightning;
out vec2 fade_texc;
out vec3 view;
out vec4 transformed_pos;

void main(void)
{
	vec3 pos			=vertex.xyz;
	//pos.xyz			*=layerDistance;
    wPosition			=pos.xyz;
    transformed_pos		=vec4(vertex.xyz,1.0)*worldViewProj;
    gl_Position			=transformed_pos;
	texCoordDiffuse.xyz	=multiTexCoord0.xyz;
	texCoordDiffuse.w	=0.5+0.5*clamp(multiTexCoord0.z,0.0,1.0);	// clamp?
	//noiseCoord		=multiTexCoord1.xy;

	layerDensity		=multiTexCoord1;
	texCoordLightning	=(wPosition.xyz-illuminationOrigin.xyz)/illuminationScales.xyz;
	view				=wPosition-eyePosition.xyz;
	float depth			=length(view)/maxFadeDistanceMetres;
	view				=normalize(view);
	
	vec4 noise_pos		=noiseMatrix*vec4(view.xyz,1.0);
	noiseCoord			=vec2(atan(noise_pos.x,noise_pos.z),atan(noise_pos.y,noise_pos.z));
	noiseCoord			=noiseCoord*noiseScale+noiseOffset;
	float sine			=view.z;
	fade_texc			=vec2(sqrt(depth),0.5*(1.0-sine));
	rainFade			=1.0-exp(-layerDistance/10000.0);
}
