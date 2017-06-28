//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef COMPOSITE_SL
#define COMPOSITE_SL
#define DEBUG_COMPOSITING


#ifndef PI
#define PI (3.1415926536)
#endif

#define CLOUD_DEFS_ONLY
#include "simul_clouds.sl"
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


// NOTE: Performance here is ALL about texture bandwidth. If the textures can be made to use smaller formats: 16-bit floats,
// R11G11B10 etc, the shader will go much faster.
TwoColourCompositeOutput CompositeAtmospherics(vec4 clip_pos
				,TextureCubeArray cloudCubeArray
				,TextureCube nearFarTexture
				,TextureCube lightpassTexture
				,Texture2D loss2dTexture
				,float dist
				,mat4 invViewProj
				,vec3 viewPos
				,Texture3D inscatterVolumeTexture
				,Texture3D godraysVolumeTexture
				,float maxFadeDistanceKm
				,float nearDist
				,bool do_lightpass
				,bool do_godrays
				,bool do_interp
				,bool do_near
				,bool do_clouds=true)
{
	TwoColourCompositeOutput res;
	vec3 view						=normalize(mul(invViewProj,clip_pos).xyz);
	float sine						=view.z;
	vec4 nearFarCloud				=vec4(1.0,0.0,0,0);
	if(do_interp)
		nearFarCloud					=texture_cube_lod(nearFarTexture	,view		,0);
	
	float dist_rt					=sqrt(dist);
	vec3 offsetMetres				=view*dist*1000.0*maxFadeDistanceKm;
	vec3 lightspaceOffset			=(mul(worldToScatteringVolumeMatrix,vec4(offsetMetres,1.0)).xyz);
	vec3 worldspaceVolumeTexCoords	=vec3(atan2(view.x,view.y)/(2.0*SIMUL_PI_F),0.5*(1.0+2.0*asin(sine)/SIMUL_PI_F),dist_rt);

	// cut-off at the edges.
	vec4 insc						=texture_3d_wmc_lod(inscatterVolumeTexture,worldspaceVolumeTexCoords,0);
	vec2 loss_texc					=vec2(dist_rt,0.5*(1.f-sine));

	float f							=nearFarCloud.x;
	float n							=nearFarCloud.y;
	float hiResInterp				=1.0-saturate(( f- dist) / max(0.000000001,f-n));
	// This is the interp from the near to the far clouds.
	float cloudLevel				=float(NUM_CLOUD_INTERP-1)*hiResInterp;	// i.e. 0,1,2 levels of the array.
	float cloudLevel_0				=floor(cloudLevel);
	vec4 lp;
	if(do_lightpass)
		lp							=texture_cube_lod(lightpassTexture,view,0);
	
	vec4 cloudNear					=cloudCubeArray.Sample(cubeSamplerState,vec4(view,cloudLevel_0));
	vec4 cloud;
	if(do_interp)
	{
		vec4 cloudFar				=cloudCubeArray.Sample(cubeSamplerState,vec4(view,cloudLevel_0+1.0));
		if(do_lightpass)
			cloudFar.rgb			+=lp.rgb;
		cloud						=lerp(cloudNear, cloudFar,frac(cloudLevel));
		if(do_near)
		{
			float nearInterp		=saturate((dist-nearDist)/ max(0.00000001, 2.0*nearDist));
			cloud					=lerp(vec4(0, 0, 0, 1.0), cloud, nearInterp);
		}
	}
	else
	{
		cloud						=cloudNear;
	}
	if(do_clouds)
		insc.rgb						*=cloud.a;
	if(do_godrays)
	{
		float r							=length(lightspaceOffset);
		vec3 lightspaceVolumeTexCoords	=vec3(frac(atan2(lightspaceOffset.x,lightspaceOffset.y)/(2.0*SIMUL_PI_F))
													,0.5+0.5*asin(lightspaceOffset.z/r)*2.0/SIMUL_PI_F
													,r);
		vec4 godrays					=texture_3d_wcc_lod(godraysVolumeTexture,lightspaceVolumeTexCoords,0);
		insc.rgb						*=godrays.rgb;
	}
	if(do_clouds)
		insc							+=cloud;
	res.multiply					=texture_clamp_mirror_lod(loss2dTexture, loss_texc, 0)*cloud.a;
	if(do_clouds)
		res.multiply				*=cloud.a;
	res.add							=insc;
    return res;
}

TwoColourCompositeOutput CompositeAtmospherics_MSAA(vec4 clip_pos
													,vec2 depth_texc
													,TextureCubeArray cloudCubeArray
													,TextureCube nearFarTexture
													,TextureCube lightpassTexture
													,Texture2D loss2dTexture
													,TEXTURE2DMS_FLOAT4 depthTextureMS
													,int numSamples
													,uint2 fullResDims
													,mat4 invViewProj
													,vec4 viewportToTexRegionScaleBias
													,DepthInterpretationStruct depthInterpretationStruct
													,vec2 lowResTexCoords
													,Texture3D inscatterVolumeTexture
													,Texture3D godraysVolumeTexture
													,float maxFadeDistanceKm
													,float nearDist
													,bool do_lightpass
													,bool do_godrays
													,bool do_interp
													,bool do_near)
{
	TwoColourCompositeOutput res;
	vec3 view						=normalize(mul(invViewProj,clip_pos).xyz);
	float sine						=view.z;
	vec4 nearFarCloud				=texture_cube_lod(nearFarTexture	,view		,0);
	
	int2 fullres_depth_pos2		=int2(depth_texc*vec2(fullResDims.xy));
	res.add						=vec4(0,0,0,1.0);
	res.multiply				=vec4(0,0,0,0);
	for(int k=0;k<numSamples;k++)
	{
		float depth				=TEXTURE_LOAD_MSAA(depthTextureMS,fullres_depth_pos2,k).x;

		float dist				=depthToLinearDistance(depth	,depthInterpretationStruct);
		float dist_rt					=sqrt(dist);
		vec3 offsetMetres				=view*dist*1000.0*maxFadeDistanceKm;
		vec3 lightspaceOffset			=(mul(worldToScatteringVolumeMatrix,vec4(offsetMetres,1.0)).xyz);
		vec3 worldspaceVolumeTexCoords	=vec3(atan2(view.x,view.y)/(2.0*SIMUL_PI_F),0.5*(1.0+2.0*asin(sine)/SIMUL_PI_F),dist_rt);
	
		// cut-off at the edges.
		vec4 insc						=texture_3d_wmc_lod(inscatterVolumeTexture,worldspaceVolumeTexCoords,0);
		vec2 loss_texc					=vec2(dist_rt,0.5*(1.f-sine));
	
		float f							=nearFarCloud.x;
		float n							=nearFarCloud.y;
		float hiResInterp				=1.0-saturate(( f- dist) / max(0.000000001,f-n));
		// This is the interp from the near to the far clouds.
		float cloudLevel				=float(NUM_CLOUD_INTERP-1)*hiResInterp;	// i.e. 0,1,2 levels of the array.
		float cloudLevel_0				=floor(cloudLevel);
		vec4 lp;
		if(do_lightpass)
			lp							=texture_cube_lod(lightpassTexture,view,0);
		
		vec4 cloudNear					=cloudCubeArray.Sample(cubeSamplerState,vec4(view,cloudLevel_0));
		vec4 cloud;
		if(do_interp)
		{
			vec4 cloudFar				=cloudCubeArray.Sample(cubeSamplerState,vec4(view,cloudLevel_0+1.0));
			if(do_lightpass)
				cloudFar.rgb			+=lp.rgb;
			cloud						=lerp(cloudNear, cloudFar,frac(cloudLevel));
			if(do_near)
			{
				float nearInterp		=saturate((dist-nearDist)/ max(0.00000001, 2.0*nearDist));
				cloud					=lerp(vec4(0, 0, 0, 1.0), cloud, nearInterp);
			}
		}
		else
		{
			cloud						=cloudNear;
		}
		
		insc.rgb						*=cloud.a;
	
		insc							+=cloud;
		if(do_godrays)
		{
			float r							=length(lightspaceOffset);
			vec3 lightspaceVolumeTexCoords	=vec3(frac(atan2(lightspaceOffset.x,lightspaceOffset.y)/(2.0*SIMUL_PI_F))
														,0.5+0.5*asin(lightspaceOffset.z/r)*2.0/SIMUL_PI_F
														,r);
			vec4 godrays					=texture_3d_wcc_lod(godraysVolumeTexture,lightspaceVolumeTexCoords,0);
			insc.rgb						*=godrays.rgb;
		}
		res.multiply					+=texture_clamp_mirror_lod(loss2dTexture, loss_texc, 0)*cloud.a;
		res.add							+=insc;
	}
	res.multiply/=float(numSamples);
	res.add/=float(numSamples);
    return res;
}
#endif
