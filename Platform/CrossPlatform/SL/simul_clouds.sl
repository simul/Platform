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

SamplerState cloudSamplerState: register( s0);
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
				,out vec4 inscatter,float fade)
{
	float alt_texc				=(world_pos.z/fadeAltitudeRangeKm);
	brightnessFactor			=unshadowedBrightness(Beta,lightResponse,ambientColour);
	float sun_alt_texc			=GetAltTexCoord(world_pos.z,minSunlightAltitudeKm,fadeAltitudeRangeKm);
	vec3 amb_lookup				=texture_clamp_lod(lightTableTexture,vec2(sun_alt_texc,2.5/4.0),0).rgb;
	ambientColour				=lightResponse.w*amb_lookup.rgb;
	vec3 combinedLightColour	=texture_clamp_lod(lightTableTexture,vec2(sun_alt_texc,3.5/4.0),0).rgb;
	vec3 ambient				=amb_lookup.rgb*light.w;
	vec4 c;
	float l						=lerp(0.02, 1.5, density.z);
	c.rgb						=(light.y*lightResponse.x*(Beta+l)+lightResponse.y*light.x)*combinedLightColour+ambient.rgb;
	c.a							=density.z*fade;

	{
		vec3 loss		=sampleLod(lossTexture		,cmcSamplerState,fade_texc,0).rgb;
		c.rgb			*=loss;
#ifdef INFRARED
		//c			=skyl.rgb;
#else
		inscatter		=texture_3d_wmc_lod(inscatterVolumeTexture,volumeTexCoords,0);
		inscatter.rgb	*=inscatter.a;
#endif
	}
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
	density				=sample_3d_lod(cloudDensity,wwcSamplerState,pos,0);
	// NOTE: VERY VERY IMPORTANT to use the original, not noise-modified, texture-coordinates for light.
	light				=sample_3d_lod(cloudLight,wwcSamplerState,texCoords,mip);
	float tz			=texCoords.z*32.0;
	density.z			*=saturate(tz+1.0)*saturate(32.0-tz);
}

FarNearPixelOutput Lightpass(Texture3D cloudDensity
								,Texture3D noiseTexture3D
								,vec4 dlookup
								,vec3 view
								,vec4 clip_pos
								,vec3 sourcePosKm_w
								,float source_radius
								,vec3 spectralFluxOver1e6
								,float minCosine
								,float maxIlluminatedRadius
								,float threshold)
{
	float max_spectral_flux			=max(max(spectralFluxOver1e6.r,spectralFluxOver1e6.g),spectralFluxOver1e6.b);
	FarNearPixelOutput res;
	res.farColour			=vec4(0,0,0,1.0);
	res.nearColour			=vec4(0,0,0,1.0);

	vec3 dir_to_source		=normalize(sourcePosKm_w-viewPosKm);
	float cos0				=dot(dir_to_source.xyz,view.xyz);
	if (cos0 < minCosine)
		return res;
	float sine				=view.z;

	float min_z				=cornerPosKm.z-(fractalScale.z*1.5)/inverseScalesKm.z;
	float max_z				=cornerPosKm.z+(1.0+fractalScale.z*1.5)/inverseScalesKm.z;
	if(view.z<-0.1&&viewPosKm.z<cornerPosKm.z-fractalScale.z/inverseScalesKm.z)
		return res;
	
	vec2 solidDist_nearFar	=(dlookup.yx);
	vec2 fade_texc			=vec2(0.0,0.5*(1.0-sine));
	float meanFadeDistance	=1.0;

	vec3 world_pos					=viewPosKm;

	// This provides the range of texcoords that is lit.
	int3 c_offset					=int3(sign(view.x),sign(view.y),sign(view.z));
	int3 start_c_offset				=-c_offset;
	start_c_offset					=int3(max(start_c_offset.x,0),max(start_c_offset.y,0),max(start_c_offset.z,0));
	vec3 viewScaled					=view/scaleOfGridCoordsKm;
	viewScaled						=normalize(viewScaled);

	vec3 offset_vec	=vec3(0,0,0);
	if(world_pos.z<min_z)
	{
		float a		=1.0/(view.z+0.00001);
		offset_vec	=(min_z-world_pos.z)*vec3(view.x*a,view.y*a,1.0);
	}
	if(view.z<0&&world_pos.z>max_z)
	{
		float a		=1.0/(-view.z+0.00001);
		offset_vec	=(world_pos.z-max_z)*vec3(view.x*a,view.y*a,-1.0);
	}
	world_pos						+=offset_vec;
	float viewScale					=length(viewScaled*scaleOfGridCoordsKm);
	// origin of the grid - at all levels of detail, there will be a slice through this in 3 axes.
	vec3 startOffsetFromOrigin		=viewPosKm-gridOriginPosKm;
	vec3 offsetFromOrigin			=world_pos-gridOriginPosKm;
	vec3 p0							=offsetFromOrigin/scaleOfGridCoordsKm;
	int3 c0							=int3(floor(p0) + start_c_offset);
	vec3 gridScale					=scaleOfGridCoordsKm;
	vec3 P0							=offsetFromOrigin/scaleOfGridCoordsKm/2.0;
	int3 C0							=c0>>1;
	
	float distanceKm				=length(offset_vec);
	int3 c							=c0;

	int idx=0;
	float W							=halfClipSize;
	const float start				=0.866*0.866;//0.707 for 2D, 0.866 for 3D;
	const float ends				=1.0;
	const float range				=ends-start;


	vec4 colour						=vec4(0.0,0.0,0.0,1.0);
	vec4 nearColour					=vec4(0.0,0.0,0.0,1.0);
	float lastFadeDistance			=0.0;
	int3 b							=abs(c-C0*2);
	for(int j=0;j<8;j++)
	{
		if(false)
		if(max(max(b.x,b.y),0)>=W)
		{
			// We want to round c and C0 downwards. That means that 3/2 should go to 1, but that -3/2 should go to -2.
			// Just dividing by 2 gives 3/2 -> 1, and -3/2 -> -1.
			c0			=	C0;
			c			+=	start_c_offset;
			c			-=	abs(c&int3(1,1,1));
			c			=	c>>1;
			gridScale	*=	2.0;
			viewScale	*=	2.0;
			if(idx==0)
				W*=2;
			p0			=	P0;
			P0			=	startOffsetFromOrigin/gridScale/2.0;
			C0			+=	start_c_offset;
			C0			-=	abs(C0&int3(1,1,1));
			C0			=	C0>>1;
			idx			++;
			b						=abs(c-C0*2);
		}
		else break;
	}
	for(int i=0;i<255;i++)
	{
		world_pos					+=0.001*view;
		if((view.z<0&&world_pos.z<min_z)||(view.z>0&&world_pos.z>max_z)||distanceKm>maxCloudDistanceKm)//||solidDist_nearFar.y<lastFadeDistance)
			break;
		offsetFromOrigin			=world_pos-gridOriginPosKm;

		// Next pos.
		int3 c1						=c+c_offset;

		//find the correct d:
		vec3 p						=offsetFromOrigin/gridScale;
		vec3 p1						=c1;
		vec3 dp						=p1-p;
		vec3 D						=(dp/viewScaled);
		
		float e						=min(min(D.x,D.y),D.z);
		// All D components are positive. Only the smallest is equal to e. Step(x,y) returns (y>=x). So step(D.x,e) returns (e>=D.x), which is only true if e==D.x
		vec3 N						=step(D,vec3(e,e,e));

		int3 c_step					=c_offset*int3(N);
		float d						=e*viewScale;
		distanceKm					+=d;

		// What offset was the original position from the centre of the cube?
		p1							=p+e*viewScaled;
		//vec3 d0						=normalize(2.0*abs(frac(p1)-vec3(.5,.5,.5)));
	
		// We fade out the intermediate steps as we approach the boundary of a detail change.
		// Now sample at the end point:
		world_pos					+=d*view;
		vec3 cloudWorldOffsetKm		=world_pos-cornerPosKm;
		vec3 cloudTexCoords			=(cloudWorldOffsetKm)*inverseScalesKm;
		c							+=c_step;
		int3 intermediate			=abs(c&int3(1,1,1));
		float is_inter				=dot(N,vec3(intermediate));
		// A spherical shell, whose outer radius is W, and, wholly containing the inner box, the inner radius must be sqrt(3 (W/2)^2).
		// i.e. from 0.5*(3)^0.5 to 1, from sqrt(3/16) to 0.5, from 0.433 to 0.5
		vec3 pw						=abs(p1-p0);
		float fade_inter			=saturate((length(pw.xy)/(float(W)*(2.0-is_inter)-1.0)-start)/range);// /(2.0-is_inter)
	
		float fade					=1.0-fade_inter;
		float fadeDistance			=saturate(distanceKm/maxFadeDistanceKm);

		b							=abs(c-C0*2);
		if(fade>0)
		{
			vec3 noise_texc			=world_pos.xyz*noise3DTexcoordScale+noise3DTexcoordOffset;

			vec4 noiseval			=vec4(0,0,0,0);
			noiseval				=texture_3d_wrap_lod(noiseTexture3D,noise_texc,3.0*fadeDistance);
			vec4 density =sample_3d_lod(cloudDensity, cloudSamplerState, cloudTexCoords, fadeDistance*4.0);
			vec4 light;
			calcDensity(cloudDensity,cloudDensity,cloudTexCoords,noiseval,fractalScale,fadeDistance,density,light);
			density.z*=fade;
			if(density.z>0)
			{
				float brightness_factor;
				float cloud_density		=density.z;
				float cosine			=1;//dot(N,viewScaled);
				density.z				*=cosine;
				density.z				*=cosine;
				density.z				*=saturate(distanceKm/0.24);
				fade_texc.x				=sqrt(fadeDistance);
			
				vec3 dist				=world_pos-sourcePosKm_w;
				float radius_km			=max(source_radius,length(dist));
				float radiance			=1.0/(4.0*3.14159*radius_km*radius_km);
				vec4 clr				=vec4(spectralFluxOver1e6.rgb*radiance*density.z,density.z);
				brightness_factor		=max(1.0,radiance*max_spectral_flux);
				//clr.ba=.5;
			//	if(do_depth_mix)
				{
					vec4 clr_n=clr;
					clr.a				*=saturate((solidDist_nearFar.y-fadeDistance)/0.01);
					clr_n.a				*=saturate((solidDist_nearFar.x-fadeDistance)/0.01);
					nearColour.rgb		+=clr_n.rgb*clr_n.a*(nearColour.a);
					nearColour.a		*=(1.0-clr_n.a);
				}
				colour.rgb				+=clr.rgb*clr.a*(colour.a);
				meanFadeDistance		=lerp(meanFadeDistance,fadeDistance,colour.a*cloud_density);
				colour.a				*=(1.0-clr.a);
				if(nearColour.a*brightness_factor<0.003)
				{
					colour.a			=0.0;
					break;
				}
			}
		}
		lastFadeDistance=fadeDistance;
		if(false)
		if(max(max(b.x,b.y),0)>=W)
		{
			// We want to round c and C0 downwards. That means that 3/2 should go to 1, but that -3/2 should go to -2.
			// Just dividing by 2 gives 3/2 -> 1, and -3/2 -> -1.
			c0			=	C0;
			c			+=	start_c_offset;
			c			-=	abs(c&int3(1,1,1));
			c			=	c>>1;
			gridScale	*=	2.0;
			viewScale	*=	2.0;
			if(idx==0)
				W*=2;
			p0			=	P0;
			P0			=	startOffsetFromOrigin/gridScale/2.0;
			C0			+=	start_c_offset;
			C0			-=	abs(C0&int3(1,1,1));
			C0			=	C0>>1;
			idx			++;
		}
	}
	float rad_mult		=saturate((cos0-minCosine)/(1.0-minCosine));
	colour.rgb			=max(vec3(0,0,0),(rad_mult*colour.rgb-vec3(threshold,threshold,threshold))/(1.0+threshold));
	nearColour.rgb		=max(vec3(0,0,0),(rad_mult*nearColour.rgb-vec3(threshold,threshold,threshold))/(1.0+threshold));
    res.farColour		=vec4(exposure*colour.rgb,colour.a);
    res.nearColour		=vec4(exposure*nearColour.rgb,nearColour.a);

	return res;
}

float GetRainAtOffsetKm(Texture2D rainMapTexture,vec3 cloudWorldOffsetKm,vec3 inverseScalesKm,vec3 world_pos_km,vec2 rainCentreKm,float rainRadiusKm,float rainEdgeKm)
{
	vec3 rain_texc		=cloudWorldOffsetKm;
	rain_texc.xy		+=rain_texc.z*rainTangent;
	vec4 rain_lookup	=rainMapTexture.SampleLevel(cloudSamplerState,rain_texc.xy*inverseScalesKm.xy,0);
	//vec4 streak			=texture_wrap_lod(noiseTexture,0.00003*rain_texc.xy,0);
	return				rain_lookup.x*saturate((rainRadiusKm-length(world_pos_km.xy-rainCentreKm.xy))*3.0)
		*saturate((20.0*rain_lookup.y-cloudWorldOffsetKm.z)/0.1);//*(0.5+0.5*saturate(cloudWorldOffsetKm.z/4.0+1.0));
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
				,vec4 noiseval,float fade)
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
										,inscatter, fade);
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

	clr[NUM_CLOUD_INTERP - 1].a			*=cosine*cosine;
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
