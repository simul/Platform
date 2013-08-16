#ifndef CLOUDSHADOW_SL
#define CLOUDSHADOW_SL

vec3 GetRadialPos(vec3 wpos)
{
	//vec3 rel_pos		=wpos-cloudOrigin;
	//vec3 cloud_texc		=rel_pos*cloudScale;
	vec3 tex_pos		=mul(invShadowMatrix,vec4(wpos,1.0)).xyz;
//for this texture, x is the square root of distance and y is the angle anticlockwise from the x-axis.
	vec3 radial_pos		=vec3(sqrt(length(tex_pos.xy)),atan2(tex_pos.y,tex_pos.x)/(2.0*3.1415926536),tex_pos.z);
	return 0.5*(tex_pos+1.0);
}

vec2 GetIlluminationAt(Texture2D cloudShadowTexture,vec3 wpos)
{
	vec3 radial_pos		=GetRadialPos(wpos);
	vec4 texel			=sampleLod(cloudShadowTexture,clampWrapSamplerState,radial_pos.xy,0);
	vec2 illumination	=texel.xy;
	float above			=saturate((radial_pos.z-texel.z)/1.05);
	illumination		+=above;
	illumination		+=1.0-exp(min(0,(radial_pos.z-texel.z)/10.0));
	return saturate(illumination);
}

#endif
