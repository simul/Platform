//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef CLOUDS_SIMPLE_SL
#define CLOUDS_SIMPLE_SL

RaytracePixelOutput RaytraceCloudsStatic(Texture3D cloudDensity
											,Texture2D rainMapTexture
											,Texture3D noiseTexture3D
											,Texture2D lightTableTexture
											,Texture2D illuminationTexture
											,Texture2D rainbowLookupTexture
											,Texture2D coronaLookupTexture
											,Texture2D lossTexture
											,Texture2D inscTexture
											,Texture2D skylTexture
											,Texture3D inscatterVolumeTexture
                                            ,bool do_depth_mix
											,vec4 dlookup
											,vec3 view
											,vec4 clip_pos
											,vec3 volumeTexCoordsXyC
											,bool noise
											,bool do_rain_effect
											,bool do_rainbow
											,vec3 cloudIrRadiance1
											,vec3 cloudIrRadiance2)
{
	RaytracePixelOutput res;
	res.colour				=vec4(0,0,0,1.0);
	res.nearColour			=vec4(0,0,0,1.0);
	res.nearFarDepth		=dlookup;

	float s					=saturate((directionToSun.z+MIN_SUN_ELEV)/0.01);
	vec3 lightDir			=lerp(directionToMoon,directionToSun,s);

	float cos0				=dot(lightDir.xyz,view.xyz);
	float sine				=view.z;

	float min_z				=cornerPosKm.z-(fractalScale.z*1.5)/inverseScalesKm.z;
	float max_z				=cornerPosKm.z+(1.0+fractalScale.z*1.5)/inverseScalesKm.z;
	if(do_rain_effect)
		min_z				=-1.0;

	else if(view.z<-0.01&&viewPosKm.z<cornerPosKm.z-fractalScale.z/inverseScalesKm.z)
		return res;
	
	vec2 solidDist_nearFar	=(dlookup.yx)+100.0*step(vec2(1.0,1.0), dlookup.yx);
	vec2 fade_texc			=vec2(0.0,0.5*(1.0-sine));
	// Lookup in the illumination texture.
	vec2 illum_texc			=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec4 illum_lookup		=texture_wrap_mirror(illuminationTexture,illum_texc);
	vec2 nearFarTexc		=illum_lookup.xy;
	float meanFadeDistance	=1.0;
	float minDistance		=1.0;
	float maxDistance		=0.0;
	// Precalculate hg effects
	float BetaClouds		=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	float BetaRayleigh		=CalcRayleighBeta(cos0);
	float BetaMie			=HenyeyGreenstein(hazeEccentricity,cos0);

	vec4 rainbowColour;
	if(do_rainbow)
		rainbowColour=RainbowAndCorona(rainbowLookupTexture,coronaLookupTexture,dropletRadius,
												rainbowIntensity,view,lightDir);
	float moisture			=0.0;

	vec3 world_pos			=viewPosKm;
	view.xyz+=vec3(0.00001,0.00001,0.00001);
	// This provides the range of texcoords that is lit.
	// In c_offset, we want 1's or -1's. NEVER zeroes!
	int3 c_offset			=int3(2.0*step(vec3(0,0,0),view.xyz)-vec3(1,1,1));
	int3 start_c_offset		=-c_offset;
	start_c_offset			=int3(max(start_c_offset.x,0),max(start_c_offset.y,0),max(start_c_offset.z,0));
	vec3 viewScaled			=view/scaleOfGridCoords;
	viewScaled				=normalize(viewScaled);

	vec3 offset_vec			=vec3(0,0,0);
	//if(world_pos.z<min_z)
	{
		float a		=1.0/(saturate(view.z)+0.00001);
		offset_vec	+=max(0.0,min_z-world_pos.z)*vec3(view.x*a,view.y*a,1.0);
	}
	//if(view.z<0&&world_pos.z>max_z)
	{
		float a		=1.0/(saturate(-view.z)+0.00001);
		offset_vec	+=max(0.0,world_pos.z-max_z)*vec3(view.x*a,view.y*a,-1.0);
	}
	vec3 halfway					=0.5*(lightDir-view);
	world_pos						+=offset_vec;
	float viewScale					=length(viewScaled*scaleOfGridCoords);
	vec3 gridOriginPos				=gridOriginPosKm;//+scaleOfGridCoords*2.0*texture_3d_wrap_lod(noiseTexture3D,frac(view*12.0),0).xyz;
	// origin of the grid - at all levels of detail, there will be a slice through this in 3 axes.
	vec3 startOffsetFromOrigin		=viewPosKm-gridOriginPos;
	vec3 offsetFromOrigin			=world_pos-gridOriginPos;
	vec3 p0							=startOffsetFromOrigin/scaleOfGridCoords;
	int3 c0							=int3(floor(p0) + start_c_offset);
	vec3 gridScale					=scaleOfGridCoords;
	vec3 P0							=startOffsetFromOrigin/scaleOfGridCoords/2.0;
	int3 C0							=c0>>1;
	
	float distanceKm				=length(offset_vec);
	vec3 p_							=offsetFromOrigin/scaleOfGridCoords;
	int3 c							=int3(floor(p_) + start_c_offset);
	int idx=0;
	float W							=halfClipSize;
	const float start				=0.866*0.866;//0.707 for 2D, 0.866 for 3D;
	const float ends				=1.0;
	const float range				=ends-start;

	vec4 colour						=vec4(0.0,0.0,0.0,1.0);
	vec4 nearColour					=vec4(0.0,0.0,0.0,1.0);
	float lastFadeDistance			=0.0;
	int3 b							=abs(c-C0*2);

	//float blinn_phong=0.0;
	//bool found=false;
	float distScale =  0.6 / maxFadeDistanceKm;
		float K=log(maxCloudDistanceKm);
		bool found=false;
		float stepKm				=K*(1.2+distanceKm)/512.0;
	for(int i=0;i<768;i++)
	{
		//world_pos					+=0.001*view;
		if((view.z<0&&world_pos.z<min_z)||(view.z>0&&world_pos.z>max_z)||distanceKm>maxCloudDistanceKm)//||solidDist_nearFar.y<lastFadeDistance)
			break;
		offsetFromOrigin			=world_pos-gridOriginPos;

		// Next pos.
		int3 c1						=c+c_offset;//viewScale;
		stepKm						*=(1.0+K/512.0);
		distanceKm					+=stepKm;
		// We fade out the intermediate steps as we approach the boundary of a detail change.
		// Now sample at the end point:
		world_pos					+=stepKm*view;
		vec3 cloudWorldOffsetKm		=world_pos-cornerPosKm;
		vec3 cloudTexCoords			=(cloudWorldOffsetKm)*inverseScalesKm;
		float fade					=1.0;
		float fadeDistance			=saturate(distanceKm/maxFadeDistanceKm);

		// maxDistance is the furthest we can *see*.
		maxDistance					=max(fadeDistance,maxDistance);

		if(fade>0)
		{
			if(!found)
			{
				vec4 density		=sample_3d_lod(cloudDensity,cloudSamplerState,cloudTexCoords,1);
				found				=found||(density.z>0);
				if(found)
				{
			//		distanceKm		-=1.0*stepKm;
				//	stepKm/=2.0;
			//		continue;
				}
			}
			if(found)
			{
				vec3 noise_texc			=(world_pos.xyz)*noise3DTexcoordScale+noise3DTexcoordOffset;

				vec4 noiseval			=vec4(0,0,0,0);
				if(noise)
					noiseval			=texture_3d_wrap_lod(noiseTexture3D,noise_texc,3.0*fadeDistance);
				vec4 density			=calcDensity(cloudDensity,cloudTexCoords,fade,noiseval,fractalScale);
				if(do_rain_effect)
				{
					// The rain fall angle is used:
					float dm			=rainEffect*fade*GetRainAtOffsetKm(rainMapTexture,cloudWorldOffsetKm,inverseScalesKm, world_pos, rainCentreKm, rainRadiusKm);
					moisture			+=0.01*dm*density.x;
					density.z			=saturate(density.z+dm);
				}
				if(density.z>0)
				{
					minDistance		=min(max(0,fadeDistance-density.z*stepKm/maxFadeDistanceKm), minDistance);
					vec4 worley		=texture_wrap_lod(smallWorleyTexture3D,world_pos.xyz/worleyScale,0);
					//density.z		=saturate(4.0*density.z-0.2);
					float wo		=worleyNoise*(worley.x+worley.y+worley.z+worley.w-0.6*(1.0+0.5+0.25+0.125));
					density.z		=saturate((density.z+wo)*(1.0+alphaSharpness)-alphaSharpness);
					//density.xy		*=1.0+wo;
					float brightness_factor;
					fade_texc.x				=sqrt(fadeDistance);
					vec3 volumeTexCoords	=vec3(volumeTexCoordsXyC.xy,fade_texc.x);

					ColourStep( colour, nearColour, meanFadeDistance, brightness_factor
								, lossTexture, inscTexture, skylTexture, inscatterVolumeTexture, lightTableTexture
								,density, distanceKm, fadeDistance
								,world_pos
								,cloudTexCoords, fade_texc, nearFarTexc
								,1.0, volumeTexCoords
								,BetaClouds, BetaRayleigh, BetaMie
								,solidDist_nearFar, noise, do_depth_mix,distScale);
					if(nearColour.a*brightness_factor<0.003)
					{
						nearColour.a=colour.a = 0.0;
						break;
					}
				}
				lastFadeDistance = lerp(lastFadeDistance, fadeDistance - density.z*stepKm / maxFadeDistanceKm, colour.a);
			}
		}
	}
    res.colour			=colour;
    res.nearColour		=nearColour;
#ifndef INFRARED
	if(do_rainbow)
		res.colour.rgb		+=saturate(moisture)*sunlightColour1.rgb/25.0*rainbowColour.rgb;
#endif
	//res.nearFarDepth.y = solidDist_nearFar.x;
	// y is the near depth. x is the distance at which we will interpolate to the far depth value.
	// Instead of using the far depth, we will use the cloud distance.
//	res.nearFarDepth.y = max(res.nearFarDepth.y,minDistance);
//	res.nearFarDepth.x = min(res.nearFarDepth.x,max(lastFadeDistance, res.nearFarDepth.y + distScale ));
	//res.nearFarDepth.w	=	meanFadeDistance;
	//res.nearFarDepth.z	=	max(0.0000001,res.nearFarDepth.x-meanFadeDistance);// / maxFadeDistanceKm;// min(res.nearFarDepth.y, max(res.nearFarDepth.x + distScale, minDistance));// min(distScale, minDistance);
	return res;
}
#endif
