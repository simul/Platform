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
				,Texture3D screenSpaceInscVolumeTexture
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
	vec4 nearFarCloud			=texture_clamp_lod(nearFarTexture	,lowResTexCoords		,0);
	// Should NOT be necessary:
	nearFarCloud.w	=(nearFarCloud.x-nearFarCloud.y);
	float depth					=texture_nearest_lod(depthTexture,depth_texc,0).x;
	//IMAGE_LOAD(depthTexture,fullres_depth_pos2).x;
	float dist					=depthToLinearDistance(depth	,depthInterpretationStruct);
	float dist_rt				=pow(dist,0.5);
	vec4 cloud					=texture_clamp_lod(farCloudTexture,lowResTexCoords,0);
	
	vec3 volumeTexCoords		= vec3(lowResTexCoords, dist_rt);
	vec4 shadow_lookup			= vec4(1.0, 1.0, 0, 0);
	//if (depth>0.0)
		shadow_lookup			=texture_clamp_lod(shadowTexture, lowResTexCoords, 0);
	float shadow				=shadow_lookup.x;
	vec2 loss_texc				=vec2(dist_rt,0.5*(1.f-sine));
	vec4 insc					=texture_3d_clamp_lod(screenSpaceInscVolumeTexture,volumeTexCoords,0);
	float hiResInterp			=saturate((dist - nearFarCloud.y) / max(nearFarCloud.w,0.000001));
	// we're going to do TWO interpolations. One from zero to the near distance,
	// and one from the near to the far distance.
	float nearInterp			=saturate(dist / max(nearFarCloud.y,0.000001));
	
	vec4 cloudNear			=texture_clamp_lod(nearCloudTexture, lowResTexCoords, 0);
	cloud					=lerp(cloudNear, cloud, hiResInterp);
	
	cloud					=lerp(vec4(0,0,0,1.0),cloud,nearInterp);
	shadow					=lerp(shadow_lookup.y,shadow_lookup.x,hiResInterp);
	insc.rgb				*=cloud.a;
	insc					+=cloud;
	shadow					*=cloud.a;
	res.multiply			=texture_clamp_lod(loss2dTexture,loss_texc,0)*shadow;
	res.add					=insc;

    return res;
}

TwoColourCompositeOutput CompositeAtmospherics_MSAA(vec2 texCoords
													,Texture2D cloudTexture
													,Texture2D nearCloudTexture
													,Texture2D nearFarTexture
													,Texture2D loss2dTexture
													,TEXTURE2DMS_FLOAT4 depthTextureMS
													,int numSamples
													,uint2 fullResDims
													,mat4 invViewProj
													,vec4 viewportToTexRegionScaleBias
													,DepthIntepretationStruct depthInterpretationStruct
													,vec4 fullResToLowResTransformXYWH
													,Texture3D screenSpaceInscVolumeTexture
													,Texture2D shadowTexture)
{
	vec4 shadow_lookup			=texture_clamp(shadowTexture,texCoords);
	vec4 clip_pos				=vec4(-1.0,1.0,1.0,1.0);
	clip_pos.x					+=2.0*texCoords.x;
	clip_pos.y					-=2.0*texCoords.y;
	vec3 view					=mul(invViewProj,vec4(clip_pos.xy,1.0,1.0)).xyz;
	float sine					=view.z;
	// texCoords.y is positive DOWN-wards
	vec2 depth_texc				=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	int2 fullres_depth_pos2		=int2(depth_texc*vec2(fullResDims.xy));
	
	vec2 lowResTexCoords		=fullResToLowResTransformXYWH.xy	+texCoords*fullResToLowResTransformXYWH.zw;
	
	vec4 cloud					=texture_clamp_lod(cloudTexture		,lowResTexCoords	,0);
	vec4 low_res_depths			=texture_clamp_lod(nearFarTexture	,lowResTexCoords		,0);
	//cloud.a						=1.0-cloud.a;

	vec4 cloudNear				=texture_clamp_lod(nearCloudTexture, lowResTexCoords, 0);
	
	float hires_edge			=low_res_depths.z;
	TwoColourCompositeOutput res;
	res.add						=vec4(0,0,0,1.0);
	res.multiply				=vec4(0,0,0,0);
	vec4 insc					=vec4(0,0,0,0);
#ifndef SCREENSPACE_VOL
	// To obtain the inscatter value: transform the clip position into a position in the scattering volume's space.
	vec4 clip_pos				=vec4(-1.0,1.0,1.0,1.0);
	clip_pos.x					+=2.0*texCoords.x;
	clip_pos.y					-=2.0*texCoords.y;
	vec3 view_sc				=mul(clipPosToScatteringVolumeMatrix,clip_pos).xyz;
	view_sc						/=length(view_sc);
	float azimuth				=atan2(view_sc.x,view_sc.y);
	float elevation				=acos(view_sc.z);
	vec3 volume_texc			=vec3(azimuth/(PI*2.0),elevation/PI,0.0);
#endif
	vec2 loss_texc				=vec2(0,0.5*(1.f-sine));
	if(hires_edge>0.0)
	{
		float nearestDepth;
		float furthestDepth;
		if(depthInterpretationStruct.reverseDepth)
		{
			nearestDepth=0.0;
			furthestDepth=1.0;
		}
		else
		{
			nearestDepth=1.0;
			furthestDepth=0.0;
		}
		float depths[8];
		for(int k=0;k<numSamples;k++)
		{
			float d					=TEXTURE_LOAD_MSAA(depthTextureMS,fullres_depth_pos2,k).x;
			depths[k]				=d;
			if(depthInterpretationStruct.reverseDepth)
			{
				nearestDepth			=max(nearestDepth,d);
				furthestDepth			=min(furthestDepth,d);
			}
			else
			{
				nearestDepth			=min(nearestDepth,d);
				furthestDepth			=max(furthestDepth,d);
			}
		}
		float nearestDist			=depthToLinearDistance(nearestDepth		,depthInterpretationStruct);
		float furthestDist			=depthToLinearDistance(furthestDepth	,depthInterpretationStruct);
		float distRange				=(abs(furthestDist-nearestDist));
		vec4 insc_far,insc_near,loss_far,loss_near;

		vec3 volumeTexCoordsFar		=vec3(lowResTexCoords,sqrt(furthestDist));
		insc_far					=texture_3d_clamp_lod(screenSpaceInscVolumeTexture,volumeTexCoordsFar,0);
		vec3 volumeTexCoordsNear	=vec3(lowResTexCoords,sqrt(nearestDist));
		insc_near					=texture_3d_clamp_lod(screenSpaceInscVolumeTexture,volumeTexCoordsNear,0);
		
		loss_far					=texture_clamp_lod(loss2dTexture,vec2(volumeTexCoordsFar.z,loss_texc.y),0)*shadow_lookup.x*cloud.a;
		loss_near					=texture_clamp_lod(loss2dTexture,vec2(volumeTexCoordsNear.z,loss_texc.y),0)*shadow_lookup.y*cloudNear.a;


		vec3 nearFarDist;
		nearFarDist.xy = low_res_depths.yx;// depthToLinearDistance(low_res_depths.yx, depthToLinFadeDistParams);
		nearFarDist.z	=(nearFarDist.y-nearFarDist.x);
		// Given that we have the near and far depths, 
		// At an edge we will do the interpolation for each MSAA sample.
		//float um=0.0;
		vec3 inscm=vec3(0,0,0);
		float trueDist=0.0;
		for(int j=0;j<numSamples;j++)
		{
			float hiresDepth	=depths[j];
			trueDist			=depthToLinearDistance(hiresDepth,depthInterpretationStruct);
			float hiResInterp	=saturate((nearFarDist.y-trueDist)/nearFarDist.z);

#ifdef VOLUME_INSCATTER
			float inscInterp	=saturate((trueDist-nearestDist)/distRange);
			insc				=lerp(insc_near,insc_far,inscInterp);
#else

			insc				=lerp(insc_far,insc_near,hiResInterp);
#endif
			vec4 cl				=lerp(cloud,cloudNear,hiResInterp);
			insc				*=(cl.a);
			insc				+=cl;
			loss_texc.x=sqrt(trueDist);
			vec4 loss			= lerp(loss_far,loss_near,hiResInterp);
			//um				+=u;
			res.multiply.rgb	+=loss.rgb;
			inscm				+=insc.rgb;
		}
		//um					/=float(numSamples);
		
		res.add					/=float(numSamples);
		res.multiply			/=float(numSamples);
		inscm					/=float(numSamples);
		res.add.rgb			+= inscm.rgb;
	}
	else
	{
#ifdef VOLUME_INSCATTER
		float d					=TEXTURE_LOAD_MSAA(depthTextureMS,fullres_depth_pos2,0).x;
		float dist				=depthToLinearDistance(d		,depthInterpretationStruct);
#ifndef SCREENSPACE_VOL
		insc					=texture_wmc_lod(lightSpaceInscVolumeTexture,vec3(volume_texc.xy,sqrt(dist)),0);
#else
		vec3 volumeTexCoords	=vec3(lowResTexCoords,sqrt(dist));
		insc					=texture_3d_clamp_lod(screenSpaceInscVolumeTexture,volumeTexCoords,0);
#endif
#else
		vec4 insc				= texture_clamp_lod(farInscatterTexture, lowResTexCoords, 0);
#endif
		insc					*=  cloud.a;

		insc					+=cloud;
		res.add = insc;
		loss_texc.x = sqrt(dist);
		vec4 loss				=texture_clamp_lod(loss2dTexture,loss_texc,0);
		float shadow				=shadow_lookup.x;
		res.multiply			=loss *(cloud.a)*shadow;// *(1.0 - cloud.a);
	}
	res.add.a					= 1.0 - res.add.a;
    return res;
}
#endif
