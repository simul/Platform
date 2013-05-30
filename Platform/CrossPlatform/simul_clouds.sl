#ifndef CLOUDS_SL
#define CLOUDS_SL

Texture3D cloudDensity1: register(t0);
Texture3D cloudDensity2: register(t1);
Texture2D noiseTexture: register(t2);
Texture2D lossTexture: register(t3);
Texture2D inscTexture: register(t4);
Texture2D skylTexture: register(t5);
Texture2D depthTexture: register(t6);
Texture3D noiseTexture3D: register(t7);
Texture3D lightningIlluminationTexture: register(t8);

SamplerState cloudSamplerState: register( s0)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Clamp;
};
SamplerState fadeSamplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
};

vec4 calcDensity(vec3 texCoords,float layerFade,vec3 noiseval)
{
	vec3 pos=texCoords.xyz+fractalScale.xyz*noiseval;
	vec4 density1=sampleLod(cloudDensity1,cloudSamplerState,pos,0);
	vec4 density2=sampleLod(cloudDensity2,cloudSamplerState,pos,0);
	vec4 density=lerp(density1,density2,cloud_interp);
	density.z*=layerFade;
	density.z=saturate(density.z*(1.f+alphaSharpness)-alphaSharpness);
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

vec3 applyFades(vec3 final,float2 fade_texc,float cos0,float earthshadowMultiplier)
{
	vec3 loss=sampleLod(lossTexture		,fadeSamplerState,fade_texc,0).rgb;
	vec4 insc=sampleLod(inscTexture		,fadeSamplerState,fade_texc,0);
	vec3 skyl=sampleLod(skylTexture		,fadeSamplerState,fade_texc,0).rgb;
	vec3 inscatter=earthshadowMultiplier*InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	final*=loss;
	final+=skyl+inscatter;
    return final;
}
#endif