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
											,DepthIntepretationStruct depthInterpretationStruct
											,vec4 dlookup
											,vec2 texCoords
											,bool noise
											,bool do_rain_effect
											,vec3 cloudIrRadiance1
											,vec3 cloudIrRadiance2)
{
	RaytracePixelOutput res;
	res.colour				=vec4(0,0,0,1.0);
	res.nearColour			=vec4(0,0,0,1.0);
	res.nearFarDepth		=vec4(depthToLinearDistance(dlookup.xy, depthInterpretationStruct),0,1.0);
	vec4 clip_pos			=vec4(-1.0,1.0,1.0,1.0);
	clip_pos.x				+=2.0*texCoords.x;
	clip_pos.y				-=2.0*texCoords.y;
	//float sineFactor		=1.0/length(clip_pos.xyz);
	vec3 view				=normalize(mul(invViewProj,clip_pos).xyz);

	float s					=saturate((directionToSun.z+MIN_SUN_ELEV)/0.01);
	vec3 lightDir			=lerp(directionToMoon,directionToSun,s);

	float cos0				=dot(lightDir.xyz,view.xyz);
	float sine				=view.z;

	float min_z				=cornerPosKm.z;//-(fractalScale.z*1.5)/inverseScalesKm.z;
	float max_z				=cornerPosKm.z+(1.0+fractalScale.z*1.5)/inverseScalesKm.z;
	if(do_rain_effect)
		min_z				=-1.0;
	else if(view.z<-0.01&&viewPosKm.z<cornerPosKm.z-fractalScale.z/inverseScalesKm.z)
		return res;
	
	vec2 solidDist_nearFar	=depthToFadeDistance(dlookup.yx,clip_pos.xy,depthInterpretationStruct,tanHalfFov);
	vec2 fade_texc			=vec2(0.0,0.5*(1.0-sine));
	// Lookup in the illumination texture.
	vec2 illum_texc			=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec4 illum_lookup		=texture_wrap_mirror(illuminationTexture,illum_texc);
	vec2 nearFarTexc		=illum_lookup.xy;
	float meanFadeDistance	=1.0;
	// Precalculate hg effects
	float BetaClouds		=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	float BetaRayleigh		=CalcRayleighBeta(cos0);
	float BetaMie			=HenyeyGreenstein(hazeEccentricity,cos0);

	vec4 rainbowColour		=RainbowAndCorona(rainbowLookupTexture,coronaLookupTexture,dropletRadius,
												rainbowIntensity,view,lightDir);
	float moisture			=0.0;

	vec3 world_pos			=viewPosKm;

	// This provides the range of texcoords that is lit.
	int3 c_offset			=int3(sign(view.x),sign(view.y),sign(view.z));
	int3 start_c_offset		=-c_offset;
	start_c_offset			=int3(max(start_c_offset.x,0),max(start_c_offset.y,0),max(start_c_offset.z,0));
	vec3 viewScaled			=view/scaleOfGridCoords;
	viewScaled				=normalize(viewScaled);

	vec3 offset_vec			=vec3(0,0,0);
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
	//vec3 halfway					=0.5*(lightDir-view);
	world_pos						+=offset_vec;
	//float viewScale					=length(viewScaled*scaleOfGridCoords);
	// origin of the grid - at all levels of detail, there will be a slice through this in 3 axes.
	vec3 startOffsetFromOrigin		=viewPosKm-gridOriginPosKm;
	vec3 offsetFromOrigin			=world_pos-gridOriginPosKm;
	vec3 p0							=startOffsetFromOrigin/scaleOfGridCoords;
	int3 c0							=int3(floor(p0) + start_c_offset);
	//vec3 gridScale					=scaleOfGridCoords;
	//vec3 P0							=startOffsetFromOrigin/scaleOfGridCoords/2.0;
	//int3 C0							=c0>>1;
	
	float distanceKm				=length(offset_vec);
	vec3 p							=offsetFromOrigin/scaleOfGridCoords;
	//int3 c							=int3(floor(p) + start_c_offset);
	
	//int idx=0;
	//float W							=halfClipSize;
	const float start				=0.866*0.866;//0.707 for 2D, 0.866 for 3D;
	const float ends				=1.0;
	//const float range				=ends-start;
	//vec3 volume_texc				=ScreenToVolumeTexcoords(clipPosToScatteringVolumeMatrix,texCoords,0.0);


	vec4 colour						=vec4(0.0,0.0,0.0,1.0);
	vec4 nearColour					=vec4(0.0,0.0,0.0,1.0);
	float lastFadeDistance			=0.0;
	//
	float stepLengthKm	=1.0/inverseScalesKm.z/30.0;
	//float blinn_phong=0.0;
	bool found=false;
	for(int i=0;i<255;i++)
	{
		world_pos					+=0.001*view;
		if((view.z<0&&world_pos.z<min_z)||(view.z>0&&world_pos.z>max_z)||distanceKm>maxCloudDistanceKm||solidDist_nearFar.y<lastFadeDistance)
			break;
		offsetFromOrigin			=world_pos-gridOriginPosKm;
		distanceKm					+=stepLengthKm;
	
		// We fade out the intermediate steps as we approach the boundary of a detail change.
		// Now sample at the end point:
		world_pos					+=stepLengthKm*view;
		vec3 cloudWorldOffsetKm		=world_pos-cornerPosKm;
		vec3 cloudTexCoords			=(cloudWorldOffsetKm)*inverseScalesKm;
		// A spherical shell, whose outer radius is W, and, wholly containing the inner box, the inner radius must be sqrt(3 (W/2)^2).
		// i.e. from 0.5*(3)^0.5 to 1, from sqrt(3/16) to 0.5, from 0.433 to 0.5
	
		float fade					=1.0;
		float fadeDistance			=saturate(distanceKm/maxFadeDistanceKm);
		if(fade>0)
		{
			if(!found)
			{
				vec4 density		=sample_3d_lod(cloudDensity,cloudSamplerState,cloudTexCoords,0);
				found				=found||(density.z>0);
			}
			if(found)
			{
				vec3 noise_texc			=world_pos.xyz*noise3DTexcoordScale+noise3DTexcoordOffset;

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
					density.z*=0.85;
	vec4 worley		=texture_wrap_lod(smallWorleyTexture3D,world_pos.xyz/worleyScale,0);
					//density.z		=saturate(4.0*density.z-0.2);
	density.z		=saturate((1.0)*density.z	+worleyNoise*0.25*((worley.x-1)+(worley.y-1)+(worley.z-1)+(worley.w-1)));
					float brightness_factor;
					density.z				*=saturate(distanceKm/0.24);
					fade_texc.x				=sqrt(fadeDistance);
					vec3 volumeTexCoords	=vec3(texCoords,fade_texc.x);//*sineFactor);
					vec4 clr;
					// The "normal" that the ray has hit is equal to N, but with the negative signs of the components of viewScaled or view.
					

					if (noise)
						clr					=calcColour(lossTexture,inscatterVolumeTexture,volumeTexCoords,lightTableTexture
														,density
														,BetaClouds
														,lightResponse
														,ambientColour
														,world_pos
														,cloudTexCoords
														,fade_texc
														,brightness_factor);
					else
						clr					=calcColourSimple(lossTexture,inscTexture,skylTexture,lightTableTexture
														,density
														,BetaClouds,BetaRayleigh,BetaMie
														,lightResponse
														,ambientColour
														,world_pos
														,cloudTexCoords
														,fade_texc
														,nearFarTexc
														,brightness_factor);
					if(do_depth_mix)
					{
						vec4 clr_n			=clr;
						vec2 m				=saturate((solidDist_nearFar.xy-vec2(fadeDistance,fadeDistance))*100.0);
						clr.a				*=m.y;
						clr_n.a				*=m.x;
						nearColour.rgb		+=clr_n.rgb*clr_n.a*(nearColour.a);
						nearColour.a		*=(1.0-clr_n.a);
					}
					colour.rgb				+=clr.rgb*clr.a*(colour.a);
					meanFadeDistance		=lerp(meanFadeDistance,fadeDistance,saturate(4.0*density.z)*colour.a);
					colour.a				*=(1.0-clr.a);
					if(nearColour.a*brightness_factor<0.003)
					{
						colour.a = 0.0;
						break;
					}
				}
			}
		}
		lastFadeDistance=fadeDistance;
	}
    res.colour			=vec4(colour.rgb,colour.a);
    res.nearColour		=vec4(nearColour.rgb,nearColour.a);
#ifndef INFRARED
	res.colour.rgb		+=saturate(moisture)*sunlightColour1.rgb/25.0*rainbowColour.rgb;
#endif
	res.nearFarDepth.z	=dlookup.z;//*(1.0-colour.a);
	res.nearFarDepth.w	=meanFadeDistance;
	return res;
}
#endif
