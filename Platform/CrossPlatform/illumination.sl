#ifndef ILLUMINATION_SL
#define ILLUMINATION_SL

vec2 OvercastDistances(float alt_km,vec2 fade_texc,vec3 view,float overcastBaseKm,float overcastRangeKm,float maxFadeDistanceKm)
{
	float3 alt_range;
	float sine						=view.z;
	vec2 range_km;
	float cutoff_alt_km				=overcastBaseKm+0.5*overcastRangeKm;
	if(alt_km>cutoff_alt_km)
	{
		if(sine<0)
			range_km.x				=max(0.0,(cutoff_alt_km-alt_km)/sine);
		else
			range_km.x				=maxFadeDistanceKm;
		range_km.y					=maxFadeDistanceKm;
	}
	else
	{
		range_km.x					=0.0;
		if(sine>0)
			range_km.y				=max(0.0,(cutoff_alt_km-alt_km)/sine);
		else
			range_km.y				=maxFadeDistanceKm;
	}
	return sqrt(range_km/maxFadeDistanceKm);
}

vec2 LimitWithin(vec2 original,vec2 maximum)
{
	original=max(original,maximum.xx);
	original=min(original,maximum.yy);
	return original;
}

vec4 IlluminationBuffer(float alt_km,vec2 texCoords,vec2 targetTextureSize,float overcastBaseKm,float overcastRangeKm,float maxFadeDistanceKm)
{
	float azimuth			=3.1415926536*2.0*texCoords.x;
	float sine				=-1.0+2.0*(texCoords.y*targetTextureSize.y/(targetTextureSize.y-1.0));
	float cosine			=sqrt(1.0-sine*sine);
	vec3 view				=vec3(cosine*sin(azimuth),cosine*cos(azimuth),sine);
	vec2 fade_texc			=vec2(1.0,texCoords.y);
	vec2 full_bright_range	=EarthShadowDistances(fade_texc,view);
	vec2 overcast_range		=OvercastDistances(alt_km,fade_texc,view,overcastBaseKm,overcastRangeKm, maxFadeDistanceKm);
	overcast_range			=LimitWithin(overcast_range,full_bright_range);
    return vec4(full_bright_range,overcast_range);
}

vec4 OvercastInscatter(Texture2D inscTexture,Texture2D illuminationTexture,vec2 fade_texc,float overcast)
{
	// Texcoords representing the full distance from the eye to the given point.
	vec4 insc				=inscTexture.Sample(cmcSamplerState,fade_texc);

	// Only need the y-coordinate of the illumination texture to get the overcast range.
	vec2 illum_texc			=vec2(0.5,fade_texc.y);
	vec4 illum_lookup		=illuminationTexture.Sample(cmcSamplerState,illum_texc);
	vec2 overcDistTexc		=illum_lookup.zw;

	vec2 overc_near_texc	=vec2(overcDistTexc.x,fade_texc.y);
	vec4 overc_near			=inscTexture.Sample(cmcSamplerState,overc_near_texc);
	
	vec2 overc_far_texc		=vec2(min(overcDistTexc.y,fade_texc.x),fade_texc.y);
	vec4 overc_far			=inscTexture.Sample(cmcSamplerState,overc_far_texc);

	vec4 overc				=max(vec4(0,0,0,0),overc_far-overc_near);

	insc=max(vec4(0,0,0,0),insc-overc*overcast);
    return insc;
}

#endif