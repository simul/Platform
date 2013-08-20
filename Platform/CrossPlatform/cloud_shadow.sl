#ifndef CLOUDSHADOW_SL
#define CLOUDSHADOW_SL

vec3 GetRadialPos(vec3 wpos)
{
	//vec3 rel_pos		=wpos-cloudOrigin;
	//vec3 cloud_texc		=rel_pos*cloudScale;

	vec3 tex_pos		=mul(invShadowMatrix,vec4(wpos,1.0)).xyz;
//for this texture, x is the square root of distance and y is the angle anticlockwise from the x-axis.
#if 1
	vec3 radial_pos		=vec3(sqrt(length(tex_pos.xy)),atan2(tex_pos.y,tex_pos.x)/(2.0*3.1415926536),tex_pos.z);
	return radial_pos;//0.5*(tex_pos+1.0);
#else
	return 0.5*(tex_pos+1.0);
#endif
}

vec2 GetIlluminationAt(Texture2D cloudShadowTexture,vec3 wpos)
{
#ifdef RADIAL_CLOUD_SHADOW
	vec3 texc			=GetRadialPos(wpos);
#else
	vec3 tex_pos		=mul(invShadowMatrix,vec4(wpos,1.0)).xyz;
	vec3 texc			=0.5*(tex_pos+1.0);
#endif
	vec4 texel			=texture_cwc_lod(cloudShadowTexture,texc.xy,0);
	vec2 illumination	=texel.xy;
	float above			=saturate((texc.z-texel.z)/2.5);
	//illumination		-=above;
	//illumination		+=1.0-exp(min(0,(radial_pos.z-texel.z)/10.0));
	return saturate(illumination);
}

#endif
