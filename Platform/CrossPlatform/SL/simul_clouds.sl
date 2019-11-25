//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef CLOUDS_SL
#define CLOUDS_SL

#define MIN_SUN_ELEV (0.2)
#ifdef __ORBIS__
#define NUM_CLOUD_INTERP 2
#else
#define NUM_CLOUD_INTERP 4
#endif

#ifndef CLOUD_DEFS_ONLY

struct RaytracePixelOutput
{
	vec4 colour[NUM_CLOUD_INTERP];
	vec4 nearFarDepth;
};

struct FarNearPixelOutput
{
	vec4 farColour;
	vec4 nearColour;
};

float unshadowedBrightness(float Beta,vec4 lightResponse,vec3 ambientColour)
{
	return max(1.0,(Beta+lightResponse.y)+ambientColour.b);
}

vec4 calcColour(Texture2D lossTexture,Texture3D inscatterVolumeTexture,vec3 volumeTexCoords,Texture2D lightTableTexture
				,vec4 density,vec4 light,float Beta,vec4 lightResponse,vec3 ambientColour
				,vec3 world_pos,vec3 cloudTexCoords,vec3 amb_dir
				,vec2 fade_texc
				,out float brightnessFactor
				,out vec4 inscatter,float fade,float alphaSharpness)
{
	float alt_texc				=(world_pos.z/fadeAltitudeRangeKm);
	brightnessFactor			=unshadowedBrightness(Beta,lightResponse,ambientColour);
	float sun_alt_texc			=GetAltTexCoord(world_pos.z,minSunlightAltitudeKm,fadeAltitudeRangeKm);
	vec3 amb_lookup				=texture_clamp_lod(lightTableTexture,vec2(sun_alt_texc,2.5/4.0),0).rgb;
	ambientColour				=lightResponse.w*amb_lookup.rgb;
	vec3 combinedLightColour	=texture_clamp_lod(lightTableTexture,vec2(sun_alt_texc,3.5/4.0),0).rgb;
	vec3 ambient				=amb_lookup.rgb*light.w;
	vec4 c;
	
	float l						=lerp(0.75, 1.2, density.z);
	c.rgb						=(light.y*lightResponse.x*(Beta+l)+lightResponse.y*light.x)*combinedLightColour+ambient.rgb;
	c.a							=density.z*fade;
	c.a							=saturate(0.3+(1.0+1.0*density.w*alphaSharpness)*(c.a-0.3));
					
	{
		vec3 loss		=sampleLod(lossTexture,cmcSamplerState,fade_texc,0).rgb;
		c.rgb			*=loss;
#ifdef INFRARED
		//inscatter			=skyl;
#else
		inscatter		=texture_3d_wmc_lod(inscatterVolumeTexture,volumeTexCoords,0);
		inscatter.rgb	*=inscatter.a;
#endif
	}
	//.c.rgb=1;
	//inscatter=0;
	return c;
}

vec4 calcColourSimple(Texture2D lossTexture, Texture2D inscTexture, Texture2D skylTexture, Texture2D lightTableTexture
, vec4 density, float Beta, float BetaRayleigh, float BetaMie, vec4 lightResponse, vec3 ambientColour
, vec3 world_pos, vec3 cloudTexCoords
, vec2 fade_texc, vec2 nearFarTexc
, out float brightnessFactor)
{
	float sun_alt_texc = GetAltTexCoord(world_pos.z, minSunlightAltitudeKm, fadeAltitudeRangeKm);
	vec3 combinedLightColour = texture_clamp_lod(lightTableTexture, vec2(sun_alt_texc, 3.5 / 4.0), 0).rgb;
	ambientColour = lightResponse.w*texture_clamp_lod(lightTableTexture, vec2(sun_alt_texc, 2.5 / 4.0), 0).rgb;

	vec3 ambient = density.w*ambientColour.rgb;
	float opacity = density.z;
	vec4 c;
	c.rgb = (density.y*Beta + lightResponse.y*density.x)*combinedLightColour + ambient.rgb;
	c.a = opacity;
	brightnessFactor = unshadowedBrightness(Beta, lightResponse, ambientColour);
	float earthshadowFactr = saturate((fade_texc.x - nearFarTexc.x) / 0.1);
#ifdef INFRARED
	c.rgb = lerp(cloudIrRadiance1, cloudIrRadiance2, saturate(cloudTexCoords.z));//*c.a;
#endif
	vec3 loss = sampleLod(lossTexture, cmcSamplerState, fade_texc, 0).rgb;
	c.rgb *= loss;
	vec4 insc = sampleLod(inscTexture, cmcSamplerState, fade_texc, 0);
	vec3 skyl = sampleLod(skylTexture, cmcSamplerState, fade_texc, 0).rgb;
	vec3 inscatter = earthshadowFactr*PrecalculatedInscatterFunction(insc, BetaRayleigh, BetaMie, mieRayleighRatio);
	c.rgb += inscatter + skyl;
	return c;
}

void calcDensity(Texture3D cloudDensity,Texture3D cloudLight,vec3 texCoords,vec4 noiseval,vec3 fractalScale,float mip,inout vec4 density,out vec4 light)
{
	vec3 pos			=texCoords.xyz+fractalScale.xyz*(noiseval.xyz);
	density				=sample_3d_lod(cloudDensity,wwcSamplerState,pos,mip);
	// NOTE: VERY VERY IMPORTANT to use the original, not noise-modified, texture-coordinates for light.
	light				=sample_3d_lod(cloudLight,wwcSamplerState,texCoords,mip);

//	float tz			=texCoords.z*32.0;
//	density.z			*=saturate(tz+1.0)*saturate(32.0-tz);
}


float GetRainAtOffsetKm(Texture3D precipitationVolume,vec3 cloudWorldOffsetKm,vec3 inverseScalesKm,vec3 world_pos_km,vec2 rainCentreKm,float rainRadiusKm,float rainEdgeKm)
{
	vec3 rain_texc = cloudWorldOffsetKm;
	rain_texc.xy += rain_texc.z*rainTangent;
	#ifdef SFX_OPENGL
	rain_texc.y = 1.0 - rain_texc.y;
	#endif
	rain_texc *= inverseScalesKm;

	//Force nearest filtering on z-axis
	vec3 pvSize;
	GET_DIMENSIONS_3D(precipitationVolume, pvSize.x, pvSize.y, pvSize.z);
	uint z_texel = uint(rain_texc.z * pvSize.z);
	rain_texc.z = (float(z_texel) + 0.5)/pvSize.z;

	vec4 rain_lookup = precipitationVolume.SampleLevel(wwcSamplerState, rain_texc, 0);
	//return rain_lookup.x *saturate((rainRadiusKm-length(cloudWorldOffsetKm.xy-rainCentreKm.xy))*3.0) *saturate((20.0*rain_lookup.y-cloudWorldOffsetKm.z)/2.0); 
	return rain_lookup.x * rain_lookup.a * saturate((20.0*rain_lookup.y - cloudWorldOffsetKm.z) / 2.0);
	
}
float GetRainToSnowAtOffsetKm(Texture3D precipitationVolume,vec3 cloudWorldOffsetKm,vec3 inverseScalesKm,vec3 world_pos_km)
{
	vec3 rain_texc = cloudWorldOffsetKm;
	rain_texc.xy += rain_texc.z*rainTangent;
	#ifdef SFX_OPENGL
	rain_texc.y = 1.0 - rain_texc.y;
	#endif
	rain_texc *= inverseScalesKm;

	//Force nearest filtering on z-axis
	vec3 pvSize;
	GET_DIMENSIONS_3D(precipitationVolume, pvSize.x, pvSize.y, pvSize.z);
	uint z_texel = uint(rain_texc.z * pvSize.z);
	rain_texc.z = (float(z_texel) + 0.5)/pvSize.z;

	vec4 rain_lookup = precipitationVolume.SampleLevel(wwcSamplerState, rain_texc, 0);
	return rain_lookup.z;
}

vec3 colours[]={{1,0,0},{0,1,0},{0,0,1},{1,1,0},{0,1,1},{1,0,1},{1,1,1}};

void DebugStep(inout vec4 colour[NUM_CLOUD_INTERP]
	,vec4 rgba,inout float brightness_factor)
{
	vec4 clr[NUM_CLOUD_INTERP];
	clr[NUM_CLOUD_INTERP-1]=rgba;
	for(int i=0;i<NUM_CLOUD_INTERP;i++)
	{
		clr[i]			= clr[NUM_CLOUD_INTERP-1];
		colour[i].rgb	+=(clr[i].rgb)*clr[i].a*(colour[i].a);
		colour[i].a		*=(1.0-clr[i].a);
	}
	brightness_factor=1.0;
}

void ColourStep(inout vec4 colour[NUM_CLOUD_INTERP]
				,inout vec4 insc[NUM_CLOUD_INTERP]
				,inout float meanFadeDistance
				,inout float brightness_factor
				,Texture2D lossTexture
				,Texture2D inscTexture
				,Texture2D skylTexture
				,Texture3D inscatterVolumeTexture
				,bool godrays
				,Texture3D godraysVolumeTexture
				,float godraysScale
				,vec3 godraysTexCoords
				,Texture2D lightTableTexture
				,vec4 density
				,vec4 light
				,float distanceKm
				,float fadeDistance
				,vec3 world_pos
				,vec3 cloudTexCoords
				,vec2 fade_texc
				,vec2 nearFarTexc
				,float cosine
				,vec3 volumeTexCoords
				,vec3 amb_dir
				,float BetaClouds
				,float BetaRayleigh
				,float BetaMie
				,float solidDist_nearFar[NUM_CLOUD_INTERP]
				,bool noise
				,bool do_depth_mix
				,float distScale
				,int idx
				,vec4 noiseval,float fade
				,float alphaSharpness)
{
	density.z				*=saturate(distanceKm/CLOUD_FADEIN_DIST);
	vec4 clr[NUM_CLOUD_INTERP];
	vec4 inscatter=vec4(0,0,0,0);
	brightness_factor		=1.0;
	if (noise)
		clr[NUM_CLOUD_INTERP - 1]	=calcColour(lossTexture,inscatterVolumeTexture,volumeTexCoords,lightTableTexture
										,density
										,light
										,BetaClouds
										,lightResponse
										,ambientColour
										,world_pos
										,cloudTexCoords
										,amb_dir
										,fade_texc
										,brightness_factor
										,inscatter, fade,alphaSharpness);
	else
		clr[NUM_CLOUD_INTERP - 1]	=calcColourSimple(lossTexture,inscTexture,skylTexture,lightTableTexture
										,vec4(light.xyw,density.z)
										,BetaClouds,BetaRayleigh,BetaMie
										,lightResponse
										,ambientColour
										,world_pos
										,cloudTexCoords
										,fade_texc
										,nearFarTexc
										,brightness_factor);

	clr[NUM_CLOUD_INTERP - 1].a *= cosine *cosine;

	//clr[NUM_CLOUD_INTERP - 1].rg = saturate(10.0*cloudTexCoords.xy);
//clr[NUM_CLOUD_INTERP - 1].rgb*=0.5*(1.0+colours[idx%7]);

	meanFadeDistance		=lerp(min(fadeDistance,meanFadeDistance), meanFadeDistance,(1.0-.4*density.z));

	godraysTexCoords.z			=distanceKm*godraysScale;
	float gr_mult				=1.0;
	if(godrays)
		gr_mult=texture_3d_wcc_lod(godraysVolumeTexture,godraysTexCoords,0).x;
	for(int i=0;i<NUM_CLOUD_INTERP;i++)
	{
		clr[i]			= clr[NUM_CLOUD_INTERP-1];
		if(do_depth_mix)
		{
			float m			=saturate((solidDist_nearFar[i]-fadeDistance)/distScale);
			clr[i].a		*=m;
		}
		colour[i].rgb	+=(clr[i].rgb)*clr[i].a*(colour[i].a);
		insc[i].rgb		+=gr_mult*inscatter.rgb*clr[i].a*(colour[i].a);
		colour[i].a		*=(1.0-clr[i].a);
	}
}

#endif
#endif
