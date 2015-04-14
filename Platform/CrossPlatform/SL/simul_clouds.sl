#ifndef CLOUDS_SL
#define CLOUDS_SL

#ifndef GLSL
SamplerState cloudSamplerState: register( s0);
#endif

#define MIN_SUN_ELEV (0.2)

struct RaytracePixelOutput
{
	vec4 colour			;
	vec4 nearColour		;
	vec4 nearFarDepth	;
};

struct FarNearPixelOutput
{
	vec4 farColour		SIMUL_RENDERTARGET_OUTPUT(0);
	vec4 nearColour		SIMUL_RENDERTARGET_OUTPUT(1);
};

struct All8DepthOutput
{
	vec4 colour1 SIMUL_RENDERTARGET_OUTPUT(0);
	vec4 colour2 SIMUL_RENDERTARGET_OUTPUT(1);
	vec4 colour3 SIMUL_RENDERTARGET_OUTPUT(2);
	vec4 colour4 SIMUL_RENDERTARGET_OUTPUT(3);
	vec4 colour5 SIMUL_RENDERTARGET_OUTPUT(4);
	vec4 colour6 SIMUL_RENDERTARGET_OUTPUT(5);
	vec4 colour7 SIMUL_RENDERTARGET_OUTPUT(6);
	vec4 colour8 SIMUL_RENDERTARGET_OUTPUT(7);
	float depth	SIMUL_DEPTH_OUTPUT;
};

float MakeRainMap(Texture3D cloudDensity,float cloud_interp,vec2 texCoords)
{
	vec3 texc		=vec3(texCoords.xy,0.25);
	vec4 density	=sample_3d_lod(cloudDensity,cloudSamplerState,texc,0);
	float r			=density.z;
	if(r<0.999)
		r=0;
	return r;
}

vec4 CloudShadow(Texture3D cloudDensity,vec2 texCoords,mat4 shadowMatrix,vec3 cornerPosKm,vec3 inverseScalesKm)
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
	vec3 texc_1						=(wpos_1-cornerPosKm)*inverseScalesKm;
	vec3 texc_2						=(wpos_2-cornerPosKm)*inverseScalesKm;
	for(int i=0;i<NUM_STEPS;i++)
	{
		//float u					=1.0-float(i)/float(NUM_STEPS);
		float u						=float(i)/float(NUM_STEPS);
		//vec3 cartesian			=vec3(pos_xy.xy,u);
		//vec3 wpos					=mul(shadowMatrix,vec4(cartesian,1.0)).xyz;
		vec3 texc					=lerp(texc_1,texc_2,u);//(wpos-cornerPosKm)*inverseScalesKm;
	
		vec4 density				=sample_3d_lod(cloudDensity,cloudSamplerState,texc,0);
		if(density.z>0)
		{
			illumination			=lerp(illumination,vec2(0,0),density.z);//density.xy
			U						=lerp(U,u,density.z);
		}
	}
	vec3 simple_texc				=vec3(texCoords,0);
	vec2 shadow						=sample_3d_lod(cloudDensity,wwcSamplerState,simple_texc,0).xy;
	return vec4(illumination,U,0.5*(shadow.x+shadow.y));//*edge
}

vec4 ShowCloudShadow(Texture2D cloudShadowTexture,Texture2D cloudGodraysTexture,vec2 texCoords)
{
//	vec2 tex_pos		=2.0*texCoords.xy-vec2(1.0,1.0);
//	float dist			=length(tex_pos.xy);
//	vec2 radial_texc	=vec2(sqrt(length(tex_pos.xy)),atan2(tex_pos.y,tex_pos.x)/(2.0*3.1415926536));
    vec4 lookup			=texture_clamp_lod(cloudShadowTexture,texCoords.xy,0);
/*
	if(radial_texc.y<0)
		radial_texc.y	+=1.0;
	vec4 godrays_illum	=texture_wrap_clamp(cloudGodraysTexture,vec2(radial_texc.y,dist));
	*/
	return vec4(lookup.rgb*lookup.a,1.0);
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
		vec4 illumination				=texture_wrap_lod(cloudShadowTexture,shadow_texc,0);
		illumination					+=texture_wrap_lod(cloudShadowTexture,shadow_texc-offset,0);
		illumination					+=texture_wrap_lod(cloudShadowTexture,shadow_texc+offset,0);
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
		vec4 illumination			=sample_lod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc,0);
		illumination				+=sample_lod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc-offset,0);
		illumination				+=sample_lod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc+offset,0);
		transparency				*=exp(-saturate(1.0-illumination.x));
	}
	return 1.0-transparency;
}


float unshadowedBrightness(float Beta,vec4 lightResponse,vec3 ambientColour)
{
	float final			=max(1.0,(Beta+lightResponse.y)+ambientColour.b);
	return final;
}


vec3 applyFades2(Texture2D lossTexture,Texture3D inscatterVolumeTexture,vec3 volumeTexCoords,vec3 c,vec2 fade_texc,float earthshadowMultiplier)
{
	vec3 loss		=sampleLod(lossTexture		,cmcSamplerState,fade_texc,0).rgb;
	c			*=loss;
#ifdef INFRARED
	//c			=skyl.rgb;
#else
	vec3 inscatter	=texture_3d_wmc_lod(inscatterVolumeTexture,volumeTexCoords,0).rgb;
	c				+=inscatter;
#endif
    return c;
}

vec4 calcColour(Texture2D lossTexture,Texture3D inscatterVolumeTexture,vec3 volumeTexCoords,Texture2D lightTableTexture
				,vec4 density,float Beta,vec4 lightResponse,vec3 ambientColour
				,vec3 world_pos,vec3 cloudTexCoords
				,vec2 fade_texc,vec2 nearFarTexc
				,out float brightnessFactor)
{
	float sun_alt_texc			=((world_pos.z-minSunlightAltitudeKm)/fadeAltitudeRangeKm);
	vec3 combinedLightColour	=texture_clamp_lod(lightTableTexture,vec2(sun_alt_texc,3.5/4.0),0).rgb;
	float alt_texc				=(world_pos.z/fadeAltitudeRangeKm);
	ambientColour				=lightResponse.w*texture_clamp_lod(lightTableTexture,vec2(alt_texc,2.5/4.0),0).rgb;
	vec3 ambient				=density.w*ambientColour.rgb;
	vec4 c;
	c.rgb						=(density.y*Beta+lightResponse.y*density.x)*combinedLightColour+ambient.rgb;
	c.a							=density.z;
	brightnessFactor			=unshadowedBrightness(Beta,lightResponse,ambientColour);
	float earthshadowMultiplier	=saturate((fade_texc.x-nearFarTexc.x)/0.1);
#ifdef INFRARED
	c.rgb						=lerp(cloudIrRadiance1,cloudIrRadiance2,saturate(cloudTexCoords.z));//*c.a;
#endif
	c.rgb						=applyFades2( lossTexture, inscatterVolumeTexture,volumeTexCoords,c.rgb,fade_texc,earthshadowMultiplier);

	return c;
}

vec4 calcColourSimple(Texture2D lossTexture, Texture2D inscTexture, Texture2D skylTexture, Texture2D lightTableTexture
, vec4 density, float Beta, float BetaRayleigh, float BetaMie, vec4 lightResponse, vec3 ambientColour
, vec3 world_pos, vec3 cloudTexCoords
, vec2 fade_texc, vec2 nearFarTexc
, out float brightnessFactor)
{
	float alt_texc = world_pos.z / fadeAltitudeRangeKm;
	vec3 combinedLightColour = texture_clamp_lod(lightTableTexture, vec2(alt_texc, 3.5 / 4.0), 0).rgb;
	ambientColour = lightResponse.w*texture_clamp_lod(lightTableTexture, vec2(alt_texc, 2.5 / 4.0), 0).rgb;

	vec3 ambient = density.w*ambientColour.rgb;
	float opacity = density.z;
	vec4 c;
	c.rgb = (density.y*Beta + lightResponse.y*density.x)*combinedLightColour + ambient.rgb;
	c.a = opacity;
	brightnessFactor = unshadowedBrightness(Beta, lightResponse, ambientColour);
	//c.rgb += (1.0 - density.x)*calcLightningColour(world_pos, lightningColour.rgb*lightningColour.w, lightningOrigin, lightningInvScales);
	float earthshadowMultiplier = saturate((fade_texc.x - nearFarTexc.x) / 0.1);
#ifdef INFRARED
	c.rgb = lerp(cloudIrRadiance1, cloudIrRadiance2, saturate(cloudTexCoords.z));//*c.a;
#endif
	vec3 loss = sampleLod(lossTexture, cmcSamplerState, fade_texc, 0).rgb;
	c.rgb *= loss;
	vec4 insc = sampleLod(inscTexture, cmcSamplerState, fade_texc, 0);
	vec3 skyl = sampleLod(skylTexture, cmcSamplerState, fade_texc, 0).rgb;
	vec3 inscatter =  earthshadowMultiplier*PrecalculatedInscatterFunction(insc, BetaRayleigh, BetaMie, mieRayleighRatio);
	c.rgb += inscatter + skyl;
	return c;
}

vec4 MakeNoise(Texture3D noiseTexture3D,vec3 noise_texc,float lod)
{
	vec4 noiseval		=vec4(0,0,0,0);
	//float mult			=1.0;///(1.0+noise3DPersistence);

	noiseval			+=texture_3d_wrap_lod(noiseTexture3D,noise_texc,0);
	//noise_texc			*=noise3DOctaveScale;
	//mult				*=noise3DPersistence;
	
	//noiseval.w			=length(noiseval);//0.5;
	//noiseval.xy*=2;
	return noiseval;
}

vec4 calcDensity(Texture3D cloudDensity,vec3 texCoords,float layerFade,vec4 noiseval,vec3 fractalScale)
{
	float noise_factor	=lerp(baseNoiseFactor,1.0,saturate(texCoords.z));
	//vec4 light			=sampleLod(cloudDensity,cloudSamplerState,texCoords,0);
	noiseval.rgb		*=noise_factor;
	vec3 pos			=texCoords.xyz+fractalScale.xyz*noiseval.xyz;
	vec4 density		=sample_3d_lod(cloudDensity,cloudSamplerState,pos,0);
	//density.xyw			=light.xyw;
		//	density.xy*=.5*(1+density.z);
	density.z			*=layerFade;//*(1.0-noiseval.w);
	density.z			=saturate(density.z*(1.0+alphaSharpness));//-alphaSharpness);
	return density;
}

/*

vec3 calcLightningColour(vec3 world_pos,vec3 lightningColour,vec3 lightningOrigin,vec3 lightningInvScales)
{
	vec3 texCoords=(world_pos-lightningOrigin)*lightningInvScales;
	float diff=length(texCoords-vec3(.5,.5,.5));
	float b=1.0/pow(diff+.0001,2.0);
	vec3 colour=b*lightningColour;
	return colour;
}
*/
FarNearPixelOutput Lightpass(Texture3D cloudDensity
								,Texture2D rainMapTexture
								,Texture3D noiseTexture3D
								,Texture2D noiseTexture
								,Texture2D lossTexture
								,DepthIntepretationStruct depthInterpretationStruct
								,vec4 dlookup
								,vec2 texCoords
								,vec3 source_pos_w
								,float source_radius
								,vec3 spectralFluxOver1e6
								,float maxCosine
								,float maxIlluminatedRadius
								,float threshold)
{
	float max_spectral_flux			=max(max(spectralFluxOver1e6.r,spectralFluxOver1e6.g),spectralFluxOver1e6.b);
	FarNearPixelOutput res;
	res.farColour			=vec4(0,0,0,1.0);
	res.nearColour			=vec4(0,0,0,1.0);
	vec4 clip_pos			=vec4(-1.0,1.0,1.0,1.0);
	clip_pos.x				+=2.0*texCoords.x;
	clip_pos.y				-=2.0*texCoords.y;
	vec3 view				=normalize(mul(invViewProj,clip_pos).xyz);

	float s					=saturate((directionToSun.z+MIN_SUN_ELEV)/0.01);
	vec3 lightDir			=lerp(directionToMoon,directionToSun,s);

	float cos0				=dot(lightDir.xyz,view.xyz);
	float sine				=view.z;

	float min_z				=cornerPosKm.z-(fractalScale.z*1.5)/inverseScalesKm.z;
	float max_z				=cornerPosKm.z+(1.0+fractalScale.z*1.5)/inverseScalesKm.z;
	if(view.z<-0.1&&viewPosKm.z<cornerPosKm.z-fractalScale.z/inverseScalesKm.z)
		return res;
	
	vec2 solidDist_nearFar	=depthToFadeDistance(dlookup.yx,clip_pos.xy,depthInterpretationStruct,tanHalfFov);
	vec2 fade_texc			=vec2(0.0,0.5*(1.0-sine));
	// Lookup in the illumination texture.
	vec2 illum_texc			=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	float meanFadeDistance	=1.0;
	// Precalculate hg effects
	float BetaClouds		=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	float BetaRayleigh		=CalcRayleighBeta(cos0);
	float BetaMie			=HenyeyGreenstein(hazeEccentricity,cos0);

	vec3 world_pos					=viewPosKm;

	// This provides the range of texcoords that is lit.
	int3 c_offset					=int3(sign(view.x),sign(view.y),sign(view.z));
	int3 start_c_offset				=-c_offset;
	start_c_offset					=int3(max(start_c_offset.x,0),max(start_c_offset.y,0),max(start_c_offset.z,0));
	vec3 viewScaled					=view/scaleOfGridCoords;
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
	float viewScale					=length(viewScaled*scaleOfGridCoords);
	// origin of the grid - at all levels of detail, there will be a slice through this in 3 axes.
	vec3 startOffsetFromOrigin		=viewPosKm-gridOriginPosKm;
	vec3 offsetFromOrigin			=world_pos-gridOriginPosKm;
	vec3 p0							=offsetFromOrigin/scaleOfGridCoords;
	int3 c0							=int3(floor(p0) + start_c_offset);
	vec3 gridScale					=scaleOfGridCoords;
	vec3 P0							=offsetFromOrigin/scaleOfGridCoords/2.0;
	int3 C0							=c0>>1;
	
	float distanceKm				=length(offset_vec);
	int3 c							=c0;

	int idx=0;
	float W							=halfClipSize;
	const float start				=0.866*0.866;//0.707 for 2D, 0.866 for 3D;
	const float ends				=1.0;
	const float range				=ends-start;
	vec3 volume_texc				=ScreenToVolumeTexcoords(clipPosToScatteringVolumeMatrix,texCoords,0.0);

	vec4 colour						=vec4(0.0,0.0,0.0,1.0);
	vec4 nearColour					=vec4(0.0,0.0,0.0,1.0);
	float lastFadeDistance			=0.0;
	// x starts at 1, so gets initialized at the first cloud found.
	// y starts at 0, so gets the furthest value.
	vec4 nearFarDepth				= vec4(1.0, 0.0, 0.0, 0.0);
	int3 b							=abs(c-C0*2);
	for(int j=0;j<8;j++)
	{
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
		//D+=100.0*step(D,vec3(0,0,0));
		float e						=min(min(D.x,D.y),D.z);
		// All D components are positive. Only the smallest is equal to e. Step(x,y) returns (y>=x). So step(D.x,e) returns (e>=D.x), which is only true if e==D.x
		vec3 N						=step(D,vec3(e,e,e));

		int3 c_step					=c_offset*int3(N);
		float d						=e*viewScale;
		distanceKm					+=(d);

		// What offset was the original position from the centre of the cube?
		p1							=p+e*viewScaled;
		vec3 d0						=normalize(2.0*abs(frac(p1)-vec3(.5,.5,.5)));
	
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
		vec3 pw						=abs(p1-p0);//+start_c_offset
		float fade_inter			=saturate((length(pw.xy)/(float(W)*(2.0-is_inter)-1.0)-start)/range);// /(2.0-is_inter)
	
		float fade					=1.0-(fade_inter);
		float fadeDistance			=saturate(distanceKm/maxFadeDistanceKm);

		int3 b						=abs(c-C0*2);
		if(fade>0)
		{
			vec3 noise_texc			=world_pos.xyz*noise3DTexcoordScale+noise3DTexcoordOffset;

			vec4 noiseval			=vec4(0,0,0,0);
			noiseval				=MakeNoise(noiseTexture3D,noise_texc,3.0*fadeDistance);
			vec4 density			=calcDensity(cloudDensity,cloudTexCoords,fade,noiseval,fractalScale);
			
			if(density.z>0)
			{
				float brightness_factor;
				float cloud_density		=density.z;
				nearFarDepth.x			=min(nearFarDepth.y, fadeDistance);
				nearFarDepth.y			=fadeDistance;
				float cosine			=abs(dot(N,viewScaled));
				density.z				*=cosine;
				density.z				*=cosine;
				density.z				*=saturate(distanceKm/0.24);
				fade_texc.x				=sqrt(fadeDistance);
				vec3 volumeTexCoords	=vec3(texCoords,sqrt(fadeDistance));
				vec3 dist				=world_pos-source_pos_w;
				float radius_km			=max(source_radius,length(dist));
				float radiance			=1.0/(4.0*3.14159*radius_km*radius_km);
				vec4 clr				=vec4(spectralFluxOver1e6.rgb*radiance*density.z,density.z);
				brightness_factor		=max(1.0,radiance*max_spectral_flux);

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
	colour.rgb			=max(vec3(0,0,0),(colour.rgb-vec3(threshold,threshold,threshold))/(1.0+threshold));
	nearColour.rgb		=max(vec3(0,0,0),(nearColour.rgb-vec3(threshold,threshold,threshold))/(1.0+threshold));
    res.farColour		=vec4(exposure*colour.rgb,colour.a);
    res.nearColour		=vec4(exposure*nearColour.rgb,nearColour.a);

	return res;
}

float GetRainAtOffsetKm(Texture2D rainMapTexture,vec3 cloudWorldOffsetKm,vec3 inverseScalesKm,vec3 world_pos_km,vec2 rainCentreKm,float rainRadiusKm)
{
	vec3 rain_texc		=cloudWorldOffsetKm;
	rain_texc.xy		+=rain_texc.z*rainTangent;
	float rain_lookup	=texture_wrap_lod(rainMapTexture,rain_texc.xy*inverseScalesKm.xy,0).x;
	//vec4 streak			=texture_wrap_lod(noiseTexture,0.00003*rain_texc.xy,0);
	return				rain_lookup
							*saturate((rainRadiusKm-length(world_pos_km.xy-rainCentreKm.xy))*3.0)
							*saturate(1.0-cloudWorldOffsetKm.z/500.0)
							*saturate(cloudWorldOffsetKm.z/1000.0+1.0)//+4.0*streak.y)
							//*(0.4+0.6*streak.x)
							;
}

RaytracePixelOutput RaytraceCloudsForward(Texture3D cloudDensity
											,Texture2D rainMapTexture
											,Texture3D noiseTexture3D
											,Texture2D noiseTexture
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
											,bool noise_3d
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
	vec3 view				=normalize(mul(invViewProj,clip_pos).xyz);

	float s					=saturate((directionToSun.z+MIN_SUN_ELEV)/0.01);
	vec3 lightDir			=lerp(directionToMoon,directionToSun,s);

	float cos0				=dot(lightDir.xyz,view.xyz);
	float sine				=view.z;

	float min_z				=cornerPosKm.z-(fractalScale.z*1.5)/inverseScalesKm.z;
	float max_z				=cornerPosKm.z+(1.0+fractalScale.z*1.5)/inverseScalesKm.z;
	if(do_rain_effect)
		min_z				=-1.0;
	else if(view.z<-0.1&&viewPosKm.z<cornerPosKm.z-fractalScale.z/inverseScalesKm.z)
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

	vec3 world_pos					=viewPosKm;

	// This provides the range of texcoords that is lit.
	int3 c_offset					=int3(sign(view.x),sign(view.y),sign(view.z));
	int3 start_c_offset				=-c_offset;
	start_c_offset					=int3(max(start_c_offset.x,0),max(start_c_offset.y,0),max(start_c_offset.z,0));
	vec3 viewScaled					=view/scaleOfGridCoords;
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
	float viewScale					=length(viewScaled*scaleOfGridCoords);
	// origin of the grid - at all levels of detail, there will be a slice through this in 3 axes.
	vec3 startOffsetFromOrigin		=viewPosKm-gridOriginPosKm;
	vec3 offsetFromOrigin			=world_pos-gridOriginPosKm;
	vec3 p0							=offsetFromOrigin/scaleOfGridCoords;
	int3 c0							=int3(floor(p0) + start_c_offset);
	vec3 gridScale					=scaleOfGridCoords;
	vec3 P0							=offsetFromOrigin/scaleOfGridCoords/2.0;
	int3 C0							=c0>>1;
	
	float distanceKm				=length(offset_vec);
	int3 c							=c0;

	int idx=0;
	float W							=halfClipSize;
	const float start				=0.866*0.866;//0.707 for 2D, 0.866 for 3D;
	const float ends				=1.0;
	const float range				=ends-start;
	vec3 volume_texc				=ScreenToVolumeTexcoords(clipPosToScatteringVolumeMatrix,texCoords,0.0);

	vec4 colour						=vec4(0.0,0.0,0.0,1.0);
	vec4 nearColour					=vec4(0.0,0.0,0.0,1.0);
	float lastFadeDistance			=0.0;
	// x starts at 1, so gets initialized at the first cloud found.
	// y starts at 0, so gets the furthest value.
	vec4 nearFarDepth				= vec4(1.0, 0.0, 0.0, 0.0);
	int3 b							=abs(c-C0*2);
	for(int i=0;i<8;i++)
	{
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
		//D+=100.0*step(D,vec3(0,0,0));
		float e						=min(min(D.x,D.y),D.z);
		// All D components are positive. Only the smallest is equal to e. Step(x,y) returns (y>=x). So step(D.x,e) returns (e>=D.x), which is only true if e==D.x
		vec3 N						=step(D,vec3(e,e,e));

		int3 c_step					=c_offset*int3(N);
		float d						=e*viewScale;
		distanceKm					+=(d);

		// What offset was the original position from the centre of the cube?
		p1							=p+e*viewScaled;
		vec3 d0						=normalize(2.0*abs(frac(p1)-vec3(.5,.5,.5)));
	
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
		vec3 pw						=abs(p1-p0);//+start_c_offset
		float fade_inter			=saturate((length(pw.xy)/(float(W)*(2.0-is_inter)-1.0)-start)/range);// /(2.0-is_inter)
	
		float fade					=1.0-(fade_inter);
		float fadeDistance			=saturate(distanceKm/maxFadeDistanceKm);

		int3 b						=abs(c-C0*2);
		if(fade>0)
		{
			vec3 noise_texc			=world_pos.xyz*noise3DTexcoordScale+noise3DTexcoordOffset;

			vec4 noiseval			=vec4(0,0,0,0);
			if(noise)
				noiseval			=MakeNoise(noiseTexture3D,noise_texc,3.0*fadeDistance);
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
				vec4 clr;
				float brightness_factor;
				float cloud_density		=density.z;
#if 0
				clr=vec4(1,1,0,1);
				brightness_factor=1.0;
#else
				nearFarDepth.x			=min(nearFarDepth.y, fadeDistance);
				nearFarDepth.y			=fadeDistance;
				float cosine			=abs(dot(N,viewScaled));
				density.z				*=cosine;
				density.z				*=cosine;
				density.z				*=saturate(distanceKm/0.24);
				fade_texc.x				=sqrt(fadeDistance);
				vec3 volumeTexCoords	=vec3(texCoords,sqrt(fadeDistance));
				if (noise)
					clr				=calcColour(lossTexture,inscatterVolumeTexture,volumeTexCoords,lightTableTexture
													,density
													,BetaClouds
													,lightResponse,ambientColour
													,world_pos,cloudTexCoords
													,fade_texc,nearFarTexc
													,brightness_factor);
				else
					clr				=calcColourSimple(lossTexture,inscTexture,skylTexture,lightTableTexture
													,density
													,BetaClouds,BetaRayleigh,BetaMie
													,lightResponse,ambientColour
													,world_pos,cloudTexCoords
													,fade_texc,nearFarTexc
													,brightness_factor);
				//clr.rgb=saturate(distanceKm/100.0);
				///clr.rg=solidDist_nearFar.xy;
			//	clr.rgb=saturate(-D);
				if(do_depth_mix)
				{
					vec4 clr_n=clr;
					clr.a				*=saturate((solidDist_nearFar.y-fadeDistance)/0.01);
					clr_n.a				*=saturate((solidDist_nearFar.x-fadeDistance)/0.01);
					nearColour.rgb		+=clr_n.rgb*clr_n.a*(nearColour.a);
					nearColour.a		*=(1.0-clr_n.a);
				}
#endif
				colour.rgb				+=clr.rgb*clr.a*(colour.a);
				meanFadeDistance		=lerp(meanFadeDistance,fadeDistance,saturate(4.0*density.z)*colour.a);
				colour.a				*=(1.0-clr.a);
				if(nearColour.a*brightness_factor<0.003)
				{
					colour.a			=0.0;
					break;
				}
			}
		}
		lastFadeDistance=fadeDistance;
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
