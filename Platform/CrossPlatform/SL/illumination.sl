#ifndef ILLUMINATION_SL
#define ILLUMINATION_SL

vec2 LimitWithin(vec2 original,vec2 maximum)
{
	original=max(original,maximum.xx);
	original=min(original,maximum.yy);
	return original;
}

vec2 OvercastDistances(float alt_km,float sine,float overcastBaseKm,float overcastRangeKm,float maxFadeDistanceKm)
{
	vec2 range_km					=vec2(0.0,maxFadeDistanceKm);

	float cutoff_alt_km				=overcastBaseKm+0.5*overcastRangeKm;
#if 0
	float dist_to_plane_km			=clamp((cutoff_alt_km-alt_km)/sine,0,maxFadeDistanceKm);
	if(dist_to_plane_km==0)
		dist_to_plane_km			=maxFadeDistanceKm;
	float over						=saturate(alt_km-cutoff_alt_km);
	float under						=saturate(cutoff_alt_km-alt_km);
	if(sine>0)
	{
	if(alt_km>cutoff_alt_km)
	{
			range_km.x	=maxFadeDistanceKm;
			range_km.y	=maxFadeDistanceKm;
		}
		else
		{
			range_km.x	=0.0;
			range_km.y	=dist_to_plane_km;
		}
	}
	if(sine==0)
	{
		if(alt_km>cutoff_alt_km)
		{
			range_km.x	=maxFadeDistanceKm;
			range_km.y	=maxFadeDistanceKm;
		}
		else
		{
			range_km.x	=0.0;
			range_km.y	=maxFadeDistanceKm;
		}
	}
		if(sine<0)
	{
		if(alt_km>cutoff_alt_km)
		{
			range_km.x	=dist_to_plane_km;
			range_km.y	=maxFadeDistanceKm;
		}
		else
		{
			range_km.x	=0.0;
			range_km.y	=maxFadeDistanceKm;
		}
	}
#else
	if(alt_km>cutoff_alt_km)
	{
		range_km.y					=maxFadeDistanceKm;
		if(sine<0)
		{
			range_km.x				=max(0.0,(cutoff_alt_km-alt_km)/sine);
		}
		else
		{
			range_km.x				=maxFadeDistanceKm;
	}
	}
	else
	{
		range_km.x					=0.0;
		if(sine>0)
			range_km.y				=max(0.0,(cutoff_alt_km-alt_km)/sine);
		else
			range_km.y				=maxFadeDistanceKm;
	}
#endif
	return sqrt(range_km/maxFadeDistanceKm);
}

vec4 IlluminationBuffer(float alt_km,vec2 texCoords,vec2 targetTextureSize
	,float overcastBaseKm,float overcastRangeKm,float maxFadeDistanceKm
	,float maxFadeDistance,float terminatorDistance,float radiusOnCylinder,vec3 earthShadowNormal,vec3 sunDir)
{
	float azimuth			=3.1415926536*2.0*texCoords.x;
	float sine				=-1.0+2.0*(texCoords.y*targetTextureSize.y/(targetTextureSize.y-1.0));
	sine					=clamp(sine,-1.0,1.0);
	float cosine			=sqrt(1.0-sine*sine);
	vec3 view				=vec3(cosine*sin(azimuth),cosine*cos(azimuth),sine);
	vec2 fade_texc			=vec2(1.0,texCoords.y);
	vec2 full_bright_range	=EarthShadowDistances(fade_texc,view,earthShadowNormal,sunDir,maxFadeDistance,terminatorDistance,radiusOnCylinder);
	vec2 overcast_range		=OvercastDistances(alt_km,sine,overcastBaseKm,overcastRangeKm,maxFadeDistanceKm);
    return vec4(full_bright_range,overcast_range);
}
#ifndef OVERCAST_STEPS
#define OVERCAST_STEPS 6
#endif
vec4 OvercastInscatter(Texture2D inscTexture,Texture2D illuminationTexture,vec2 texCoords,float alt_km,float maxFadeDistanceKm
	,float overcast,float overcastBaseKm,float overcastRangeKm,vec2 targetTextureSize)
{
	vec2 fade_texc			=vec2(texCoords.x,1.0-texCoords.y);
    
	// Texcoords representing the full distance from the eye to the given point.
	vec4 insc				=texture_cmc_lod(inscTexture,fade_texc,0);
	
	// Only need the y-coordinate of the illumination texture to get the overcast range.
	vec2 illum_texc			=vec2(0.5,fade_texc.y);
	vec4 illum_lookup		=texture_clamp_lod(illuminationTexture,illum_texc,0);

	float sine				=1.0-(2.0*(fade_texc.y)*targetTextureSize.y/(targetTextureSize.y-1.0));
	float dist_km			=fade_texc.x*fade_texc.x*maxFadeDistanceKm;
	vec4 insc_diff			=vec4(0,0,0,0);

	float ov_total			=0.0;
	for(int i=0;i<OVERCAST_STEPS;i++)
	{
		float u1			=float(i)/float(OVERCAST_STEPS);
		float u2			=float(i+1)/float(OVERCAST_STEPS);
		float this_alt_km	=alt_km+sine*dist_km*0.5*(u1+u2);
		float ov			=saturate(((overcastRangeKm+overcastBaseKm)-this_alt_km)/overcastRangeKm);
		vec2 fade_texc_1	=vec2(fade_texc.x*u1,fade_texc.y);
		vec2 fade_texc_2	=vec2(fade_texc.x*u2,fade_texc.y);
		vec4 overc_1		=texture_cmc_lod(inscTexture,fade_texc_1,0);
		vec4 overc_2		=texture_cmc_lod(inscTexture,fade_texc_2,0);
		insc_diff.rgb		+=ov*max(vec3(0,0,0),overc_2.rgb-overc_1.rgb);
		insc_diff.w			+=ov*0.5*(overc_1.a+overc_2.a);
		ov_total			+=ov;
	}
	insc_diff.w				/=float(OVERCAST_STEPS);
	ov_total				/=float(OVERCAST_STEPS);
	insc.rgb				=max(vec3(0,0,0),insc.rgb-insc_diff.rgb*overcast);
	insc.w					=lerp(insc.a,0,ov_total);
    return insc;
}

vec4 ShowIlluminationBuffer(Texture2D illTexture,vec2 texCoords)
{
	if(texCoords.x<0.5)
	{
		texCoords.x		*=2.0;
		vec4 nf			=texture_cmc_nearest_lod(illTexture,texCoords,0);
		return saturate(vec4(nf.zw,0.0,1.0));
	}
	else
	{
		texCoords.x		=2.0*(texCoords.x-0.5);
		vec2 texc		=vec2(0.5,texCoords.y);
		vec4 nf			=texture_cmc_nearest_lod(illTexture,texc,0);
		// Near Far for EarthShadow illumination is xy
		// Near Far for clouds overcast is zw.
		vec4 result		=vec4(0,1.0,0,0);
		if(texCoords.x>=nf.x&&texCoords.x<=nf.y)
			result.r	=1.0;
		if(texCoords.x>=nf.z&&texCoords.x<=nf.w)
			result.g	=0.0;
		return vec4(result.rgb,1);
	}
}
#endif