#version 140
#include "CppGlsl.hs"
#include "../../CrossPlatform/simul_cloud_constants.sl"

uniform vec3 eyePosition;
uniform mat4 worldViewProj;
in vec4 vertex;
in vec3 multiTexCoord0;
in float multiTexCoord1;
in vec2 multiTexCoord2;

out float layerDensity;
out float rainFade;
out vec4 texCoordDiffuse;
out vec2 noise_texc;
out vec3 wPosition;
out vec3 texCoordLightning;
out vec2 fade_texc;
out vec3 view;
out vec4 transformed_pos;

void main(void)
{
	LayerData layer		=layers[layerNumber];
	vec3 pos			=vertex.xyz;
	//pos.xyz				*=layer.layerDistance;
    wPosition			=pos.xyz;
    transformed_pos		=vec4(vertex.xyz,1.0)*worldViewProj;
    gl_Position			=transformed_pos;
	texCoordDiffuse.xyz	=multiTexCoord0.xyz;
	texCoordDiffuse.w	=0.5+0.5*clamp(multiTexCoord0.z,0.0,1.0);	// clamp?

	layerDensity		=multiTexCoord1;
	texCoordLightning	=(wPosition.xyz-illuminationOrigin.xyz)/illuminationScales.xyz;
	view				=wPosition-eyePosition.xyz;
	float depth			=length(view)/maxFadeDistanceMetres;
	view				=normalize(view);
	vec2 screen_pos		=transformed_pos.xy/transformed_pos.w;
	vec3 n				=vec3(screen_pos.xy*tanHalfFov,1.0);
	n					=normalize(n);
	vec2 noise_texc_0	=(noiseMatrix*vec4(n.xy,0.0,0.0)).xy;

	noise_texc			=noise_texc_0.xy*layer.noiseScale;//+layer.noiseOffset;

	float sine			=view.z;
	fade_texc			=vec2(sqrt(depth),0.5*(1.0-sine));
	rainFade			=1.0-exp(-layer.layerDistance/10000.0);
}
