//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef COMPOSITE_SL
#define COMPOSITE_SL

#define DEBUG_COMPOSITING


#ifndef PI
#define PI (3.1415926536)
#endif

struct TwoColourCompositeOutput
{
	vec4 add		SIMUL_RENDERTARGET_OUTPUT(0);
	vec4 multiply	SIMUL_RENDERTARGET_OUTPUT(1);
};

struct LookupQuad4
{
	vec4 _11;
	vec4 _21;
	vec4 _12;
	vec4 _22;
};

#define VOLUME_INSCATTER
#define SCREENSPACE_VOL
TwoColourCompositeOutput CompositeAtmospherics(vec4 clip_pos
				,vec2 depth_texc
				,Texture2D farCloudTexture
				,Texture2D nearCloudTexture
				,Texture2D nearFarTexture
				,Texture2D loss2dTexture
				,Texture2D depthTexture
				,mat4 invViewProj
				,DepthIntepretationStruct depthInterpretationStruct
				,vec2 lowResTexCoords
				,Texture3D inscatterVolumeTexture
				,Texture2D shadowTexture)
{
	TwoColourCompositeOutput res;
	// we only care about view.z, i.e. the third element of the vector.
	// so only dot-product the third row of invViewProj, with clip_pos.
#ifdef GLSL
	vec4 zrow					=vec4(invViewProj[0][2],invViewProj[1][2],invViewProj[2][2],invViewProj[3][2]);
#else
	vec4 zrow					=invViewProj._31_32_33_34;
#endif
	float sine					=dot(zrow,clip_pos);
	vec4 nearFarCloud			=texture_wrap_lod(nearFarTexture	,lowResTexCoords		,0);
	// Should NOT be necessary:
	float dd					=(nearFarCloud.x-nearFarCloud.y);
	float depth					=texture_wrap_nearest_lod(depthTexture,depth_texc,0).x;

	float dist					=depthToLinearDistance(depth	,depthInterpretationStruct);
	float dist_rt				=pow(dist,0.5);
	vec4 cloud					=texture_wrap_lod(farCloudTexture,lowResTexCoords,0);
#if 1
	vec3 view					=vec3(clip_pos.xy,1.0);
	vec3 lightspaceView			=normalize((mul(clipPosToScatteringVolumeMatrix,vec4(view,1.0))).xyz);
	vec3 volumeTexCoords		=vec3(atan2(lightspaceView.x,lightspaceView.y)/(2.0*pi),0.5*(1.0+2.0*asin(lightspaceView.z)/pi),dist_rt);
	vec4 insc					=texture_3d_wmc_lod(inscatterVolumeTexture,volumeTexCoords,0);
#else
	vec3 volumeTexCoords		= vec3(lowResTexCoords, dist_rt);
	vec4 insc					=texture_3d_wwc_lod(inscatterVolumeTexture,volumeTexCoords,0);
#endif
	vec4 shadow_lookup			=texture_wrap_lod(shadowTexture, lowResTexCoords, 0);
	float shadow				=shadow_lookup.x;
	vec2 loss_texc				=vec2(dist_rt,0.5*(1.f-sine));
	float hiResInterp			=saturate((dist - nearFarCloud.y) / max(dd,0.000001));
	// we're going to do TWO interpolations. One from zero to the near distance,
	// and one from the near to the far distance.
	float nearInterp			=saturate(dist / max(nearFarCloud.y,0.000001));
	
		vec4 cloudNear				=texture_wrap_lod(nearCloudTexture, lowResTexCoords, 0);
		cloud						=lerp(cloudNear, cloud, hiResInterp);
	
		cloud						=lerp(vec4(0,0,0,1.0),cloud,nearInterp);
	float shadowInterp			=saturate((dist - nearFarCloud.w) / max(nearFarCloud.z-nearFarCloud.w,0.000001));
	//float hiResInterp			=saturate((dist - nearFarCloud.y) / max(dd,0.000001));
	shadow						=lerp(shadow_lookup.y,shadow_lookup.x,shadowInterp);

		insc.rgb					*=cloud.a;
		insc						+=cloud;
		shadow						*=cloud.a;
	res.multiply				=texture_clamp_mirror_lod(loss2dTexture,loss_texc,0)*shadow;
	res.add						=insc;
    return res;
}

TwoColourCompositeOutput CompositeAtmospherics_MSAA(vec4 clip_pos
													,vec2 depth_texc
													,Texture2D farCloudTexture
													,Texture2D nearCloudTexture
													,Texture2D nearFarTexture
													,Texture2D loss2dTexture
													,TEXTURE2DMS_FLOAT4 depthTextureMS
													,int numSamples
													,uint2 fullResDims
													,mat4 invViewProj
													,vec4 viewportToTexRegionScaleBias
													,DepthIntepretationStruct depthInterpretationStruct
													,vec2 lowResTexCoords
													,vec4 fullResToLowResTransformXYWH
													,Texture3D inscatterVolumeTexture
													,Texture2D shadowTexture)
{
	TwoColourCompositeOutput res;
	vec3 lightspaceView			=normalize((mul(clipPosToScatteringVolumeMatrix,vec4(clip_pos.xy,1.0,1.0))).xyz);
#ifdef GLSL
	vec4 zrow					=vec4(invViewProj[0][2],invViewProj[1][2],invViewProj[2][2],invViewProj[3][2]);
#else
	vec4 zrow					=invViewProj._31_32_33_34;
#endif
	int2 fullres_depth_pos2		=int2(depth_texc*vec2(fullResDims.xy));
	
	float sine					=dot(zrow,clip_pos);
	vec4 nearFarCloud			=texture_wrap_lod(nearFarTexture	,lowResTexCoords		,0);
	
	float dd					=abs(nearFarCloud.x-nearFarCloud.y);
	vec4 cloud					=texture_wrap_lod(farCloudTexture,lowResTexCoords,0);
	vec4 cloudNear				=texture_wrap_lod(nearCloudTexture, lowResTexCoords, 0);
	
	float hires_edge			=dd;
	res.add						=vec4(0,0,0,1.0);
	res.multiply				=vec4(0,0,0,0);
		for(int k=0;k<numSamples;k++)
		{
		float depth					=TEXTURE_LOAD_MSAA(depthTextureMS,fullres_depth_pos2,k).x;

		float dist					=depthToLinearDistance(depth	,depthInterpretationStruct);
		float dist_rt				=pow(dist,0.5);
		
		vec3 volumeTexCoords		= vec3(lowResTexCoords, dist_rt);
		vec4 shadow_lookup			= vec4(1.0, 1.0, 0, 0);
		shadow_lookup				=texture_wrap_lod(shadowTexture, lowResTexCoords, 0);
		float shadow				=shadow_lookup.x;
		vec2 loss_texc				=vec2(dist_rt,0.5*(1.f-sine));
#if 1
		volumeTexCoords				=vec3(atan2(lightspaceView.x,lightspaceView.y)/(2.0*pi),0.5*(1.0+2.0*asin(lightspaceView.z)/pi),dist_rt);
		vec4 insc					=texture_3d_wmc_lod(inscatterVolumeTexture,volumeTexCoords,0);
#else
		vec4 insc					=texture_3d_wwc_lod(inscatterVolumeTexture,volumeTexCoords,0);
#endif
		float hiResInterp			=saturate((dist - nearFarCloud.y) / max(dd,0.000001));
		// we're going to do TWO interpolations. One from zero to the near distance,
		// and one from the near to the far distance.
		float nearInterp			=saturate(dist / max(nearFarCloud.y,0.000001));
		cloud						=lerp(vec4(0,0,0,1.0),cloud,nearInterp);

		cloud						=lerp(cloudNear, cloud, hiResInterp);

		cloud						=lerp(vec4(0,0,0,1.0),cloud,nearInterp);
		float shadowInterp			=saturate((dist - nearFarCloud.w) / max(nearFarCloud.z-nearFarCloud.w,0.000001));
		//float hiResInterp			=saturate((dist - nearFarCloud.y) / max(dd,0.000001));
		shadow						=lerp(shadow_lookup.y,shadow_lookup.x,shadowInterp);

		insc.rgb					*=cloud.a;
		insc					+=cloud;
		shadow						*=cloud.a;
		res.multiply				+=texture_wrap_lod(loss2dTexture,loss_texc,0)*shadow;
		res.add						+=insc;
	}
	res.multiply/=float(numSamples);
	res.add/=float(numSamples);
    return res;
}
#endif
