#ifndef CLOUDS_SL
#define CLOUDS_SL
#ifndef GLSL
Texture3D cloudDensity1	: register(t0);
Texture3D cloudDensity2	: register(t1);
Texture2D noiseTexture	: register(t2);
Texture2D cloudShadowTexture	: register(t2);

Texture2D lossTexture	: register(t3);
Texture2D inscTexture	: register(t4);
Texture2D skylTexture	: register(t5);
Texture2D depthTexture	: register(t6);
Texture3D noiseTexture3D: register(t7);
Texture3D lightningIlluminationTexture: register(t8);
Texture3D cloudDensity			: register(t9);
Texture2D illuminationTexture	: register(t10);
SamplerState cloudSamplerState	: register( s0);
#endif

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
	const int NUM_STEPS=16;
	for(int i=0;i<NUM_STEPS;i++)
	{
		float u						=1.0-float(i)/float(NUM_STEPS);
		vec3 cartesian				=vec3(pos_xy.xy,u);
		vec3 wpos					=mul(shadowMatrix,vec4(cartesian,1.0)).xyz;
		vec3 texc					=(wpos-cornerPos)*inverseScales;
		vec4 density1				=sampleLod(cloudDensity1,wwcSamplerState,texc,0);
		vec4 density2				=sampleLod(cloudDensity2,wwcSamplerState,texc,0);
		vec4 density				=lerp(density1,density2,cloud_interp);
		if(density.z>0)
		{
			illumination			=lerp(illumination,vec2(0,0),density1.z);//density.xy
			U						=lerp(U,u,density.z);
		}
	}
	//float edge						=saturate((1.0-distance_off_centre)/0.04);
	//float4 result		=vec4(illumination,Z,1.0);
	//illumination		=1.0-(1.0-illumination)*exp(-IN.texCoords.x*4.0);
/*	if(illumination.y<.5)
		illumination.y=0;
	else
		illumination.y=1.0;*/
	return vec4(illumination,U,1.0);//*edge
}

// from the viewer, trace outwards to find the outer and inner ranges of cloud shadow.
// Then the outer and inner shadow distances are put in the xy.
// Within that, the outer and inner lit distances are put in the zw.
vec4 CloudShadowNearFar(Texture2D cloudShadowTexture,int shadowTextureSize,vec2 texCoords)
{
	//float theta						=texCoords.x*2.0*3.1415926536;
	int in_shadow					=0;		//0 = start, 1=in shadow, not yet in light, 2=in both
	vec2 shadow_range				=vec2(1.0,0.0);
	vec2 light_range				=vec2(1.0,0.0);
	const float U					=1.0;
	const float L					=0.0;
	int N							=2*shadowTextureSize;
	float pixel						=1.0/float(shadowTextureSize);
#ifdef RADIAL_CLOUD_SHADOW
	vec2 offset						=vec2(0,pixel/1.0);
#else
	vec2 offset						=vec2(-sin(theta),cos(theta))*pixel/4.0;
#endif
	// First find the range where ther is ANY shadow:
	for(int i=0;i<N;i++)
	{
		float interp					=float(i)/float(N-1);
#ifdef RADIAL_CLOUD_SHADOW
		vec2 shadow_texc				=vec2(sqrt(interp),texCoords.x);
#else
		float distance_off_centre		=interp;
		vec2 shadow_texc				=0.5*(distance_off_centre*vec2(cos(theta),sin(theta))+1.0);
#endif
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
	int in_light=0;
	// Second, within this range, find where there is ANY light.
	for(int j=0;j<N;j++)
	{
		float interp					=float(j)/float(N-1);
#ifdef RADIAL_CLOUD_SHADOW
		vec2 shadow_texc				=vec2(sqrt(interp),texCoords.x);
#else
		float distance_off_centre		=interp;
		vec2 shadow_texc				=0.5*(distance_off_centre*vec2(cos(theta),sin(theta))+1.0);
#endif
		vec4 illumination				=sampleLod(cloudShadowTexture,cwcNearestSamplerState,shadow_texc,0);
		illumination					+=sampleLod(cloudShadowTexture,clampWrapSamplerState,shadow_texc-offset,0);
		illumination					+=sampleLod(cloudShadowTexture,clampWrapSamplerState,shadow_texc+offset,0);
		if(interp>=shadow_range.x&&interp<=shadow_range.y)
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
    float4 lookup=cloudShadowTexture.Sample(clampWrapSamplerState,radial_texc);
#else
    float4 lookup=cloudShadowTexture.SampleLevel(clampWrapSamplerState,IN.texCoords.xy,3);//radial_texc);
#endif
    vec4 nearFarShadowLight=nearFarTexture.Sample(wrapSamplerState,vec2(radial_texc.y,0.5));
	if(dist>=nearFarShadowLight.x&&dist<=nearFarShadowLight.y)
	{
		lookup*=0.5;
		if(dist>=nearFarShadowLight.z&&dist<=nearFarShadowLight.w)
			lookup+=0.5;
	}
	//if(abs(radial_texc.y)<0.003)
	lookup.r+=abs(radial_texc.y);
	return float4(lookup.rgb,1.0);
}
#endif
