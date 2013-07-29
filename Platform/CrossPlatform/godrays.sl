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
vec3 GetRadialPos(vec3 wpos)
{
	vec3 rel_pos		=wpos-cloudOrigin;
	vec3 cloud_texc		=rel_pos*cloudScale;

	vec3 tex_pos		=mul(invShadowMatrix,vec4(wpos,1.0)).xyz;
	tex_pos				/=vec3(shadowRange,shadowRange,1000.0);
//for this texture, x is the square root of distance and y is the angle anticlockwise from the x-axis.
	vec3 radial_pos		=vec3(sqrt(length(tex_pos.xy)),atan2(tex_pos.y,tex_pos.x)/(2.0*3.1415926536),tex_pos.z);
	return radial_pos;
}

float GetIlluminationAt(vec3 wpos)
{
	vec3 radial_pos		=GetRadialPos(wpos);
	vec4 texel			=sampleLod(cloudShadowTexture,clampWrapSamplerState,radial_pos.xy,0);
	float illumination	=0.5*(texel.x+texel.y);
	float above		=saturate((radial_pos.z-texel.z)/1.05);
	illumination		+=above;
	illumination		+=1.0-exp(min(0,(radial_pos.z-texel.z)/10.0));
	return saturate(illumination);
}

#endif
