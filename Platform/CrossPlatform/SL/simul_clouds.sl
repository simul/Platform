#ifndef CLOUDS_SL
#define CLOUDS_SL

#ifndef GLSL
SamplerState cloudSamplerState	: register( s0);
#endif

#ifdef __PSSL__
	#ifdef USE_LIGHT_TABLES
		#define USE_LIGHT_TABLES1
	#else
		#define USE_LIGHT_TABLES0
	#endif
#else
	#if USE_LIGHT_TABLES==1
		#define USE_LIGHT_TABLES1
	#else
		#define USE_LIGHT_TABLES0
	#endif
#endif

#define MIN_SUN_ELEV (0.2)

struct RaytracePixelOutput
{
	vec4 colour SIMUL_TARGET_OUTPUT;
	float depth	SIMUL_DEPTH_OUTPUT;
};

struct FarNearPixelOutput
{
	vec4 farColour SIMUL_RENDERTARGET_OUTPUT(0);
	vec4 nearColour SIMUL_RENDERTARGET_OUTPUT(1);
	float depth	SIMUL_DEPTH_OUTPUT;
};


float MakeRainMap(Texture3D cloudDensity1,Texture3D cloudDensity2,float cloud_interp,vec2 texCoords)
{
	vec3 texc		=vec3(texCoords.xy,0.25);
	vec4 density1	=sampleLod(cloudDensity1,cloudSamplerState,texc,0);
	vec4 density2	=sampleLod(cloudDensity2,cloudSamplerState,texc,0);
	vec4 density	=lerp(density1,density2,cloud_interp);
	float r			=density.z;
	if(r<0.999)
		r=0;
	return r;
}

vec4 CloudShadow(Texture3D cloudDensity1,Texture3D cloudDensity2,vec2 texCoords,mat4 shadowMatrix,vec3 cornerPos,vec3 inverseScales)
{
//for this texture, let x be the square root of distance and y be the angle anticlockwise from the x-axis.
	vec2 pos_xy						=2.0*texCoords.xy-1.0;
	//float distance_off_centre		=length(pos_xy);
	vec2 illumination				=vec2(1.0,1.0);
	float U							=-1.0;
	#define NUM_STEPS 8
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

vec4 ShowCloudShadow(Texture2D cloudShadowTexture,Texture2D cloudGodraysTexture,vec2 texCoords)
{
	vec2 tex_pos		=2.0*texCoords.xy-vec2(1.0,1.0);
	float dist			=length(tex_pos.xy);
	vec2 radial_texc	=vec2(sqrt(length(tex_pos.xy)),atan2(tex_pos.y,tex_pos.x)/(2.0*3.1415926536));
    vec4 lookup			=texture_clamp_lod(cloudShadowTexture,texCoords.xy,0);
	//lookup				*=0.5;
	if(radial_texc.y<0)
		radial_texc.y	+=1.0;
	vec4 godrays_illum	=texture_wrap_clamp(cloudGodraysTexture,vec2(radial_texc.y,dist));
	//lookup.rgb			+=godrays_illum.xxx;
	return vec4(lookup.rgb,1.0);
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
//for this texture, let x be the square root of distance and y be the angle anticlockwise from the x-axis.
	float theta						=texCoords.x*2.0*3.1415926536;
	vec2 offset						=vec2(-sin(theta),cos(theta))*pixel/4.0;
	// First find the range where there is ANY shadow:
	for(int i=0;i<N;i++)
	{
		float interp					=float(i)/float(N-1);
		float distance_off_centre		=interp;
		vec2 shadow_texc				=0.5*(distance_off_centre*vec2(cos(theta),sin(theta))+1.0);
		vec4 illumination				=sampleLod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc,0);
		illumination					+=sampleLod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc-offset,0);
		illumination					+=sampleLod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc+offset,0);
		
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
		float distance_off_centre		=interp;
		vec2 shadow_texc				=0.5*(distance_off_centre*vec2(cos(theta),sin(theta))+1.0);
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

#ifndef GLSL
void GodraysAccumulation(RWTexture2D<float> targetTexture1,Texture2D cloudShadowTexture,int posx)
{
	uint2 dims;
	targetTexture1.GetDimensions(dims.x,dims.y);
//for this texture, let x be the square root of distance and y be the angle anticlockwise from the x-axis.
	float theta						=float(posx)/float(dims.x)*2.0*3.1415926536;
	//vec2 offset					=vec2(-sin(theta),cos(theta))*pixel/4.0;
	float total_ill					=0.0;
	// Find the total illumination
	for(int i=0;i<dims.y;i++)
	{
		float interp				=float(i)/float(dims.y-1);
		float distance_off_centre	=interp;
		vec2 shadow_texc			=0.5*(distance_off_centre*vec2(cos(theta),sin(theta))+1.0);
		vec4 illumination			=sampleLod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc,0);
	//	illumination				+=sampleLod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc-offset,0);
	//	illumination				+=sampleLod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc+offset,0);
		total_ill					+=illumination.x;
		targetTexture1[uint2(posx,i)]=total_ill/float(dims.y);
	}
}

float MoistureAccumulation(Texture2D cloudShadowTexture,int shadowTextureSize,vec2 texCoords)
{
	int N							=int(texCoords.y*float(shadowTextureSize));
	float pixel						=1.0/float(shadowTextureSize);
//for this texture, let x be the square root of distance and y be the angle anticlockwise from the x-axis.
	float theta						=texCoords.x*2.0*3.1415926536;
	vec2 offset						=vec2(-sin(theta),cos(theta))*pixel/4.0;
	float transparency				=1.0;
	// Find the total illumination
	for(int i=0;i<N;i++)
	{
		float interp				=float(i)/float(shadowTextureSize-1);
		float distance_off_centre	=interp;
		vec2 shadow_texc			=0.5*(distance_off_centre*vec2(cos(theta),sin(theta))+1.0);
		vec4 illumination			=sampleLod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc,0);
		illumination				+=sampleLod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc-offset,0);
		illumination				+=sampleLod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc+offset,0);
		transparency				*=exp(-saturate(1.0-illumination.x));
	}
	return 1.0-transparency;
}
#endif

vec3 applyFades2(Texture2D lossTexture,Texture2D inscTexture,Texture2D skylTexture,vec3 c,vec2 fade_texc,float BetaRayleigh,float BetaMie,float earthshadowMultiplier)
{
	vec3 loss		=sampleLod(lossTexture		,cmcSamplerState,fade_texc,0).rgb;
	vec3 skyl		=sampleLod(skylTexture		,cmcSamplerState,fade_texc,0).rgb;
	c			*=loss;
#ifdef INFRARED
	//c			=skyl.rgb;
#else
	vec4 insc		=sampleLod(inscTexture		,cmcSamplerState,fade_texc,0);
	vec3 inscatter	=earthshadowMultiplier*PrecalculatedInscatterFunction(insc,BetaRayleigh,BetaMie,mieRayleighRatio);
	c				+=skyl+inscatter;
#endif
    return c;
}


float unshadowedBrightness(float Beta,vec4 lightResponse,vec3 ambientColour)
{
	float final			=max(1.0,(Beta+lightResponse.y)+ambientColour.b);
	return final;
}

vec3 calcLightningColour(vec3 world_pos,vec3 lightningColour,vec3 lightningOrigin,vec3 lightningInvScales)
{
	vec3 texCoords=(world_pos-lightningOrigin)*lightningInvScales;
	float diff=length(texCoords-vec3(.5,.5,.5));
	float b=1.0/pow(diff+.0001,2.0);
	vec3 colour=b*lightningColour;
	return colour;
}

vec4 calcColour(Texture2D lossTexture,Texture2D inscTexture,Texture2D skylTexture,Texture2D lightTableTexture
				,vec4 density,float Beta,float BetaRayleigh,float BetaMie,vec4 lightResponse,vec3 ambientColour
				,vec3 world_pos,vec3 cloudTexCoords
				,vec2 fade_texc,vec2 nearFarTexc
				,out float brightnessFactor)
{
#ifdef USE_LIGHT_TABLES1
	float alt_texc				=world_pos.z/maxAltitudeMetres;
	vec3 combinedLightColour	=texture_clamp_lod(lightTableTexture,vec2(alt_texc,3.5/4.0),0).rgb;
	ambientColour				=lightResponse.w*texture_clamp_lod(lightTableTexture,vec2(alt_texc,2.5/4.0),0).rgb;
#else
	vec3 combinedLightColour	=lerp(sunlightColour1.rgb,sunlightColour2.rgb,saturate(cloudTexCoords.z));
#endif
	vec3 ambient				=density.w*ambientColour.rgb;
	float opacity				=density.z;
	vec4 c;
	c.rgb						=(density.y*Beta+lightResponse.y*density.x)*combinedLightColour+ambient.rgb;
	c.a							=opacity;
	brightnessFactor			=unshadowedBrightness(Beta,lightResponse,ambientColour);
	c.rgb						+=(1.0-density.x)*calcLightningColour(world_pos,lightningColour.rgb*lightningColour.w,lightningOrigin,lightningInvScales);
	float earthshadowMultiplier	=saturate((fade_texc.x-nearFarTexc.x)/0.1);
#ifdef INFRARED
	c.rgb						=lerp(cloudIrRadiance1,cloudIrRadiance2,saturate(cloudTexCoords.z));//*c.a;
#endif
	c.rgb						=applyFades2( lossTexture, inscTexture, skylTexture,c.rgb,fade_texc,BetaRayleigh,BetaMie,earthshadowMultiplier);
	return c;
}

vec4 MakeNoise(Texture3D noiseTexture3D,bool noise,float noise_centre_factor,vec3 noise_texc)
{
	vec4 noiseval				=vec4(0,0,0,0);
	if(noise)
	{
		float mult			=0.5;
		for(int j=0;j<1;j++)
		{
			noiseval		+=texture_wrap_lod(noiseTexture3D,noise_texc,0)*mult;
			noise_texc		*=noise3DOctaveScale;
			mult			*=noise3DPersistence;
		}
	}
	return noiseval;
}

vec4 calcDensity(Texture3D cloudDensity1,Texture3D cloudDensity2,vec3 texCoords,float layerFade,vec4 noiseval,vec3 fractalScale,float cloud_interp)
{
	float noise_factor	=lerp(baseNoiseFactor,1.0,saturate(texCoords.z));
	noiseval.rgb		*=noise_factor;
	vec3 pos			=texCoords.xyz+fractalScale.xyz*noiseval.xyz;
	vec4 density1		=sampleLod(cloudDensity1,cloudSamplerState,pos,0);
	vec4 density2		=sampleLod(cloudDensity2,cloudSamplerState,pos,0);
	vec4 density		=lerp(density1,density2,cloud_interp);
	density.z			*=layerFade*(1.0-noiseval.w);
	density.z			=saturate(density.z*(1.0+alphaSharpness)-alphaSharpness);
	return density;
}

#ifndef GLSL
RaytracePixelOutput RaytraceCloudsForward(Texture3D cloudDensity1
											,Texture3D cloudDensity2
											,Texture2D rainMapTexture
											,Texture2D noiseTexture
											,Texture3D noiseTexture3D
											,Texture2D lightTableTexture
											,Texture2D illuminationTexture
											,Texture2D rainbowLookupTexture
											,Texture2D coronaLookupTexture
											,Texture2D lossTexture
											,Texture2D inscTexture
											,Texture2D skylTexture
                                            ,bool do_depth_mix
											,vec4 dlookup
											,vec2 texCoords
											,bool near_pass
											,bool noise
											,bool noise_3d
											,bool do_rain_effect
											,vec3 cloudIrRadiance1,vec3 cloudIrRadiance2)
{
	RaytracePixelOutput res;
	res.colour				=vec4(0,0,0,1.0);
	res.depth				=0.0;
	vec4 clip_pos			=vec4(-1.0,1.0,1.0,1.0);
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

	float min_z				=cornerPos.z-(fractalScale.z*1.5)/inverseScales.z;
	float max_z				=cornerPos.z+(1.0+fractalScale.z*1.5)/inverseScales.z;
	if(do_rain_effect)
		min_z				=-1000.0;
	else if(view.z<-0.1&&viewPos.z<cornerPos.z-fractalScale.z/inverseScales.z)
		return res;
	float depth;
	if(near_pass)
	{
		if(dlookup.z==0)
		{
			res.colour		=vec4(0,0,0,1.0);
			res.depth		=0.0;
			return res;
		}
		depth				=dlookup.y;
	}
	else
	{
		depth				=dlookup.x;
	}
	float solid_dist		=depthToFadeDistance(depth,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
	vec4 colour				=vec4(0.0,0.0,0.0,1.0);
	vec2 fade_texc			=vec2(0.0,0.5*(1.0-sine));
	// Lookup in the illumination texture.
	vec2 illum_texc			=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec4 illum_lookup		=texture_wrap_mirror(illuminationTexture,illum_texc);
	vec2 nearFarTexc		=illum_lookup.xy;
	float meanFadeDistance	=0.0;
	// Precalculate hg effects
	float BetaClouds		=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	float BetaRayleigh		=CalcRayleighBeta(cos0);
	float BetaMie			=HenyeyGreenstein(hazeEccentricity,cos0);
#ifdef USE_LIGHT_TABLES0	
	vec3 amb				=ambientColour.rgb;
#endif
	vec4 rainbowColour		=RainbowAndCorona(rainbowLookupTexture,coronaLookupTexture,dropletRadius,
												rainbowIntensity,view,lightDir,texCoords.xy);
	float moisture				=0.0;
	float noise_centre_factor	=1.0;//exp(-length(clip_pos.xy));

	vec3 world_pos					=viewPos;

	// This provides the range of texcoords that is lit.
	int3 c_offset					=int3(sign(view.x),sign(view.y),sign(view.z));
	int3 start_c_offset				=-c_offset;
	start_c_offset					=int3(max(start_c_offset.x,0),max(start_c_offset.y,0),max(start_c_offset.z,0));
	vec3 viewScaled					=view/scaleOfGridCoords;
	viewScaled						=normalize(viewScaled);

	vec3 offset_vec	=vec3(0,0,0);
/*	if(world_pos.z<min_z)
	{
		float a		=1.0/(view.z+0.00001);
		offset_vec	=(min_z-world_pos.z)*vec3(view.x*a,view.y*a,1.0);
	}
	if(view.z<0&&world_pos.z>max_z)
	{
		float a		=1.0/(-view.z+0.00001);
		offset_vec	=(world_pos.z-max_z)*vec3(view.x*a,view.y*a,-1.0);
	}*/
	world_pos						+=offset_vec;
	vec3 gridOriginPos				=cornerPos+0.5/inverseScales.z;
	float viewScale					=length(viewScaled*scaleOfGridCoords);
	vec3 startOffsetFromOrigin		=viewPos-gridOriginPos;
	vec3 offsetFromOrigin			=startOffsetFromOrigin;
	vec3 p0							=offsetFromOrigin/scaleOfGridCoords;
	int3 c0							=floor(p0) + start_c_offset;
	vec3 gridScale					=scaleOfGridCoords;
	vec3 P0							=offsetFromOrigin/scaleOfGridCoords/2.0;
	int3 C0							=c0/2;
	
	float distanceMetres			=distance(world_pos,viewPos);
	int3 c							=c0;

	vec3 colours[]					={{1.0,0.5,0.5},{0.0,1.0,0.0},{1.0,0.0,0.7},{0.0,1.0,1.0},{0.5,0.5,0.0}};
	int idx=0;
	float W							=halfClipSize;
	const float start				=0.866;//0.707 for 2D, 0.866 for 3D;
	const float ends				=1.0;
	const float range				=ends-start;
	// origin of the grid - at all levels of detail, there will be a slice through this in 3 axes.
	for(int i=0;i<layerCount;i++)
	{
		vec4 density				=vec4(0,0,0,0);
		world_pos					+=view;
		offsetFromOrigin			=world_pos-gridOriginPos;

		if((view.z<0&&world_pos.z<min_z)||(view.z>0&&world_pos.z>max_z)||distanceMetres>maxCloudDistanceMetres)
			break;
		// Next pos.
		int3 c1						=c+c_offset;

		//find the correct d:
		vec3 p						=offsetFromOrigin / gridScale;
		vec3 p1						=c1;
		vec3 dp						=p1-p;
		vec3 D						=dp/viewScaled;

		float e						=D.x;
		vec3 N						=vec3(1.0,0,0);
	/*	if(D.y<e)
		{
			e						=D.y;
			N						=vec3(0,1.0,0);
		}*/
		if(D.z<e)
		{
			e						=D.z;
			N						=vec3(0,0,1.0);
		}
		
		int3 c_step					=c_offset*int3(N);
		float d						=e*viewScale;
		distanceMetres				+=d;

		// What offset was the original position from the centre of the cube?
		p1							=p+e*viewScaled;
		vec3 d0						=normalize(2.0*abs(frac(p1)-vec3(.5,.5,.5)));
		float fade					=abs(dot(N,viewScaled));
	
	//	fade					*=1.0-exp(-e);
		// We fade out the intermediate steps as we approach the boundary of a detail change.
		//float verticalShift			=0;
		// Now sample at the halfway point:
		world_pos					+=d*view;
		vec3 cloudWorldOffset		=world_pos-cornerPos;
		vec3 cloudTexCoords			=(cloudWorldOffset)*inverseScales;
		c							+=c_step;
		int3 intermediate			=abs(int3(c.x%2,c.y%2,c.z%2));
		float is_inter				=dot(N,vec3(intermediate));
		// A spherical shell, whose outer radius is W, and, wholly containing the inner box, the inner radius must be sqrt(3 (W/2)^2).
		// i.e. from 0.5*(3)^0.5 to 1, from sqrt(3/16) to 0.5, from 0.433 to 0.5
		vec3 pw						=abs(p1-p0);//+start_c_offset
		float fade_inter			=saturate((length(pw.xyz)/float(W-1.0)-start)/range);// /(2.0-is_inter)
	//	if(idx==0)
			fade					*=1.0-(is_inter*fade_inter);
		fade						*=saturate(distanceMetres/40.0);
		float fadeDistance			=saturate(distanceMetres/maxFadeDistanceMetres);
	//	fade*=1-idx;
		int3 b						=abs(c-C0*2);//+start_c_offset
	//	if(idx==0)
		bool transition=(max(max(b.x,b.y),b.z)>=W);
		if(fade>0&&(fadeDistance<=solid_dist||!do_depth_mix)&&world_pos.z<=max_z)
		{
			vec3 noise_texc			=cloudTexCoords.xyz*noise3DTexcoordScale+noise3DTexcoordOffset;
			vec4 noiseval			=MakeNoise(noiseTexture3D,noise,noise_centre_factor,noise_texc);
		
			density					=calcDensity(cloudDensity1,cloudDensity2,cloudTexCoords,fade,noiseval,fractalScale,cloud_interp);
		//	density.z				=fade*(cloudTexCoords.z>=0&&cloudTexCoords.z<1.0);
			// The rain fall angle is used:
			vec3 rain_texc			=cloudWorldOffset;
			rain_texc.xy			+=rain_texc.z*rainTangent;
			float rain_lookup		=texture_wrap_lod(rainMapTexture,rain_texc.xy*inverseScales.xy,0).x;
			vec4 streak				=texture_wrap_lod(noiseTexture,0.00003*rain_texc.xy,0);
			float dm=0.0;
			if(do_rain_effect)
			{
				dm					=fade*rainEffect*rain_lookup
										*saturate((rainRadius-length(world_pos.xy-rainCentre.xy))*0.0003)
										*saturate(1.0-10*cloudTexCoords.z)
										*saturate(cloudTexCoords.z+1.0+4.0*streak.y)
										*(0.4+0.6*streak.x)
										;
				moisture			+=0.01*dm*density.x;
				density.z			=saturate(density.z+dm);
			}
			if(do_depth_mix)
				density.z				*=saturate((solid_dist-fadeDistance)/0.01);
			if(density.z>0)
			{
				float brightness_factor;
				fade_texc.x				=sqrt(fadeDistance);
				vec4 clr				=calcColour(lossTexture,inscTexture,skylTexture,lightTableTexture
													,density
													,BetaClouds,BetaRayleigh,BetaMie
													,lightResponse,ambientColour
													,world_pos,cloudTexCoords
													,fade_texc,nearFarTexc
													,brightness_factor);
#if 1//def DEBUG_SAMPLING
				if(texCoords.y>.9)
				{
					clr.a=.5;
					clr.rgb=colours[idx%5];
					if(texCoords.y>.95)
						clr.rgb=fade_inter;//is_inter;
					if(texCoords.y>.975)
						clr.rgb=is_inter;
				}
				if(cloudTexCoords.z>12.2)
					clr.a=0.0;
#endif
				//if(transition)
				//	clr.r=0;
				colour.rgb				+=clr.rgb*clr.a*(colour.a);
				meanFadeDistance		+=fadeDistance*clr.a*colour.a;
				colour.a				*=(1.0-clr.a);
				if(colour.a*brightness_factor<0.003)
				{
					colour.a			=0.0;
					break;
				}
			}
		}
	//	if(b.x>=halfClipSize)
		if(transition)
		{
			c0			=	C0;
			c			=	(c+start_c_offset)/2;
			gridScale	*=	2.0;
			viewScale	*=	2.0;
			if(!idx)
				W*=2;
			p0			=	P0;
			P0			=	startOffsetFromOrigin/gridScale/2.0;
			C0			=	(c0+start_c_offset)/2;
			idx			++;
		}
	}
	meanFadeDistance	+=colour.a;
    res.colour			=vec4(exposure*colour.rgb,colour.a);
	res.depth			=fadeDistanceToDepth(meanFadeDistance,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
#ifndef INFRARED
	res.colour.rgb		+=saturate(moisture)*sunlightColour1.rgb/25.0*rainbowColour.rgb;
#endif
	return res;
}
#endif
#endif
