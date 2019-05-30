//  Copyright (c) 2015-2017 Simul Software Ltd. All rights reserved.
#ifndef COMPOSITE_SL
#define COMPOSITE_SL
#define DEBUG_COMPOSITING


#ifndef PI
#define PI (3.1415926536)
#endif


#define CLOUD_DEFS_ONLY
#include "simul_clouds.sl"

#ifdef SFX_OPENGL
struct TwoColourCompositeOutput
{
	vec4 add		SIMUL_RENDERTARGET_OUTPUT(0);
	vec4 multiply	SIMUL_RENDERTARGET_OUTPUT(0);
};
#else
struct TwoColourCompositeOutput
{
	vec4 add		SIMUL_RENDERTARGET_OUTPUT(0);
	vec4 multiply	SIMUL_RENDERTARGET_OUTPUT(1);
};
#endif

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
				,vec3 fogColour
				,vec3 fogAmbient
				,bool do_height_fog
				,float fogExtinction
				,float fogHeightKm)
{
	TwoColourCompositeOutput res;
	vec3 view						=normalize(mul(invViewProj,clip_pos).xyz);
	float sine						=view.z;
	vec4 nearFarCloud				=vec4(1.0,0.0,1,1);
	if(do_interp)
		nearFarCloud				=texture_cube_lod(nearFarTexture	,view		,0);

	float dist_rt					=sqrt(dist);
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
	vec4 lp=vec4(0.0,0.0,0.0,0.0);
	if(do_lightpass)
		lp							=texture_cube_lod(lightpassTexture,view,0);
	
	vec4 cloudNear					=cloudCubeArray.Sample(cubeSamplerState,vec4(view,cloudLevel_0));
	vec4 cloud;
	if(do_interp)
	{
		vec4 cloudFar				=cloudCubeArray.Sample(cubeSamplerState,vec4(view,cloudLevel_0+1.0));
		cloud						=lerp(cloudNear, cloudFar,frac(cloudLevel));
		if(do_lightpass)
		{
			cloud.rgb				+=lp.rgb;
		}
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
	if(do_godrays)
	{
		vec3 offsetKm					=view*dist*maxFadeDistanceKm;
		vec3 lightspaceOffset			=mul(worldToScatteringVolumeMatrix,vec4(offsetKm,1.0)).xyz;
		float r							=length(lightspaceOffset);
		vec3 lightspaceVolumeTexCoords	=vec3(frac(atan2(lightspaceOffset.x,lightspaceOffset.y)/(2.0*SIMUL_PI_F))
													,0.5+0.5*asin(lightspaceOffset.z/r)*2.0/SIMUL_PI_F
													,r);
		vec4 godrays					=texture_3d_wcc_lod(godraysVolumeTexture,lightspaceVolumeTexCoords,0);
		insc.rgb						*=godrays.x;
	}
	if(do_clouds)
		insc.rgb						*=cloud.a;
	if(do_clouds)
		insc							+=cloud;
	res.multiply						=texture_clamp_mirror_lod(loss2dTexture, loss_texc, 0)*cloud.a;
	if (do_height_fog)
	{
		// we seek two distances: the maximum distance that fog is seen, and the minimum.
		// We find these by tracing toward the eye from maxFadeDistanceKm to dist*maxFadeDistanceKm.
		// The maximum fog distance is the furthest 
		// distance from the eye to the fog "surface":
		float H_km		= fogHeightKm;
		float d_solid	= maxFadeDistanceKm*dist;
		float z			= viewPos.z / 1000.0;
		float transition=pow(saturate(H_km-z),0.5);
		float zs		= z + sine*d_solid;
		// z_max is the highest of the two heights, and z_min is the lowest.
		float z_max		= max(z,zs);
		float z_min		= min(z,zs);
		// z_1 and z_2 are the start and end heights of our integral.
		float z_1		= min(z_min,H_km);
		float z_2		= min(z_max,H_km);
		float sn		=max(0.0000000001,abs(sine));
		float sn1		=max(0.0000000001,(-sine));
		// The distance between the points of integration is:
		float s1		=min(dist,saturate((max(0,(z-z_2) / sn1))/maxFadeDistanceKm));
		float above_fog	=step(0,z-H_km);
		float upwards	=step(0,sine);
		float dz		=z_2-z_1;
		float s			=(1.0-above_fog*upwards)*min(maxFadeDistanceKm,min(d_solid,dz/ sn));
		// The mean height is:
		float z_mean	=0.5*(z_1+z_2);
		s				*=saturate((H_km-z_mean)/0.1);
		// Integral of p0 (H_km-z)/H
		float fog_in	=1.0-saturate(exp(-s * fogExtinction));// fogExtinction is 1/km.

		vec3 fogLoss	=vec3(1.0,1.0,1.0);

		// retain is the amount of visibility for whatever's behind the fog.
		// It gives us the fog inscatter also.
		//retain=lerp(1.0,retain,transition);
		//if(z<H_km)
			float rt_s1		=sqrt(s1);
			//insc.rgb		=texture_3d_wmc_lod(inscatterVolumeTexture,vec3(worldspaceVolumeTexCoords.xy,rt_s1),0);
		{
			fogLoss *=fog_in*lerp((1-(above_fog*(1-cloud.a)))*texture_clamp_mirror_lod(loss2dTexture, vec2(rt_s1,loss_texc.y), 0).rgb,vec3(1,1,1),transition);
		}
		insc.rgb		*=vec3(1.0,1.0,1.0)-fogLoss;///lerp(retain,1.0,saturate(z-H_km));
		insc.rgb		+=(fogColour+fogAmbient)*fogLoss;
		res.multiply	*=(1.0-fog_in);
	}
	//if(do_clouds)
	//	res.multiply				*=cloud.a;
	res.add							=insc;

    return res;
}

#endif
