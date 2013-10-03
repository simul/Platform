#ifndef ATMOSPHERICS_SL
#define ATMOSPHERICS_SL

vec3 AtmosphericsLoss(Texture2D depthTexture,vec4 viewportToTexRegionScaleBias,Texture2D lossTexture
	,mat4 invViewProj,vec2 texCoords,vec2 clip_pos,vec3 depthToLinFadeDistParams,vec2 tanHalfFov)
{
	float3 view		=mul(invViewProj,vec4(clip_pos.xy,1.0,1.0)).xyz;
	view			=normalize(view);
	vec2 depth_texc	=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	float depth		=texture_clamp(depthTexture,depth_texc).x;
	float dist		=depthToFadeDistance(depth,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
	float sine		=view.z;
	vec2 texc2		=vec2(pow(dist,0.5),0.5*(1.f-sine));
	vec3 loss		=texture_clamp_mirror(lossTexture,texc2).rgb;
	return loss;
}

vec4 CalcInsc(	Texture2D inscTexture
				,Texture2D illuminationTexture
				,Texture2D depthTexture
				,vec2 texCoords
				,vec2 clip_pos
				,vec2 fade_texc
				,vec2 illum_texc
				,vec4 viewportToTexRegionScaleBias
				,vec3 depthToLinFadeDistParams
				,vec2 tanHalfFov)
{
	vec2 depth_texc		=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	float depth			=texture_clamp_lod(depthTexture,depth_texc,0).x;
	float dist			=depthToFadeDistance(depth,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
	fade_texc.x			=pow(dist,0.5f);
	vec4 illum_lookup	=texture_wrap_mirror(illuminationTexture,illum_texc);
	vec2 nearFarTexc	=illum_lookup.xy;
	vec2 near_texc		=vec2(min(nearFarTexc.x,fade_texc.x),fade_texc.y);
	vec2 far_texc		=vec2(min(nearFarTexc.y,fade_texc.x),fade_texc.y);
	vec4 insc_near		=texture_clamp_mirror(inscTexture,near_texc);
	vec4 insc_far		=texture_clamp_mirror(inscTexture,far_texc);
	return vec4(insc_far.rgb-insc_near.rgb,0.5*(insc_near.a+insc_far.a));
}

vec3 AtmosphericsInsc(	Texture2D depthTexture
						,Texture2D illuminationTexture
						,Texture2D inscTexture
						,Texture2D skylTexture
						,mat4 invViewProj
						,vec2 texCoords
						,vec2 clip_pos
						,vec4 viewportToTexRegionScaleBias
						,vec3 depthToLinFadeDistParams
						,vec2 tanHalfFov
						,float hazeEccentricity
						,vec3 lightDir
						,vec3 mieRayleighRatio)
{
	vec3 view			=normalize(mul(invViewProj,vec4(clip_pos,1.0,1.0)).xyz);
	view				=normalize(view);
	vec2 depth_texc		=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	float depth			=texture_clamp(depthTexture,depth_texc).x;
	float dist			=depthToFadeDistance(depth,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
	
	float sine			=view.z;
	
	float2 fade_texc	=vec2(pow(dist,0.5f),0.5f*(1.f-sine));
	
	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec4 illum_lookup	=texture_wrap_mirror(illuminationTexture,illum_texc);
	vec2 nearFarTexc	=vec2(0.0,1.0);//illum_lookup.xy;
	vec2 near_texc		=vec2(min(nearFarTexc.x,fade_texc.x),fade_texc.y);
	vec2 far_texc		=vec2(min(nearFarTexc.y,fade_texc.x),fade_texc.y);
	vec4 insc_near		=texture_clamp_mirror(inscTexture,near_texc);
	vec4 insc_far		=texture_clamp_mirror(inscTexture,far_texc);

	vec4 insc			=vec4(insc_far.rgb-insc_near.rgb,0.5*(insc_near.a+insc_far.a));
	float cos0			=dot(view,lightDir);
	float3 colour		=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);

	colour				+=texture_clamp_mirror(skylTexture,fade_texc).rgb;
    return				float4(colour.rgb,1.f);
}

vec4 InscatterMSAA(	Texture2D inscTexture
				,Texture2D skylTexture
				,Texture2D illuminationTexture
				,Texture2D depthTexture
				,vec2 depthPixelScales
				,vec2 texCoords
				,mat4 invViewProj
				,vec3 lightDir
				,float hazeEccentricity
				,vec3 mieRayleighRatio
				,vec4 viewportToTexRegionScaleBias
				,vec3 depthToLinFadeDistParams
				,vec2 tanHalfFov)
{
	vec4 clip_pos		=vec4(-1.f,1.f,1.f,1.f);
	clip_pos.x			+=2.0*texCoords.x;
	clip_pos.y			-=2.0*texCoords.y;
	vec3 view			=normalize(mul(invViewProj,clip_pos).xyz);
	view				=normalize(view);
	vec4 insc			=vec4(0,0,0,0);
	vec2 offset[]		={vec2(1.0,1.0),vec2(1.0,0.0),vec2(1.0,-1.0)
							,vec2(-1.0,1.0),vec2(-1.0,0.0),vec2(-1.0,-1.0)
							,vec2(0.0,1.0),vec2(0.0,-1.0),vec2(0.0,0.0)};
	float sine			=view.z;
	float2 fade_texc	=vec2(0,0.5f*(1.f-sine));
	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec2 textureDims;
	//uint2 dims;
	///depthTexture.GetDimensions(dims.x,dims.y);
	//depthPixelScales=.5/vec2(dims);
	for(int i=0;i<9;i++)
	{
		insc	+=CalcInsc(	inscTexture
							,illuminationTexture
							,depthTexture
							,texCoords.xy+offset[i]*depthPixelScales
							,clip_pos
							,fade_texc
							,illum_texc
							,viewportToTexRegionScaleBias
							,depthToLinFadeDistParams
							,tanHalfFov)/9.0;
	}
	float cos0			=dot(view,lightDir);
	float3 colour		=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour				+=texture_clamp_mirror(skylTexture,fade_texc).rgb;
    return float4(colour.rgb,1.f);
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
				,vec3 depthToLinFadeDistParams
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
	
	vec4 insc			=CalcInsc(	inscTexture
							,illuminationTexture
							,depthTexture
							,texCoords.xy
							,clip_pos
							,fade_texc
							,illum_texc
							,viewportToTexRegionScaleBias
							,depthToLinFadeDistParams
							,tanHalfFov);
	float cos0			=dot(view,lightDir);
	float3 colour		=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour				+=texture_clamp_mirror(skylTexture,fade_texc).rgb;
    return float4(colour.rgb,1.f);
}


#endif
