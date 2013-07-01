#version 140
#include "CppGlsl.hs"
#include "saturate.glsl"
#include "../../CrossPlatform/atmospherics_constants.sl"
#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "../../CrossPlatform/depth.sl"
uniform sampler2D inscTexture;
uniform sampler2D skylightTexture;
uniform sampler2D depthTexture;
uniform sampler2D lossTexture;
#include "../../CrossPlatform/earth_shadow_uniforms.sl"
#include "../../CrossPlatform/earth_shadow.sl"
#include "view_dir.glsl"

in vec2 pos;
in vec2 texc;

void main()
{
	vec3 view			=texCoordToViewDirection(texc);
	float sine			=view.z;
    vec4 lookup			=texture(depthTexture,texc);
	float depth			=lookup.x;
	float dist			=depthToDistance(depth,pos.xy,nearZ,farZ,tanHalfFov);
	vec2 fade_texc		=vec2(pow(dist,0.5),0.5*(1.0-sine));
	float cos0			=dot(view,lightDir);
	vec3 skyl			=texture(skylightTexture,fade_texc).rgb;

	vec2 fade_texc2		=vec2(1.0,0.5*(1.0-sine));

	vec2 nearFarTexc	=EarthShadowDistances(fade_texc2,view);
	vec2 near_texc		=vec2(min(nearFarTexc.x,fade_texc.x),fade_texc.y);
	vec2 far_texc		=vec2(min(nearFarTexc.y,fade_texc.x),fade_texc.y);

	vec4 insc_far		=texture_clamp(inscTexture,far_texc);
	vec4 insc_near		=texture_clamp(inscTexture,near_texc);

	vec4 insc			=insc_far-insc_near;

	vec3 colour			=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio).rgb;
	colour				+=skyl;
	//colour.rg			=nearFarTexc.xx;
	//colour.b=0;
	//colour.rgb			=insc_near.rgb;
    gl_FragColor		=vec4(colour.rgb,1.0);
}
