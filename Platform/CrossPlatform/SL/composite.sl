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
				,bool do_clouds
				,bool do_height_fog=false
				,float fogExtinction=0.0
				,vec3 fogColour=vec3(0,0,0)
				,float fogHeightKm=0.0)
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
	if (do_height_fog)
	{
		// we seek two distances: the maximum distance that fog is seen, and the minimum.
		// We find these by tracing toward the eye from maxFadeDistanceKm to dist*maxFadeDistanceKm.
		// The maximum fog distance is the furthest 
		// distance from the eye to the fog "surface":
		float H_km = fogHeightKm;
		float d_solid	= maxFadeDistanceKm*dist;
		float z			= viewPos.z / 1000.0;
		float zs		= z + sine*d_solid;
		// z_max is the highest of the two heights, and z_min is the lowest.
		float z_max		= max(z,zs);
		float z_min		= min(z,zs);
		// z_1 and z_2 are the start and end heights of our integral.
		float z_1		= min(z_min,H_km);
		float z_2		= min(z_max,H_km);
		// The distance between the points of integration is:
		float s1		= (max(0,z_1) / abs(sine))/maxFadeDistanceKm;
		float s			= min(d_solid, (z_2 - z_1) / abs(sine));
		// Integral of p0 (H_km-z)/H
		float retain	= saturate(exp(-s * fogExtinction));// fogExtinction is 1/km.

		vec3 fogLoss	=texture_clamp_mirror_lod(loss2dTexture, vec2(loss_texc.x,s1), 0).rgb*cloud.a;
		insc.rgb		+= (1.0- retain)*fogColour*fogLoss;
#if 0
		// example, if viewPos.z-H>0 and sine>0, d_surf would be max.
		float km_above = max(0, z - H_km);
		float d_surf = min(km_above / max(-sine, 0.00001), maxFadeDistanceKm);
		float d_min = min(d_surf, d_solid);
		float d_max = d_solid;

		float zmin = viewPos.z / 1000.0;
		zmin = min(zmin, zmin + sine*d_solid);
		float dens = exp(-zmin);
		float sn = abs(sine);
		float thickness = max(0, H_km/sn*dens*(1.0-exp(-d_solid*sn/H_km)));// dens*(d_solid - d_solid*d_solid*sine / 2.0 / H_km));
		float T = 1.0;
		float retain = saturate(exp(-thickness / T));
	//	insc.rgb *= retain;
		insc.rgb +=  (1.0 - retain)*vec3(1, 1, 1)*res.multiply.rgb;
#endif
	}
	//if(do_clouds)
	//	res.multiply				*=cloud.a;
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
