#ifndef CLOUDS_SL
#define CLOUDS_SL
#ifndef GLSL
Texture3D cloudDensity1	: register(t0);
Texture3D cloudDensity2	: register(t1);
Texture2D noiseTexture	: register(t2);
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

vec4 calcDensity(vec3 pos,float layerFade)
{
	vec4 density1=sampleLod(cloudDensity1,cloudSamplerState,pos,0);
	vec4 density2=sampleLod(cloudDensity2,cloudSamplerState,pos,0);
	vec4 density=lerp(density1,density2,cloud_interp);
	density.z*=layerFade;
	//density.z=saturate(density.z*(1.f+alphaSharpness)-alphaSharpness);
	return density;
}

vec4 calcDensity(vec3 texCoords,float layerFade,vec3 noiseval)
{
	vec3 pos=texCoords.xyz+fractalScale.xyz*noiseval;
	vec4 density1=sampleLod(cloudDensity1,cloudSamplerState,pos,0);
	vec4 density2=sampleLod(cloudDensity2,cloudSamplerState,pos,0);
	vec4 density=lerp(density1,density2,cloud_interp);
	density.z*=layerFade;
	//density.z=saturate(density.z*(1.f+alphaSharpness)-alphaSharpness);
	return density;
}

vec4 calcColour(vec4 density,float cos0,float texz)
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

vec4 calcUnfadedColour(vec3 texCoords,float layerFade,vec3 noiseval,float cos0)
{
	vec4 density=calcDensity(texCoords,layerFade,noiseval);
	if(density.z<=0)
		discard;
	vec4 final=calcColour(density,cos0,texCoords.z);
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
	float distance_off_centre		=pow(texCoords.x,2.0);
	vec2 pos_xy						=2.0*texCoords.xy-1.0;//distance_off_centre*vec2(cos(theta),sin(theta));
	vec2 illumination				=vec2(1.0,1.0);
	float U							=-1.0;//(startZMetres-extentZMetres)/1000.0;
	const int NUM_STEPS=16;
	for(int i=0;i<NUM_STEPS;i++)
	{
		float u						=1.0-float(i)/float(NUM_STEPS);
		//float z					=startZMetres+extentZMetres*(1.0-u);
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
		//Z=wpos.z;
	//	else if(illumination.x<1.0)
		{
		//	break;
		}
	}
	//float4 result		=vec4(illumination,Z,1.0);
	//illumination		=1.0-(1.0-illumination)*exp(-IN.texCoords.x*4.0);
	return vec4(illumination,U,1.0);
}
#endif