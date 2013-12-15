#ifndef CLOUDS_SL
#define CLOUDS_SL

#ifndef GLSL
Texture3D cloudDensity1					: register(t0);
Texture3D cloudDensity2					: register(t1);
Texture2D noiseTexture					: register(t2);
Texture2D cloudShadowTexture			: register(t2);
Texture2D lossTexture					: register(t3);
Texture2D inscTexture					: register(t4);
Texture2D skylTexture					: register(t5);
Texture2D depthTexture					: register(t6);
Texture3D noiseTexture3D				: register(t7);
Texture3D lightningIlluminationTexture	: register(t8);
Texture3D cloudDensity					: register(t9);
Texture2D illuminationTexture			: register(t10);
Texture2D lightTableTexture				: register(t11);
SamplerState cloudSamplerState			: register(s0);
#endif

#define MIN_SUN_ELEV (0.2)
struct RaytracePixelOutput
{
	vec4 colour SIMUL_TARGET_OUTPUT;
	float depth	SIMUL_DEPTH_OUTPUT;
};

vec4 calcDensity(Texture3D cloudDensity1,Texture3D cloudDensity2,vec3 texc,float layerFade,float cloud_interp)
{
	vec4 density1=sampleLod(cloudDensity1,cloudSamplerState,texc,0);
	vec4 density2=sampleLod(cloudDensity2,cloudSamplerState,texc,0);
	vec4 density=lerp(density1,density2,cloud_interp);
	density.z*=layerFade;
	density.z=saturate(density.z*(1.f+alphaSharpness)-alphaSharpness);
	return density;
}

vec4 calcDensity(Texture3D cloudDensity1,Texture3D cloudDensity2,vec3 texCoords,float layerFade,vec3 noiseval,vec3 fractalScale,float cloud_interp)
{
	vec3 pos=texCoords.xyz+fractalScale.xyz*noiseval;
	vec4 density1=sampleLod(cloudDensity1,cloudSamplerState,pos,0);
	vec4 density2=sampleLod(cloudDensity2,cloudSamplerState,pos,0);
	vec4 density=lerp(density1,density2,cloud_interp);
	density.z*=layerFade;
	//density.z+=0.2*(noiseval-1.0);
	///density.z*=step(alphaSharpness,density.z);
	density.z=saturate(density.z*(1.f+alphaSharpness)-alphaSharpness);
	//density.z=saturate(density.z);
	return density;
}

vec4 calcColour(vec4 density,float cos0,vec4 lightResponse,vec3 combinedLightColour,vec3 ambientColour)
{
	float Beta=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	vec3 ambient=density.w*ambientColour.rgb;
	float opacity=density.z;
	vec4 final;
	final.rgb=(density.y*Beta+lightResponse.y*density.x)*combinedLightColour+ambient.rgb;
	final.a=opacity;
	return final;
}

vec4 calcUnfadedColour(Texture3D cloudDensity1,Texture3D cloudDensity2,vec3 texCoords,float layerFade,vec3 noiseval,vec3 fractalScale,float cloud_interp,vec4 lightResponse,vec3 combinedLightColour,vec3 ambientColour,float cos0)
{
	vec4 density=calcDensity(cloudDensity1,cloudDensity2,texCoords,layerFade,noiseval,fractalScale,cloud_interp);
	if(density.z<=0)
		discard;
	vec4 final=calcColour(density,cos0,lightResponse,combinedLightColour,ambientColour);
	return final;
}

vec3 applyFades(vec3 final,vec2 fade_texc,float cos0,float earthshadowMultiplier)
{
	vec4 l=sampleLod(lossTexture		,cmcSamplerState,fade_texc,0);
	vec3 loss=l.rgb;
	vec4 insc=sampleLod(inscTexture		,cmcSamplerState,fade_texc,0);
	vec3 skyl=sampleLod(skylTexture		,cmcSamplerState,fade_texc,0).rgb;
	vec3 inscatter=earthshadowMultiplier*InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	final*=loss;
#ifdef INFRARED
	final=skyl.rgb;
#else
	final+=skyl+inscatter;
#endif
    return final;
}

vec4 CloudShadow(Texture3D cloudDensity1,Texture3D cloudDensity2,vec2 texCoords,mat4 shadowMatrix,vec3 cornerPos,vec3 inverseScales)
{
//for this texture, let x be the square root of distance and y be the angle anticlockwise from the x-axis.
#ifdef RADIAL_CLOUD_SHADOW
	float theta						=texCoords.y*2.0*3.1415926536;
	float distance_off_centre		=pow(texCoords.x,2.0);
	vec2 pos_xy						=distance_off_centre*vec2(cos(theta),sin(theta));
#else
	vec2 pos_xy						=2.0*texCoords.xy-1.0;
	//float distance_off_centre		=length(pos_xy);
#endif
	vec2 illumination				=vec2(1.0,1.0);
	float U							=-1.0;
	#define NUM_STEPS 12
	vec3 cartesian_1				=vec3(pos_xy.xy,1.0);
	vec3 wpos_1						=mul(shadowMatrix,vec4(cartesian_1,1.0)).xyz;
	vec3 cartesian_2				=vec3(pos_xy.xy,0.0);
	vec3 wpos_2						=mul(shadowMatrix,vec4(cartesian_2,1.0)).xyz;
	vec3 texc_1						=(wpos_1-cornerPos)*inverseScales;
	vec3 texc_2						=(wpos_2-cornerPos)*inverseScales;
	for(int i=0;i<NUM_STEPS;i++)
	{
		//float u					=1.0-float(i)/float(NUM_STEPS);
		float u						=float(i)/float(NUM_STEPS);
		//vec3 cartesian			=vec3(pos_xy.xy,u);
		//vec3 wpos					=mul(shadowMatrix,vec4(cartesian,1.0)).xyz;
		vec3 texc					=lerp(texc_1,texc_2,u);//(wpos-cornerPos)*inverseScales;
		vec4 density1				=sampleLod(cloudDensity1,cloudSamplerState,texc,0);
		vec4 density2				=sampleLod(cloudDensity2,cloudSamplerState,texc,0);
		vec4 density				=lerp(density1,density2,cloud_interp);
		if(density.z>0)
		{
			illumination			=lerp(illumination,vec2(0,0),density.z);//density.xy
			U						=lerp(U,u,density.z);
		}
	}
	vec3 simple_texc				=vec3(texCoords,0);
	vec2 shadow						=lerp(sampleLod(cloudDensity1,wwcSamplerState,simple_texc,0).xy,sampleLod(cloudDensity2,wwcSamplerState,simple_texc,0).xy,cloud_interp);
	return vec4(illumination,U,0.5*(shadow.x+shadow.y));//*edge
}

// from the viewer, trace outwards to find the outer and inner ranges of cloud shadow.
// Then the outer and inner shadow distances are put in the xy.
// Within that, the outer and inner lit distances are put in the zw.
vec4 CloudShadowNearFar(Texture2D cloudShadowTexture,int shadowTextureSize,vec2 texCoords)
{
	vec2 shadow_range				=vec2(1.0,0.0);
	vec2 light_range				=vec2(1.0,0.0);
	const float U					=1.0;
	const float L					=0.0;
	int N							=1*shadowTextureSize;
	float pixel						=1.0/float(shadowTextureSize);
#ifdef RADIAL_CLOUD_SHADOW
	vec2 offset						=vec2(0,pixel/1.0);
#else
//for this texture, let x be the square root of distance and y be the angle anticlockwise from the x-axis.
	float theta						=texCoords.x*2.0*3.1415926536;
	vec2 offset						=vec2(-sin(theta),cos(theta))*pixel/4.0;
#endif
	// First find the range where there is ANY shadow:
	for(int i=0;i<N;i++)
	{
		float interp				=float(i)/float(N-1);
#ifdef RADIAL_CLOUD_SHADOW
		vec2 shadow_texc			=vec2(sqrt(interp),texCoords.x);
#else
		float distance_off_centre	=interp;
		vec2 shadow_texc			=0.5*(distance_off_centre*vec2(cos(theta),sin(theta))+1.0);
#endif
		vec4 illumination			=sampleLod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc,0);
		illumination				+=sampleLod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc-offset,0);
		illumination				+=sampleLod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc+offset,0);
		
		if(illumination.y<3.0*U)
		{
			if(interp<shadow_range.x)
				shadow_range.x=interp-pixel;
			shadow_range.y=interp+pixel;
		}
	}
	shadow_range=saturate(shadow_range);
	//int in_light=0;
	// Second, within this range, find where there is ANY light.
	//if(shadow_range.x<=0&&shadow_range.y>=1.0)
	for(int j=0;j<N;j++)
	{
		float interp					=float(j)/float(N-1);
#ifdef RADIAL_CLOUD_SHADOW
		vec2 shadow_texc				=vec2(sqrt(interp),texCoords.x);
#else
		float distance_off_centre		=interp;
		vec2 shadow_texc				=0.5*(distance_off_centre*vec2(cos(theta),sin(theta))+1.0);
#endif
		vec4 illumination				=texture_cwc_lod(cloudShadowTexture,shadow_texc,0);
		illumination					+=texture_cwc_lod(cloudShadowTexture,shadow_texc-offset,0);
		illumination					+=texture_cwc_lod(cloudShadowTexture,shadow_texc+offset,0);
	//	if(interp>=shadow_range.x&&interp<=shadow_range.y)
		{
			if(illumination.y>L*3.0)
			{
				if(interp<light_range.x)
					light_range.x=interp-pixel;
				light_range.y=interp+pixel;
			}
		}
	}
	light_range=saturate(light_range);
	return vec4(shadow_range,light_range);
}


vec4 GodraysAccumulation(Texture2D cloudShadowTexture,int shadowTextureSize,vec2 texCoords)
{
	//const float U					=1.0;
	//const float L					=0.0;
	int N							=int(texCoords.y*float(shadowTextureSize));
	float pixel						=1.0/float(shadowTextureSize);
#ifdef RADIAL_CLOUD_SHADOW
	vec2 offset						=vec2(0,pixel/1.0);
#else
//for this texture, let x be the square root of distance and y be the angle anticlockwise from the x-axis.
	float theta						=texCoords.x*2.0*3.1415926536;
	vec2 offset						=vec2(-sin(theta),cos(theta))*pixel/4.0;
#endif
	vec4 total_ill					=vec4(0,0,0,0);
	// Find the total illumination
	for(int i=0;i<N;i++)
	{
		float interp				=float(i)/float(shadowTextureSize-1);
#ifdef RADIAL_CLOUD_SHADOW
		vec2 shadow_texc			=vec2(sqrt(interp),texCoords.x);
#else
		float distance_off_centre	=interp;
		vec2 shadow_texc			=0.5*(distance_off_centre*vec2(cos(theta),sin(theta))+1.0);
#endif
		vec4 illumination			=sampleLod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc,0);
		illumination				+=sampleLod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc-offset,0);
		illumination				+=sampleLod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc+offset,0);
		total_ill					+=illumination.xxxx;
	}
	return total_ill/float(shadowTextureSize);
}

vec4 ShowCloudShadow(Texture2D cloudShadowTexture,Texture2D cloudGodraysTexture,vec2 texCoords)
{
	vec2 tex_pos=2.0*texCoords.xy-vec2(1.0,1.0);
	float dist=length(tex_pos.xy);
	vec2 radial_texc=vec2(sqrt(length(tex_pos.xy)),atan2(tex_pos.y,tex_pos.x)/(2.0*3.1415926536));
#ifdef RADIAL_CLOUD_SHADOW
    vec4 lookup=texture_cwc_lod(cloudShadowTexture,radial_texc,0);
#else
    vec4 lookup=texture_clamp_lod(cloudShadowTexture,texCoords.xy,0);
#endif
	lookup*=0.5;
	if(radial_texc.y<0)
		radial_texc.y+=1.0;
	vec4 godrays_illum=texture_wrap_clamp(cloudGodraysTexture,vec2(radial_texc.y,dist));
	lookup.rgb+=godrays_illum.xxx;
	return vec4(lookup.rgb,1.0);
}

vec3 applyFades2(vec3 final,vec2 fade_texc,float BetaRayleigh,float BetaMie,float earthshadowMultiplier)
{
	vec3 loss		=sampleLod(lossTexture		,cmcSamplerState,fade_texc,0).rgb;
	vec3 skyl		=sampleLod(skylTexture		,cmcSamplerState,fade_texc,0).rgb;
	final			*=loss;
#ifdef INFRARED
	final			=skyl.rgb;
#else
	vec4 insc		=sampleLod(inscTexture		,cmcSamplerState,fade_texc,0);
	vec3 inscatter	=earthshadowMultiplier*PrecalculatedInscatterFunction(insc,BetaRayleigh,BetaMie,mieRayleighRatio);
	final			+=skyl+inscatter;
#endif
    return final;
}

vec4 calcColour2(vec4 density,float Beta,vec4 lightResponse,vec3 combinedLightColour,vec3 ambientColour)
{
	vec3 ambient=density.w*ambientColour.rgb;
	float opacity=density.z;
	vec4 final;
	final.rgb=(density.y*Beta+lightResponse.y*density.x)*combinedLightColour+ambient.rgb;
	final.a=opacity;
	return final;
}


float unshadowedBrightness(float Beta,vec4 lightResponse,vec3 ambientColour)
{
	float final			=max(1.0,(Beta+lightResponse.y)+ambientColour.b);
	return final;
}

#ifndef GLSL
RaytracePixelOutput RaytraceCloudsForward3DNoise(Texture3D cloudDensity1
												,Texture3D cloudDensity2
												,Texture3D noiseTexture3D
												,Texture2D depthTexture
												,Texture2D lightTableTexture
												,vec2 texCoords)
{
	float dlookup 		=sampleLod(depthTexture,clampSamplerState,viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias),0).r;
	vec4 clip_pos		=vec4(-1.f,1.f,1.f,1.f);
	clip_pos.x			+=2.0*texCoords.x;
	clip_pos.y			-=2.0*texCoords.y;

	float s				=saturate((directionToSun.z+MIN_SUN_ELEV)/0.01);
	vec3 lightDir		=lerp(directionToMoon,directionToSun,s);

	vec3 view			=normalize(mul(invViewProj,clip_pos).xyz);
	float cos0			=dot(lightDir.xyz,view.xyz);
	float sine			=view.z;
	vec3 n				=vec3(clip_pos.xy*tanHalfFov,1.0);
	n					=normalize(n);
	//vec2 noise_texc_0	=mul(noiseMatrix,vec4(n.xy,0,0)).xy;

	float min_texc_z	=-fractalScale.z*1.5;
	float max_texc_z	=1.0-min_texc_z;

	float depth			=dlookup;
	float d				=depthToFadeDistance(depth,clip_pos.xy,nearZ,farZ,tanHalfFov);
	vec4 colour			=vec4(0.0,0.0,0.0,1.0);
	vec2 fade_texc		=vec2(0.0,0.5*(1.0-sine));

	// Lookup in the illumination texture.
	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec4 illum_lookup	=texture_wrap_mirror(illuminationTexture,illum_texc);
	vec2 nearFarTexc	=illum_lookup.xy;

	float meanFadeDistance		=0.0;
	//float up			=step(0.1,sine);
	//float down			=step(0.1,-sine);
	// Precalculate hg effects
	float BetaClouds	=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	float BetaRayleigh	=0.0596831*(1.0+cos0*cos0);
	float BetaMie		=HenyeyGreenstein(hazeEccentricity,cos0);
#ifndef USE_LIGHT_TABLES
	vec3 combinedLightColour=sunlightColour1.rgb;
	vec3 ambientLightColour	=ambientColour.rgb;
#endif
	// This provides the range of texcoords that is lit.
	for(int i=0;i<layerCount;i++)
	{
		vec4 density			=vec4(0,0,0,0);
		const LayerData layer	=layers[i];
		float layerWorldDist	=layer.layerDistance;
		float fadeDistance		=saturate(layerWorldDist/maxFadeDistanceMetres);
		vec3 world_pos			=viewPos+layerWorldDist*view;
		world_pos.z				-=layer.verticalShift;
		vec3 layerTexCoords		=(world_pos-cornerPos)*inverseScales;
		float layerFade			=layer.layerFade;//*saturate((abs(sine)-layer.sine_threshold)/layer.sine_range);
		if(layerFade>0&&fadeDistance<=d&&layerTexCoords.z>=min_texc_z&&layerTexCoords.z<=max_texc_z)
		{
			float noise_factor		=lerp(baseNoiseFactor,1.0,saturate(layerTexCoords.z));
			vec3 noiseval			=vec3(0.0,0.0,0.0);
			vec3 noise_texc			=layerTexCoords.xyz*noise3DTexcoordScale;
			float mult=0.5;
			for(int j=0;j<4;j++)
			{
				noiseval			+=(texture_wrap_lod(noiseTexture3D,noise_texc,0).xyz)*mult;
				noise_texc			*=2.0;
				mult				*=noise3DPersistence;
			}
			noiseval				*=noise_factor;
			density					=calcDensity(cloudDensity1,cloudDensity2,layerTexCoords,layerFade,noiseval,fractalScale,cloud_interp);
			density.z				*=saturate((d-fadeDistance)/0.001);
		}
		if(density.z>0)
		{
#ifdef USE_LIGHT_TABLES
			float alt_texc			=world_pos.z/maxAltitudeMetres;
			vec3 combinedLightColour=texture_clamp_lod(lightTableTexture,vec2(alt_texc,3.5/4.0),0).rgb;
			vec3 ambientLightColour	=lightResponse.w*texture_clamp_lod(lightTableTexture,vec2(alt_texc,2.5/4.0),0).rgb;
#endif
			float brightness_factor	=unshadowedBrightness(BetaClouds,lightResponse,ambientLightColour);
			vec4 c					=calcColour2( density,BetaClouds,lightResponse,combinedLightColour,ambientLightColour);
			
			fade_texc.x				=sqrt(fadeDistance);
			float sh				=saturate((fade_texc.x-nearFarTexc.x)/0.1);
			c.rgb					*=sh;
			c.rgb					=applyFades2(c.rgb,fade_texc,BetaRayleigh,BetaMie,sh);
			colour.rgb				+=c.rgb*c.a*(colour.a);
			// depth here:
			meanFadeDistance					+=fadeDistance*c.a*colour.a;
			colour.a				*=(1.0-c.a);
			if(colour.a*brightness_factor<0.003)
			{
				colour.a	=0.0;
				//meanFadeDistance		=fadeDistance;//lerp(meanFadeDistance,fadeDistance,depthMix);
				break;
			}
		}
	}
	if(colour.a>=1.0)
	   discard;
	meanFadeDistance+=colour.a;
	RaytracePixelOutput res;
    res.colour		=vec4(exposure*colour.rgb,colour.a);
	res.depth		=fadeDistanceToDepth(meanFadeDistance,clip_pos.xy,nearZ,farZ,tanHalfFov);
	return res;
}

RaytracePixelOutput RaytraceCloudsForward(Texture3D cloudDensity1
											,Texture3D cloudDensity2
											,Texture2D noiseTexture
											,Texture2D depthTexture
											,Texture2D lightTableTexture
                                            ,bool do_depth_mix
											,vec2 texCoords
											,bool near_pass
											,bool noise)
{
	vec4 dlookup 			=sampleLod(depthTexture,samplerStateNearest,viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias),0);
	vec4 clip_pos			=vec4(-1.f,1.f,1.f,1.f);
	clip_pos.x				+=2.0*texCoords.x;
	clip_pos.y				-=2.0*texCoords.y;
	vec3 view				=normalize(mul(invViewProj,clip_pos).xyz);

	float s					=saturate((directionToSun.z+MIN_SUN_ELEV)/0.01);
	vec3 lightDir			=lerp(directionToMoon,directionToSun,s);

	float cos0				=dot(lightDir.xyz,view.xyz);
	float sine				=view.z;
	vec3 n					=vec3(clip_pos.xy*tanHalfFov,1.0);
	n						=normalize(n);
	vec2 noise_texc_0		=mul(noiseMatrix,vec4(n.xy,0,0)).xy/fractalRepeatLength;

	float min_texc_z		=-fractalScale.z*1.5;
	float max_texc_z		=1.0-min_texc_z;

	float depth;
	if(near_pass)
	{
		if(dlookup.z==0)
			discard;
		depth=dlookup.y;
	}
	else
	{
		depth=dlookup.x;
	}
	float d					=depthToFadeDistance(depth,clip_pos.xy,nearZ,farZ,tanHalfFov);
	vec4 colour				=vec4(0.0,0.0,0.0,1.0);
	vec2 fade_texc			=vec2(0.0,0.5*(1.0-sine));

	// Lookup in the illumination texture.
	vec2 illum_texc			=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec4 illum_lookup		=texture_wrap_mirror(illuminationTexture,illum_texc);
	vec2 nearFarTexc		=illum_lookup.xy;

	float meanFadeDistance	=0.0;
	// Precalculate hg effects
	float BetaClouds		=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	float BetaRayleigh		=0.0596831*(1.0+cos0*cos0);
	float BetaMie			=HenyeyGreenstein(hazeEccentricity,cos0);
#ifndef USE_LIGHT_TABLES	
	vec3 amb				=ambientColour.rgb;
#endif
	// This provides the range of texcoords that is lit.
	for(int i=0;i<layerCount;i++)
	{
		vec4 density				=vec4(0,0,0,0);
		const LayerData layer		=layers[i];
		float layerWorldDist		=layer.layerDistance;
		float fadeDistance			=saturate(layerWorldDist/maxFadeDistanceMetres);
		vec3 world_pos				=viewPos+layerWorldDist*view;
		world_pos.z					-=layer.verticalShift;
		vec3 layerTexCoords			=(world_pos-cornerPos)*inverseScales;
		float layerFade				=layer.layerFade;//*saturate((abs(sine)-layer.sine_threshold)/layer.sine_range);
		if(layerFade>0&&(fadeDistance<=d||!do_depth_mix)&&layerTexCoords.z>=min_texc_z&&layerTexCoords.z<=max_texc_z)
		{
			vec3 noiseval			=vec3(0,0,0);
			if(noise)
			{
				float noise_factor		=lerp(baseNoiseFactor,1.0,saturate(layerTexCoords.z));
				vec2 noise_texc			=noise_texc_0*layerWorldDist+layer.noiseOffset;
				vec3 noiseval			=noise_factor*texture_wrap_lod(noiseTexture,noise_texc,0).xyz;
			}
			density					=calcDensity(cloudDensity1,cloudDensity2,layerTexCoords,layer.layerFade,noiseval,fractalScale,cloud_interp);
            if(do_depth_mix)
				density.z				*=saturate((d-fadeDistance)/0.01);
			if(density.z>0)
			{
#ifdef USE_LIGHT_TABLES
				float alt_texc			=world_pos.z/maxAltitudeMetres;
				vec3 combinedLightColour=texture_clamp_lod(lightTableTexture,vec2(alt_texc,3.5/4.0),0).rgb;
				vec3 amb				=lightResponse.w*texture_clamp_lod(lightTableTexture,vec2(alt_texc,2.5/4.0),0).rgb;
#else
				vec3 combinedLightColour=lerp(sunlightColour1.rgb,sunlightColour2.rgb,layerTexCoords.z);
#endif
				float brightness_factor	=unshadowedBrightness(BetaClouds,lightResponse,amb);
				vec4 c					=calcColour2( density,BetaClouds,lightResponse,combinedLightColour,amb);
				fade_texc.x				=sqrt(fadeDistance);

				float sh				=saturate((fade_texc.x-nearFarTexc.x)/0.1);
#ifdef INFRARED
	c.rgb=1.0;//cloudIrRadiance*c.a;
#endif
				c.rgb					=applyFades2(c.rgb,fade_texc,BetaRayleigh,BetaMie,sh);
				colour.rgb				+=c.rgb*c.a*(colour.a);
				meanFadeDistance		+=fadeDistance*c.a*colour.a;
				//meanFadeDistance		=min(meanFadeDistance,fadeDistance);
				colour.a				*=(1.0-c.a);
				if(colour.a*brightness_factor<0.003)
				{
					colour.a	=0.0;
					//meanFadeDistance		=fadeDistance;//lerp(meanFadeDistance,fadeDistance,depthMix);
					break;
				}
			}
		}
	}
	if(colour.a>=1.0)
	   discard;
	meanFadeDistance+=colour.a;
	RaytracePixelOutput res;
    res.colour		=vec4(exposure*colour.rgb,colour.a);
	res.depth		=fadeDistanceToDepth(meanFadeDistance,clip_pos.xy,nearZ,farZ,tanHalfFov);
	return res;
}
#endif
#endif
