#version 140
#include "saturate.glsl"

#include "CppGlsl.hs"

uniform sampler2D imageTexture;
uniform sampler2D coverageTexture1;
uniform sampler2D coverageTexture2;
uniform sampler2D lossTexture;
uniform sampler2D inscTexture;
uniform sampler2D skylTexture;
uniform sampler2D depthTexture;

in vec2 texc_global;
in vec2 texc_detail;
in vec3 wPosition;
in vec4 clip_pos;

#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "simul_earthshadow_uniforms.glsl"
#include "../../CrossPlatform/depth.sl"

#include "../../CrossPlatform/simul_2d_clouds.hs"
#include "../../CrossPlatform/simul_2d_clouds.sl"

void main()
{
	vec3 depth_pos	=clip_pos.xyz/clip_pos.w;
	//depth_pos.z	=clip_pos.z;
	vec3 depth_texc	=0.5*(depth_pos+vec3(1.0,1.0,1.0));
	//depth_texc.z	=depth_pos.z;
	vec4 ret		=Clouds2DPS(texc_global,texc_detail,wPosition,depth_texc);
	gl_FragColor	=ret;//vec4(depth_texc.zzz,ret.a);
}
