#ifndef GODRAYS_SL
#define GODRAYS_SL

vec2 GetIlluminationAt2(Texture2D cloudShadowTexture,vec3 texc)
{
	vec4 texel			=texture_clamp_lod(cloudShadowTexture,texc.xy,0);
	vec2 illumination	=texel.xy;
	float above			=saturate((texc.z-texel.z)/0.5);
	illumination		+=above;
	return saturate(illumination);
}

	
#endif
