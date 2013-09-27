#ifndef ATMOSPHERICS_SL
#define ATMOSPHERICS_SL

vec3 AtmosphericsLoss(Texture2D depthTexture,Texture2D lossTexture
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
	float dist			=1.0;//depthToFadeDistance(depth,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
	float sine			=view.z;
	float2 fade_texc	=vec2(pow(dist,0.5f),0.5f*(1.f-sine));
	
	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec4 illum_lookup	=texture_wrap_mirror(illuminationTexture,illum_texc);
	vec2 nearFarTexc	=vec2(0.0,1.0);//illum_lookup.xy;
	vec2 near_texc		=vec2(min(nearFarTexc.x,fade_texc.x),fade_texc.y);
	vec2 far_texc		=vec2(min(nearFarTexc.y,fade_texc.x),fade_texc.y);
	vec4 insc_near		=texture_clamp_mirror(inscTexture,near_texc);
	vec4 insc_far		=texture_clamp_mirror(inscTexture,far_texc);
	return insc_far.rgb/20.0;
	vec4 insc			=vec4(insc_far.rgb-insc_near.rgb,0.5*(insc_near.a+insc_far.a));
	float cos0			=dot(view,lightDir);
	float3 colour		=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);

	colour				+=texture_clamp_mirror(skylTexture,fade_texc).rgb;
	colour				*=exposure;
    return				float4(colour.rgb,1.f);
}
#endif
