#version 140
#include "CppGlsl.hs"
#include "saturate.glsl"
#include "../../CrossPlatform/atmospherics_constants.sl"
#include "view_dir.glsl"
#include "../../CrossPlatform/simul_inscatter_fns.sl"
// Godrays are cloud-dependent. So we require the cloud texture.
uniform sampler2D cloudShadowTexture;
#include "../../CrossPlatform/cloud_shadow.sl"
#include "../../CrossPlatform/godrays.sl"
#include "../../CrossPlatform/depth.sl"
uniform sampler2D imageTexture;
uniform sampler2D overcTexture;
uniform sampler2D inscTexture;
uniform sampler2D lossTexture;
uniform sampler2D cloudNearFarTexture;
uniform sampler2D depthTexture;
uniform sampler2D cloudDepthTexture;

in vec2 texCoords;

void main()
{
	vec2 depth_texc		=(texCoords.xy);
	vec2 pos			=2.0*texCoords.xy-vec2(1.0,1.0);
	float solid_depth	=texture_clamp_lod(depthTexture,depth_texc,0).x;
	float cloud_depth	=texture_clamp_lod(cloudDepthTexture,texCoords.xy,0).x;
	float depth			=max(solid_depth,cloud_depth);
	// Convert to true distance, in units of the fade distance (i.e. 1.0= at maximum fade):
	float solid_dist	=depthToFadeDistance(depth,pos.xy,nearZ,farZ,tanHalfFov);
	vec4 gr=Godrays(cloudShadowTexture,cloudNearFarTexture,inscTexture,overcTexture,pos,invViewProj,maxFadeDistanceMetres,solid_dist);
    gl_FragColor=gr;
}
