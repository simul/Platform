#ifndef GODRAYS_SL
#define GODRAYS_SL

float GetIlluminationAt(vec3 vd)
{
	vec3 pos			=vd+viewPosition;
	vec3 rel_pos		=pos-cloudOrigin;
	rel_pos.xy			-=lightDir.xy/lightDir.z*rel_pos.z;
	vec3 cloud_texc		=rel_pos*cloudScale;
	
	vec4 cloud_texel	=texture_wrap(cloudShadowTexture,cloud_texc.xy);
	float illumination	=cloud_texel.z;
	float above			=saturate(cloud_texc.z*10.0);
	illumination		+=above;
	illumination		=saturate(illumination);
	return 1.0-illumination;
}

#endif