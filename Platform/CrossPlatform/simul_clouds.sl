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
SamplerState cloudSamplerState			: register(s0);
#endif

struct RaytracePixelOutput
{
	vec4 colour SIMUL_TARGET_OUTPUT;
	float depth	SIMUL_DEPTH_OUTPUT;
};

vec4 calcDensity(vec3 pos,float layerFade,float cloud_interp)
{
	vec4 density1=sampleLod(cloudDensity1,cloudSamplerState,pos,0);
	vec4 density2=sampleLod(cloudDensity2,cloudSamplerState,pos,0);
	vec4 density=lerp(density1,density2,cloud_interp);
	density.z*=layerFade;
	//density.z=saturate(density.z*(1.f+alphaSharpness)-alphaSharpness);
	return density;
}

vec4 calcDensity(vec3 texCoords,float layerFade,vec3 noiseval,vec3 fractalScale,float cloud_interp)
{
	vec3 pos=texCoords.xyz+fractalScale.xyz*noiseval;
	vec4 density1=sampleLod(cloudDensity1,cloudSamplerState,pos,0);
	vec4 density2=sampleLod(cloudDensity2,cloudSamplerState,pos,0);
	vec4 density=lerp(density1,density2,cloud_interp);
	density.z*=layerFade;
	//density.z=saturate(density.z*(1.f+alphaSharpness)-alphaSharpness);
	return density;
}

vec4 calcColour(vec4 density,float cos0,float texz,vec4 lightResponse,vec3 ambientColour)
{
	float Beta=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	vec3 ambient=density.w*ambientColour.rgb;
	float opacity=density.z;
	vec3 sunlightColour=lerp(sunlightColour1,sunlightColour2,saturate(texz));
	vec4 final;
	final.rgb=(density.y*Beta+lightResponse.y*density.x)*sunlightColour+ambient.rgb;
	final.a=opacity;
	return final;
}

vec4 calcUnfadedColour(vec3 texCoords,float layerFade,vec3 noiseval,vec3 fractalScale,float cloud_interp,vec4 lightResponse,vec3 ambientColour,float cos0)
{
	vec4 density=calcDensity(texCoords,layerFade,noiseval,fractalScale,cloud_interp);
	if(density.z<=0)
		discard;
	vec4 final=calcColour(density,cos0,texCoords.z,lightResponse,ambientColour);
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
	float theta						=texCoords.y*2.0*3.1415926536;
#ifdef RADIAL_CLOUD_SHADOW
	float distance_off_centre		=pow(texCoords.x,2.0);
	vec2 pos_xy						=distance_off_centre*vec2(cos(theta),sin(theta));
#else
	vec2 pos_xy						=2.0*texCoords.xy-1.0;
	float distance_off_centre		=length(pos_xy);
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
		vec4 density1				=sampleLod(cloudDensity1,wwcSamplerState,texc,0);
		vec4 density2				=sampleLod(cloudDensity2,wwcSamplerState,texc,0);
		vec4 density				=lerp(density1,density2,cloud_interp);
		if(density.z>0)
		{
			illumination			=lerp(illumination,vec2(0,0),density1.z);//density.xy
			U						=lerp(U,u,density.z);
		}
	}
	vec3 simple_texc				=vec3(texCoords,0);
	float shadow					=lerp(sampleLod(cloudDensity1,wwcSamplerState,simple_texc,0).y,sampleLod(cloudDensity2,wwcSamplerState,simple_texc,0).y,cloud_interp);
	return vec4(illumination,U,shadow);//*edge
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
	int in_light=0;
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

vec4 ShowCloudShadow(Texture2D cloudShadowTexture,Texture2D nearFarTexture,vec2 texCoords)
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
    vec4 nearFarShadowLight=texture_wrap_lod(nearFarTexture,vec2(radial_texc.y,0.5),0);
	if(dist>=nearFarShadowLight.x&&dist<=nearFarShadowLight.y)
	{
		lookup*=0.5;
	}
	if(dist>=nearFarShadowLight.z&&dist<=nearFarShadowLight.w)
		lookup.g+=0.5;
	//if(abs(radial_texc.y)<0.003)
	//lookup.r+=abs(radial_texc.y);
	//lookup.gb+=lookup.a;
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

vec4 calcColour2(vec4 density,float Beta,float texz,vec4 lightResponse,vec3 ambientColour)
{
	vec3 ambient=density.w*ambientColour.rgb;
	float opacity=density.z;
	vec3 sunlightColour=lerp(sunlightColour1,sunlightColour2,saturate(texz));
	vec4 final;
	final.rgb=(density.y*Beta+lightResponse.y*density.x)*sunlightColour+ambient.rgb;
	final.a=opacity;
	return final;
}


float unshadowedBrightness(float Beta,float texz,vec4 lightResponse)
{
	vec3 ambient		=ambientColour.rgb;
	float final			=max(1.0,(Beta+lightResponse.y)+ambient.b);
	return final;
}

#ifndef GLSL
vec4 SimpleRaytraceCloudsForward(Texture3D cloudDensity1,Texture3D cloudDensity2,Texture2D depthTexture,vec2 texCoords)
{
	float dlookup 		=sampleLod(depthTexture,clampSamplerState,viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias),0).r;
	vec4 clip_pos		=vec4(-1.f,1.f,1.f,1.f);
	clip_pos.x			+=2.0*texCoords.x;
	clip_pos.y			-=2.0*texCoords.y;
	vec3 view			=normalize(mul(invViewProj,clip_pos).xyz);
	float cos0			=dot(lightDir.xyz,view.xyz);
	float sine			=view.z;
	vec3 n				=vec3(clip_pos.xy*tanHalfFov,1.0);
	n					=normalize(n);
	vec2 noise_texc_0	=mul(noiseMatrix,vec4(n.xy,0,0)).xy;

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

	float mean_z		=0.0;
	float up			=step(0.1,sine);
	float down			=step(0.1,-sine);
	// Precalculate hg effects
	float BetaClouds	=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	float BetaRayleigh	=0.0596831*(1.0+cos0*cos0);
	float BetaMie		=HenyeyGreenstein(hazeEccentricity,cos0);
	// This provides the range of texcoords that is lit.
	for(int i=0;i<layerCount;i++)
	{
		vec4 density			=vec4(0,0,0,0);
		const LayerData layer	=layers[i];
		float layerWorldDist	=layer.layerDistance;
		float normLayerZ		=saturate(layerWorldDist/maxFadeDistanceMetres);
		vec3 world_pos			=viewPos+layerWorldDist*view;
		world_pos.z				-=layer.verticalShift;
		vec3 layerTexCoords		=(world_pos-cornerPos)*inverseScales;
		if(normLayerZ<=d&&layerTexCoords.z>=min_texc_z&&layerTexCoords.z<=max_texc_z)
		{
			density					=calcDensity(layerTexCoords,layer.layerFade,cloud_interp);
			density.z				*=saturate((d-normLayerZ)/0.001);
		}
		// TODO: faster inside above brace. But: PS4 problems?
		if(density.z>0)
		{
			float brightness_factor	=unshadowedBrightness(BetaClouds,layerTexCoords.z,lightResponse);
			vec4 c					=calcColour2(density,BetaClouds,layerTexCoords.z,lightResponse,ambientColour);
			fade_texc.x				=sqrt(mean_z);
			float sh				=saturate((fade_texc.x-nearFarTexc.x)/0.1);
			c.rgb					*=sh;
			colour.rgb				+=c.rgb*c.a*(colour.a);
			mean_z					+=normLayerZ*c.a*colour.a;
			colour.a				*=(1.0-c.a);
		}
	}
	if(colour.a>=1.0)
	   discard;
//	mean_z+=colour.a;
	fade_texc.x				=sqrt(mean_z);
	float sh				=saturate((fade_texc.x-nearFarTexc.x)/0.1);
	colour.rgb				=exposure*applyFades2(colour.rgb,fade_texc,BetaRayleigh,BetaMie,sh);
	return colour;
}

RaytracePixelOutput ExperimentalRaytraceCloudsForward3DNoise(Texture3D cloudDensity1
														,Texture3D cloudDensity2
														,Texture3D noiseTexture3D
														,Texture2D depthTexture
														,vec2 texCoords)
{
	float dlookup 		=sampleLod(depthTexture,clampSamplerState,viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias),0).r;
	vec4 clip_pos		=vec4(-1.f,1.f,1.f,1.f);
	clip_pos.x			+=2.0*texCoords.x;
	clip_pos.y			-=2.0*texCoords.y;
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

	float mean_z		=0.0;
	float up			=step(0.1,sine);
	float down			=step(0.1,-sine);
	// Precalculate hg effects
	float BetaClouds	=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	float BetaRayleigh	=0.0596831*(1.0+cos0*cos0);
	float BetaMie		=HenyeyGreenstein(hazeEccentricity,cos0);

	float dh1			=cornerPos.z-viewPos.z;
	float dh2			=cornerPos.z+1.0/inverseScales.z-viewPos.z;

	float D1			=0.0;
	float D2			=1.0;
	// If we're not IN the clouds:
	if(dh1*dh2>0)
	{
		// Looking in the wrong direction?
		if(dh1*sine<0)
			discard;
		if(sine>0)
		{
			D1=dh1/sine;
			D2=dh2/sine;
		}
		else
		{
			D1=abs(dh2/sine);
			D2=abs(dh1/sine);
		}
	}
	else
	{
		// looking up or down?
		if(sine>0)
			D2=dh2/sine;
		else
			D2=dh1/sine;
	}
	float D=D2-D1;
	// For a given layerCount, let N=layerCount.
	// We require N layers in each direction, with thickness D=D2-D1.
	// density: p=N/D layers per metre.
	float p=float(100)/D;

	float stepLength=maxFadeDistanceMetres/float(100);
	int i0=max(int(D1/stepLength)-1,0);
	int i1=int(D2/stepLength)+1;
	for(int i=0;i<100;i++)
	{
		if(i>=i0&&i<=i1)
		{
			vec4 density			=vec4(0,0,0,0);
			float layerFade			=1.0;
			//const LayerData layer	=layers[i];
			float layerWorldDist	=i*stepLength;//layer.layerDistance;
			float normLayerZ		=saturate(layerWorldDist/maxFadeDistanceMetres);
			vec3 world_pos			=viewPos+layerWorldDist*view;
		//	world_pos.z				-=layer.verticalShift;
			vec3 layerTexCoords		=(world_pos-cornerPos)*inverseScales;
			if(normLayerZ<=d&&layerTexCoords.z>=min_texc_z&&layerTexCoords.z<=max_texc_z)
			{
				float noise_factor		=lerp(baseNoiseFactor,1.0,saturate(layerTexCoords.z));
				vec3 noise_texc			=layerTexCoords.xyz*noise3DTexcoordScale;
				vec3 noiseval			=vec3(0.0,0.0,0.0);
				float mul=0.5;
				for(int j=0;j<2;j++)
				{
					noiseval			+=(texture_wrap_lod(noiseTexture3D,noise_texc,0).xyz)*mul;
					noise_texc			*=2.0;
					mul					*=noise3DPersistence;
				}
				noiseval				*=noise_factor;
				density					=calcDensity(layerTexCoords,layerFade,noiseval,fractalScale,cloud_interp);
		// density=sampleLod(cloudDensity1,cloudSamplerState,layerTexCoords,0);
				//density.z=saturate(density.z*100.0);
				density.z				*=saturate((d-normLayerZ)/0.001);
			}
			if(density.z>0)
			{
				float brightness_factor	=unshadowedBrightness(BetaClouds,layerTexCoords.z,lightResponse);
				vec4 c					=calcColour2( density,BetaClouds,layerTexCoords.z,lightResponse,ambientColour);
			
				fade_texc.x				=sqrt(normLayerZ);
				float sh				=saturate((fade_texc.x-nearFarTexc.x)/0.1);
				c.rgb					*=sh;
				c.rgb					=applyFades2(c.rgb,fade_texc,BetaRayleigh,BetaMie,sh);
				colour.rgb				+=c.rgb*c.a*(colour.a);
				
				// depth here:
				mean_z					+=normLayerZ*c.a*colour.a;
				colour.a				*=(1.0-c.a);
				if(colour.a*brightness_factor<0.003)
				{
					colour.a	=0.0;
					//mean_z		=normLayerZ;//lerp(mean_z,normLayerZ,depthMix);
					break;
				}
			}
		}
	}
	if(colour.a>=1.0)
	   discard;
	mean_z+=colour.a;
	RaytracePixelOutput res;
    res.colour		=vec4(exposure*colour.rgb,colour.a);
	res.depth		=fadeDistanceToDepth(mean_z,clip_pos.xy,nearZ,farZ,tanHalfFov);
	return res;
}


RaytracePixelOutput RaytraceCloudsForward3DNoise(Texture3D cloudDensity1
												,Texture3D cloudDensity2
												,Texture3D noiseTexture3D
												,Texture2D depthTexture
												,vec2 texCoords)
{
	float dlookup 		=sampleLod(depthTexture,clampSamplerState,viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias),0).r;
	vec4 clip_pos		=vec4(-1.f,1.f,1.f,1.f);
	clip_pos.x			+=2.0*texCoords.x;
	clip_pos.y			-=2.0*texCoords.y;
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

	float mean_z		=0.0;
	float up			=step(0.1,sine);
	float down			=step(0.1,-sine);
	// Precalculate hg effects
	float BetaClouds	=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	float BetaRayleigh	=0.0596831*(1.0+cos0*cos0);
	float BetaMie		=HenyeyGreenstein(hazeEccentricity,cos0);
	// This provides the range of texcoords that is lit.
	for(int i=0;i<layerCount;i++)
	{
		vec4 density			=vec4(0,0,0,0);
		const LayerData layer	=layers[i];
		float layerWorldDist	=layer.layerDistance;
		float normLayerZ		=saturate(layerWorldDist/maxFadeDistanceMetres);
		vec3 world_pos			=viewPos+layerWorldDist*view;
		world_pos.z				-=layer.verticalShift;
		vec3 layerTexCoords		=(world_pos-cornerPos)*inverseScales;
		float layerFade			=layer.layerFade;//*saturate((abs(sine)-layer.sine_threshold)/layer.sine_range);
		if(layerFade>0&&normLayerZ<=d&&layerTexCoords.z>=min_texc_z&&layerTexCoords.z<=max_texc_z)
		{
			float noise_factor		=lerp(baseNoiseFactor,1.0,saturate(layerTexCoords.z));
			vec3 noiseval			=vec3(0.0,0.0,0.0);
			vec3 noise_texc			=layerTexCoords.xyz*noise3DTexcoordScale;
			float mul=0.5;
			for(int j=0;j<4;j++)
			{
				noiseval			+=(texture_wrap_lod(noiseTexture3D,noise_texc,0).xyz)*mul;
				noise_texc			*=2.0;
				mul					*=noise3DPersistence;
			}
			noiseval				*=noise_factor;
			density					=calcDensity(layerTexCoords,layerFade,noiseval,fractalScale,cloud_interp);
			density.z				*=saturate((d-normLayerZ)/0.001);
		}
		if(density.z>0)
		{
			float brightness_factor	=unshadowedBrightness(BetaClouds,layerTexCoords.z,lightResponse);
			vec4 c					=calcColour2( density,BetaClouds,layerTexCoords.z,lightResponse,ambientColour);
			
			fade_texc.x				=sqrt(normLayerZ);
			float sh				=saturate((fade_texc.x-nearFarTexc.x)/0.1);
			c.rgb					*=sh;
			c.rgb					=applyFades2(c.rgb,fade_texc,BetaRayleigh,BetaMie,sh);
			colour.rgb				+=c.rgb*c.a*(colour.a);
			// depth here:
			mean_z					+=normLayerZ*c.a*colour.a;
			colour.a				*=(1.0-c.a);
			if(colour.a*brightness_factor<0.003)
			{
				colour.a	=0.0;
				//mean_z		=normLayerZ;//lerp(mean_z,normLayerZ,depthMix);
				break;
			}
		}
	}
	if(colour.a>=1.0)
	   discard;
	mean_z+=colour.a;
	RaytracePixelOutput res;
    res.colour		=vec4(exposure*colour.rgb,colour.a);
	res.depth		=fadeDistanceToDepth(mean_z,clip_pos.xy,nearZ,farZ,tanHalfFov);
	return res;
}

RaytracePixelOutput RaytraceCloudsForward(Texture3D cloudDensity1
											,Texture3D cloudDensity2
											,Texture2D noiseTexture
											,Texture2D depthTexture
											,vec2 texCoords)
{
	float dlookup 		=sampleLod(depthTexture,clampSamplerState,viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias),0).r;
	vec4 clip_pos		=vec4(-1.f,1.f,1.f,1.f);
	clip_pos.x			+=2.0*texCoords.x;
	clip_pos.y			-=2.0*texCoords.y;
	vec3 view			=normalize(mul(invViewProj,clip_pos).xyz);
	float cos0			=dot(lightDir.xyz,view.xyz);
	float sine			=view.z;
	vec3 n				=vec3(clip_pos.xy*tanHalfFov,1.0);
	n					=normalize(n);
	vec2 noise_texc_0	=mul(noiseMatrix,vec4(n.xy,0,0)).xy/fractalRepeatLength;

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

	float mean_z		=0.0;
	float up			=step(0.1,sine);
	float down			=step(0.1,-sine);
	// Precalculate hg effects
	float BetaClouds	=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	float BetaRayleigh	=0.0596831*(1.0+cos0*cos0);
	float BetaMie		=HenyeyGreenstein(hazeEccentricity,cos0);
	// This provides the range of texcoords that is lit.
	for(int i=0;i<layerCount;i++)
	{
		vec4 density			=vec4(0,0,0,0);
		const LayerData layer	=layers[i];
		float layerWorldDist	=layer.layerDistance;
		float normLayerZ		=saturate(layerWorldDist/maxFadeDistanceMetres);
		vec3 world_pos			=viewPos+layerWorldDist*view;
		world_pos.z				-=layer.verticalShift;
		vec3 layerTexCoords		=(world_pos-cornerPos)*inverseScales;
		float layerFade			=layer.layerFade;//*saturate((abs(sine)-layer.sine_threshold)/layer.sine_range);
		if(layerFade>0&&normLayerZ<=d&&layerTexCoords.z>=min_texc_z&&layerTexCoords.z<=max_texc_z)
		{
			float noise_factor		=lerp(baseNoiseFactor,1.0,saturate(layerTexCoords.z));
			vec2 noise_texc			=noise_texc_0*layerWorldDist+layer.noiseOffset;
			vec3 noiseval			=noise_factor*texture_wrap_lod(noiseTexture,noise_texc,0).xyz;
			density					=calcDensity(layerTexCoords,layer.layerFade,noiseval,fractalScale,cloud_interp);
			density.z				*=saturate((d-normLayerZ)/0.001);
		}
		// TODO: faster inside above brace. But: PS4 problems?
		if(density.z>0)
		{
			float brightness_factor	=unshadowedBrightness(BetaClouds,layerTexCoords.z,lightResponse);
			vec4 c					=calcColour2( density,BetaClouds,layerTexCoords.z,lightResponse,ambientColour);
			fade_texc.x				=sqrt(normLayerZ);
			float sh				=saturate((fade_texc.x-nearFarTexc.x)/0.1);
			c.rgb					*=sh;
			c.rgb					=applyFades2(c.rgb,fade_texc,BetaRayleigh,BetaMie,sh);
			colour.rgb				+=c.rgb*c.a*(colour.a);
			mean_z					+=normLayerZ*c.a*colour.a;
			colour.a				*=(1.0-c.a);
			if(colour.a*brightness_factor<0.003)
			{
				colour.a	=0.0;
				//mean_z		=normLayerZ;//lerp(mean_z,normLayerZ,depthMix);
				break;
			}
		}
	}
	if(colour.a>=1.0)
	   discard;
	mean_z+=colour.a;
	RaytracePixelOutput res;
    res.colour		=vec4(exposure*colour.rgb,colour.a);
	res.depth		=fadeDistanceToDepth(mean_z,clip_pos.xy,nearZ,farZ,tanHalfFov);
	return res;
}
#endif
#endif
