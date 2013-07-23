#ifndef GODRAYS_SL
#define GODRAYS_SL
/*
float GetIlluminationAt(vec3 pos)
{
	vec3 rel_pos		=pos-cloudOrigin;
	rel_pos.xy			-=lightDir.xy/lightDir.z*rel_pos.z;
	vec3 cloud_texc		=rel_pos*cloudScale;
	
	vec4 cloud_texel	=texture_wrap(cloudShadowTexture,cloud_texc.xy);
	float illumination	=cloud_texel.x; // HLSL x=2nd, y=1st, w=amb
	float above			=saturate(cloud_texc.z);
	illumination		+=above;
	return saturate(illumination);
}*/

float GetIlluminationAt(vec3 wpos)
{
	vec3 rel_pos		=wpos-cloudOrigin;
	vec3 cloud_texc		=rel_pos*cloudScale;

	vec3 tex_pos		=mul(shadowMatrix,vec4(wpos,1.0)).xyz;
	tex_pos.xy			/=160000.0;
	tex_pos.xy			+=vec2(0.5,0.5);
	vec4 texel			=sampleLod(cloudShadowTexture,clampSamplerState,tex_pos.xy,0);
	float illumination	=texel.r;
	float above			=saturate((tex_pos.z-texel.z)/1000.0);
	illumination		+=above;
	//illumination		+=1.0-exp(min(0,(tex_pos.z-texel.z)/10000.0));
	return saturate(illumination);
}

#endif
