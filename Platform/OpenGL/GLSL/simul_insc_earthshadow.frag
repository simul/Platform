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
#include "simul_earthshadow_uniforms.glsl"
#include "../../CrossPlatform/earth_shadow.sl"
#include "view_dir.glsl"

in vec2 pos;
in vec2 texc;

void main()
{
	vec3 view		=texCoordToViewDirection(texc);
	float sine		=view.z;
    vec4 lookup		=texture(depthTexture,texc);
	float depth		=lookup.x;
	float dist		=depthToDistance(depth,pos.xy,nearZ,farZ,tanHalfFov);
	vec2 fade_texc	=vec2(pow(dist,0.5),0.5*(1.0-sine));
	float cos0		=dot(view,lightDir);
	vec3 skyl		=texture(skylightTexture,fade_texc).rgb;

	vec2 fade_texc2	=vec2(1.0,0.5*(1.0-sine));
	vec4 insc		=EarthShadowFunction(fade_texc,view);
	vec3 colour		=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio).rgb;
	colour			+=skyl;

    gl_FragColor	=vec4(view.xyz,1.0);
}
