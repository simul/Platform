#version 140
#include "saturate.glsl"

#include "CppGlsl.hs"

uniform sampler2D imageTexture;
uniform sampler2D coverageTexture;
uniform sampler2D lossTexture;
uniform sampler2D inscTexture;
uniform sampler2D skylTexture;
uniform sampler2D depthTexture;
uniform sampler2D illuminationTexture;

in vec2 texc_global;
in vec2 texc_detail;
in vec3 wPosition;
in vec4 clip_pos;

#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "../../CrossPlatform/earth_shadow_uniforms.sl"
#include "../../CrossPlatform/depth.sl"

#include "../../CrossPlatform/simul_2d_clouds.hs"
#include "../../CrossPlatform/earth_shadow.sl"
#include "../../CrossPlatform/earth_shadow_fade.sl"
#include "../../CrossPlatform/simul_2d_clouds.sl"

void main()
{
	vec3 depth_pos	=clip_pos.xyz/clip_pos.w;
	//depth_pos.z	=clip_pos.z;
	vec3 depth_texc	=0.5*(depth_pos+vec3(1.0,1.0,1.0));

    float depth			=texture(depthTexture,depth_texc.xy).x;
#ifdef REVERSE_DEPTH
	if(depth>0)
		discard;
#else
	if(1.0>depth)
		discard;
#endif
	float dist		=depthToFadeDistance(depth,depth_pos.xy,nearZ,farZ,tanHalfFov);
	vec3 wEyeToPos	=wPosition-eyePosition;
	vec4 ret		=Clouds2DPS(imageTexture
						,coverageTexture
						,lossTexture
						,inscTexture
						,skylTexture
						,texc_global
						,texc_detail
						,wEyeToPos
						,sunlight.rgb
						,ambientLight.rgb
						,lightDir.xyz
						,lightResponse);
	ret.rgb			*=exposure;
	gl_FragColor	=ret;//vec4(depth_texc.zzz,ret.a);
}
