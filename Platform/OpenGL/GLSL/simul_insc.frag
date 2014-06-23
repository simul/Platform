#version 140
#include "CppGlsl.hs"
#include "saturate.glsl"
#include "../../CrossPlatform/SL/atmospherics_constants.sl"
#include "../../CrossPlatform/SL/simul_inscatter_fns.sl"
#include "../../CrossPlatform/SL/depth.sl"
uniform sampler2D depthTexture;
uniform sampler2D inscTexture;
uniform sampler2D skylightTexture;
uniform sampler2D cloudShadowTexture;

in vec2 pos;
in vec2 texCoords;
out vec4 gl_FragColor;

#include "view_dir.glsl"

float GetIlluminationAt(vec3 vd)
{
	vec3 pos=vd+viewPosition;
	vec3 cloud_texc=(pos-cloudOrigin)*cloudScale;
	vec4 cloud_texel=texture(cloudShadowTexture,cloud_texc.xy);
	float illumination=cloud_texel.z;
	float above=saturate(cloud_texc.z);
	illumination=saturate(illumination-above);
	return illumination;
}

vec4 simple()
{
	vec3 view=texCoordToViewDirection(texCoords);
	float sine=view.z;
    vec4 lookup=texture(depthTexture,texCoords);
	float depth=lookup.x;
	float dist=depthToFadeDistance(depth,pos.xy,depthToLinFadeDistParams,tanHalfFov);
	vec2 fade_texc=vec2(pow(dist,0.5),0.5*(1.0-sine));
	vec4 insc=texture(inscTexture,fade_texc);
	float cos0=dot(view,lightDir);
	vec3 colour=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio).rgb;
	vec3 skyl=texture(skylightTexture,fade_texc).rgb;
	colour+=skyl;
	colour*=exposure;
    return vec4(colour.rgb,1.0);
}

void main()
{
	gl_FragColor=simple();
}
