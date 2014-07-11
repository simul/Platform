#ifndef ATMOSPHERICS_SL
#define ATMOSPHERICS_SL

vec3 AtmosphericsLoss(Texture2D depthTexture,vec4 viewportToTexRegionScaleBias,Texture2D lossTexture
	,mat4 invViewProj,vec2 texCoords,vec2 clip_pos,vec4 depthToLinFadeDistParams,vec2 tanHalfFov)
{
	float3 view		=mul(invViewProj,vec4(clip_pos.xy,1.0,1.0)).xyz;
	view			=normalize(view);
	vec2 depth_texc	=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	float depth		=texture_clamp(depthTexture,depth_texc).y;
	//discardOnFar(depth);
	float dist		=depthToFadeDistance(depth,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
	float sine		=view.z;
	vec2 texc2		=vec2(pow(dist,0.5),0.5*(1.f-sine));
	vec3 loss		=texture_clamp_mirror(lossTexture,texc2).rgb;
	return loss;
}

vec3 AtmosphericsLossMSAA(Texture2DMS<float4> depthTextureMS,uint i,vec4 viewportToTexRegionScaleBias,Texture2D lossTexture
	,mat4 invViewProj,vec2 texCoords,uint2 depth_pos2,vec2 clip_pos,vec4 depthToLinFadeDistParams,vec2 tanHalfFov)
{
	float3 view	=mul(invViewProj,vec4(clip_pos.xy,1.0,1.0)).xyz;
	view		=normalize(view);
	float sine	=view.z;
	vec3 loss	=vec3(0,0,0);
	//for(uint i=0;i<numSamples;i++)
	{
		vec4 insc_i;
        vec3 skyl_i;
		vec2 depth_texc		=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
		float depth			=depthTextureMS.Load(depth_pos2,i).x;
		float dist	=depthToFadeDistance(depth,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
		vec2 texc2	=vec2(pow(dist,0.5),0.5*(1.f-sine));
		loss				+=texture_clamp_mirror(lossTexture,texc2).rgb;
	}
	//loss/=float(numSamples);
	return loss;
}

void CalcInsc(	Texture2D inscTexture
				,Texture2D skylTexture
				,Texture2D illuminationTexture
				,float dist
				,vec2 fade_texc
				,vec2 illum_texc
                ,out vec4 insc
                ,out vec3 skyl)
{
	fade_texc.x			=pow(dist,0.5f);
	vec4 illum_lookup	=texture_wrap_mirror(illuminationTexture,illum_texc);
	vec2 nearFarTexc	=illum_lookup.xy;
	vec2 near_texc		=vec2(min(nearFarTexc.x,fade_texc.x),fade_texc.y);
	vec2 far_texc		=vec2(min(nearFarTexc.y,fade_texc.x),fade_texc.y);
	vec4 insc_near		=texture_clamp_mirror(inscTexture,near_texc);
	vec4 insc_far		=texture_clamp_mirror(inscTexture,far_texc);
	insc                =vec4(insc_far.rgb-insc_near.rgb,0.5*(insc_near.a+insc_far.a));
    skyl                =texture_clamp_mirror(skylTexture,fade_texc).rgb;
}

vec4 Inscatter(	Texture2D inscTexture
						,Texture2D skylTexture
						,Texture2D depthTexture
						,int numSamples
						,Texture2D illuminationTexture
						,mat4 invViewProj
						,vec2 texCoords
						,vec3 lightDir
						,float hazeEccentricity
						,vec3 mieRayleighRatio
						,vec4 viewportToTexRegionScaleBias
						,vec4 depthToLinFadeDistParams
						,vec2 tanHalfFov
						,bool USE_NEAR_FAR
						,bool nearPass)
{
	vec2 clip_pos	=vec2(-1.0,1.0);
	clip_pos.x		+=2.0*texCoords.x;
	clip_pos.y		-=2.0*texCoords.y;
	vec3 view		=normalize(mul(invViewProj,vec4(clip_pos,1.0,1.0)).xyz);
	view			=normalize(view);

	vec2 depth_texc		=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	vec4 depth_lookup	=texture_clamp(depthTexture,depth_texc);
	float dist			=depthToFadeDistance(depth_lookup.x,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
	float sine			=view.z;
	
	vec2 fade_texc	=vec2(pow(dist,0.5f),0.5f*(1.f-sine));
	vec2 illum_texc	=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);

	vec4 insc;
	vec3 skyl;
	CalcInsc(	inscTexture
				,skylTexture
				,illuminationTexture
				,dist
				,fade_texc
				,illum_texc
				,insc
				,skyl);
#ifdef INFRARED
	vec3 colour			=skyl.rgb;
    colour.rgb			*=infraredIntegrationFactors.xyz;
    float final_radiance=colour.x+colour.y+colour.z;
	return vec4(final_radiance,final_radiance,final_radiance,final_radiance);
#else
	float cos0			=dot(view,lightDir);
	vec3 colour	    	=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour				+=skyl.rgb;
	return vec4(colour,1.0);
#endif
}

// In depthTextureNF, x=far, y=near, z=edge
vec4 Inscatter_NFDepth(	Texture2D inscTexture
				,Texture2D skylTexture
				,Texture2D illuminationTexture
				,Texture2D depthTextureNF
				,vec2 texCoords
				,mat4 invViewProj
				,vec3 lightDir
				,float hazeEccentricity
				,vec3 mieRayleighRatio
				,vec4 viewportToTexRegionScaleBias
				,vec4 depthToLinFadeDistParams
				,vec2 tanHalfFov
				,bool USE_NEAR_FAR
				,bool nearPass)
{
	vec2 depth_texc		=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	vec4 depth_lookup	=texture_nearest_lod(depthTextureNF,depth_texc,0);
	float depth			=0.0;
	if(nearPass)
	{
		depth			=depth_lookup.y;
		if(depth_lookup.z==0)
			discard;
	}
	else
		depth			=depth_lookup.x;
	vec2 clip_pos		=vec2(-1.0,1.0);
	clip_pos.x			+=2.0*texCoords.x;
	clip_pos.y			-=2.0*texCoords.y;
	vec3 view			=normalize(mul(invViewProj,vec4(clip_pos,1.0,1.0)).xyz);
	view				=normalize(view);
	vec4 insc			=vec4(0,0,0,0);
	vec3 skyl			=vec3(0,0,0);
	vec2 offset[]		={	vec2(1.0,1.0),vec2(1.0,0.0),vec2(1.0,-1.0)
							,vec2(-1.0,1.0),vec2(-1.0,0.0),vec2(-1.0,-1.0)
							,vec2(0.0,1.0),vec2(0.0,-1.0),vec2(0.0,0.0)};
	float sine			=view.z;
	float2 fade_texc	=vec2(0,0.5f*(1.f-sine));
	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	
	
	float dist			=depthToFadeDistance(depth,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
	
    CalcInsc(	inscTexture
				,skylTexture
				,illuminationTexture
				,dist
				,fade_texc
				,illum_texc
                ,insc
				,skyl);
	float cos0			=dot(view,lightDir);
#ifdef INFRARED
	vec3 colour			=skyl.rgb;
    colour.rgb			*=infraredIntegrationFactors.xyz;
    float final_radiance=colour.x+colour.y+colour.z;
	return float4(final_radiance,final_radiance,final_radiance,1.f);
#else

	vec3 colour	    	=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour				+=skyl;
	
	return float4(colour.rgb,1.0);
#endif
}

vec4 InscatterMSAA(	Texture2D inscTexture
				,Texture2D skylTexture
				,Texture2D illuminationTexture
				,Texture2DMS<float4> depthTextureMS
				,int numSamples
				,vec2 texCoords
				,int2 pos2
				,mat4 invViewProj
				,vec3 lightDir
				,float hazeEccentricity
				,vec3 mieRayleighRatio
				,vec4 viewportToTexRegionScaleBias
				,vec4 depthToLinFadeDistParams
				,vec2 tanHalfFov
				,bool USE_NEAR_FAR
				,bool nearPass)
{
	vec2 clip_pos		=vec2(-1.0,1.0);
	clip_pos.x			+=2.0*texCoords.x;
	clip_pos.y			-=2.0*texCoords.y;
	vec3 view			=normalize(mul(invViewProj,vec4(clip_pos,1.0,1.0)).xyz);
	view				=normalize(view);
	vec4 insc			=vec4(0,0,0,0);
	vec3 skyl			=vec3(0,0,0);
	vec2 offset[]		={	vec2(1.0,1.0),vec2(1.0,0.0),vec2(1.0,-1.0)
							,vec2(-1.0,1.0),vec2(-1.0,0.0),vec2(-1.0,-1.0)
							,vec2(0.0,1.0),vec2(0.0,-1.0),vec2(0.0,0.0)};
	float sine			=view.z;
	float2 fade_texc	=vec2(0,0.5f*(1.f-sine));
	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	float extreme_dist	=0.0;
	if(USE_NEAR_FAR)
	{
		if(nearPass)
			extreme_dist	=1000.0;
	}
	vec2 depth_texc		=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	for(int i=0;i<numSamples;i++)
	{
		vec4 insc_i;
        vec3 skyl_i;
		float depth			=depthTextureMS.Load(pos2,i).x;
		float dist			=depthToFadeDistance(depth,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
		if(USE_NEAR_FAR)
		{
			if(nearPass)
			{
				if(dist<extreme_dist)
					extreme_dist=dist;
			}
			else
			{
				if(dist>extreme_dist)
					extreme_dist=dist;
			}
		}
		else
		{
			CalcInsc(	inscTexture
						,skylTexture
						,illuminationTexture
						,dist
						,fade_texc
						,illum_texc
						,insc_i
						,skyl_i);
			insc+=insc_i;
			skyl+=skyl_i;
		}
	}
	if(USE_NEAR_FAR)
	{
        CalcInsc(	inscTexture
					,skylTexture
					,illuminationTexture
					,extreme_dist
					,fade_texc
					,illum_texc
                    ,insc
					,skyl);}
	else
	{
		insc/=float(numSamples);
		skyl/=float(numSamples);
	}
	float cos0			=dot(view,lightDir);
#ifdef INFRARED
	vec3 colour			=skyl.rgb;
    colour.rgb			*=infraredIntegrationFactors.xyz;
    float final_radiance=colour.x+colour.y+colour.z;
	return float4(final_radiance,final_radiance,final_radiance,1.f);
#else
	vec3 colour	    	=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour				+=skyl;

	return float4(colour.rgb,1.0);
#endif
}

vec4 Inscatter(	Texture2D inscTexture
				,Texture2D skylTexture
				,Texture2D illuminationTexture
				,Texture2D depthTexture
				,vec2 texCoords
				,mat4 invViewProj
				,vec3 lightDir
				,float hazeEccentricity
				,vec3 mieRayleighRatio
				,vec4 viewportToTexRegionScaleBias
				,vec4 depthToLinFadeDistParams
				,vec2 tanHalfFov)
{
	vec4 clip_pos		=vec4(-1.f,1.f,1.f,1.f);
	clip_pos.x			+=2.0*texCoords.x;
	clip_pos.y			-=2.0*texCoords.y;
	vec3 view			=normalize(mul(invViewProj,clip_pos).xyz);
	view				=normalize(view);
	vec2 offset[]		={vec2(1.0,1.0),vec2(1.0,0.0),vec2(1.0,-1.0)
							,vec2(-1.0,1.0),vec2(-1.0,0.0),vec2(-1.0,-1.0)
							,vec2(0.0,1.0),vec2(0.0,-1.0),vec2(0.0,0.0)};
	float sine			=view.z;
	float2 fade_texc	=vec2(0,0.5f*(1.f-sine));
	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	
	vec4 insc;
	vec3 skyl;
	vec2 depth_texc		=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	float depth			=texture_clamp_lod(depthTexture,depth_texc,0).x;
	float dist			=depthToFadeDistance(depth,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
	CalcInsc(	inscTexture
				,skylTexture
				,illuminationTexture
				,dist
				,fade_texc
				,illum_texc
				,insc
				,skyl);
	float cos0			=dot(view,lightDir);
	float3 colour		=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour				+=skyl;
    return float4(colour.rgb,1.f);
}


#endif
