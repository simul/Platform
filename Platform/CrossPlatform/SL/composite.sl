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

void ExtractCompactedLookupQuads(out LookupQuad4 farQ,out LookupQuad4 nearQ,TEXTURE2D_UINT4 image,vec2 texc,uint2 texDims)
{
	vec2 texc_unit	=texc*texDims-vec2(.5,.5);
	uint2 idx		=uint2(floor(texc_unit));
	uint i1			=max(0,idx.x);
	uint i2			=min(idx.x+1,texDims.x-1);
	uint j1			=max(0,idx.y);
	uint j2			=min(idx.y+1,texDims.y-1);
	int2 i11		=int2(i1,j1);
	int2 i21		=int2(i2,j1);
	int2 i12		=int2(i1,j2);
	int2 i22		=int2(i2,j2);
	uint4 v11		=IMAGE_LOAD(image,i11);
	uint4 v21		=IMAGE_LOAD(image,i21);
	uint4 v12		=IMAGE_LOAD(image,i12);
	uint4 v22		=IMAGE_LOAD(image,i22);
	farQ._11		=vec4(uint2_to_colour3(v11.xy),1.0);
	farQ._21		=vec4(uint2_to_colour3(v21.xy),1.0);
	farQ._12		=vec4(uint2_to_colour3(v12.xy),1.0);
	farQ._22		=vec4(uint2_to_colour3(v22.xy),1.0);
	nearQ._11		=vec4(uint2_to_colour3(v11.zw),1.0);
	nearQ._21		=vec4(uint2_to_colour3(v21.zw),1.0);
	nearQ._12		=vec4(uint2_to_colour3(v12.zw),1.0);
	nearQ._22		=vec4(uint2_to_colour3(v22.zw),1.0);
}

void GetNearFarLookupQuads(out LookupQuad4 farQ, out LookupQuad4 nearQ, Texture2D farImage,Texture2D nearImage, vec2 texc, int2 texDims)
{
	vec2 texc_unit = texc*texDims - vec2(.5, .5);
	int2 idx = int2(floor(texc_unit));
	int i1 = max(0, idx.x);
	int i2 = min(idx.x + 1, texDims.x - 2);
	int j1 = max(0, idx.y);
	int j2 = min(idx.y + 1, texDims.y - 2);
	int2 i11 = int2(i1, j1);
	int2 i21 = int2(i2, j1);
	int2 i12 = int2(i1, j2);
	int2 i22 = int2(i2, j2);

	farQ._11 =IMAGE_LOAD(farImage,i11);
	farQ._21 =IMAGE_LOAD(farImage,i21);
	farQ._12 =IMAGE_LOAD(farImage,i12);
	farQ._22 =IMAGE_LOAD(farImage,i22);
									 
	nearQ._11 =IMAGE_LOAD(nearImage,i11);
	nearQ._21 =IMAGE_LOAD(nearImage,i21);
	nearQ._12 =IMAGE_LOAD(nearImage,i12);
	nearQ._22 =IMAGE_LOAD(nearImage,i22);
}

LookupQuad4 GetLookupQuad(Texture2D image,vec2 texc,int2 texDims)
{
	vec2 texc_unit	=texc*texDims-vec2(.5,.5);
	int2 idx		=int2(floor(texc_unit));
	int i1			=max(0,idx.x);
	int i2			=min(idx.x+1,texDims.x-2);
	int j1			=max(0,idx.y);
	int j2			=min(idx.y+1,texDims.y-2);
	int2 i11		=int2(i1,j1);
	int2 i21		=int2(i2,j1);
	int2 i12		=int2(i1,j2);
	int2 i22		=int2(i2,j2);

	LookupQuad4 q;
	q._11=IMAGE_LOAD(image,i11);
	q._21=IMAGE_LOAD(image,i21);
	q._12=IMAGE_LOAD(image,i12);
	q._22=IMAGE_LOAD(image,i22);
	return q;
}

LookupQuad4 GetDepthLookupQuad(Texture2D image,vec2 texc,int2 texDims,vec4 depthToLinFadeDistParams)
{
	LookupQuad4 q	=GetLookupQuad(image,texc,texDims);
	q._11.xy		=depthToLinearDistance(q._11.xy,depthToLinFadeDistParams);
	q._21.xy		=depthToLinearDistance(q._21.xy,depthToLinFadeDistParams);
	q._12.xy		=depthToLinearDistance(q._12.xy,depthToLinFadeDistParams);
	q._22.xy		=depthToLinearDistance(q._22.xy,depthToLinFadeDistParams);
	return q;
}

LookupQuad4 MaskDepth(LookupQuad4 d1,vec2 depthMask)
	{
	LookupQuad4 d2=d1;
	d2._11.x		=dot(d1._11.xy,depthMask.xy);
	d2._21.x		=dot(d1._21.xy,depthMask.xy);
	d2._12.x		=dot(d1._12.xy,depthMask.xy);
	d2._22.x		=dot(d1._22.xy,depthMask.xy);
	return d2;
}

LookupQuad4 LerpZ(LookupQuad4 q,LookupQuad4 f,LookupQuad4 interp)
{
	q._11			=lerp(f._11,q._11,interp._11.z);
	q._21			=lerp(f._21,q._21,interp._21.z);
	q._12			=lerp(f._12,q._12,interp._12.z);
	q._22			=lerp(f._22,q._22,interp._22.z);
	return q;
}

vec4 depthFilteredTexture(	LookupQuad4 image
							,LookupQuad4 dist
							,vec2 xy
							,float d)
{
	float D1		=saturate((d-dist._11.x)/(dist._21.x-dist._11.x));
	float delta1	=abs(dist._21.x-dist._11.x);			
	vec4 _11		=lerp(image._11,image._21,delta1*D1);
	vec4 _21		=lerp(image._21,image._11,delta1*(1-D1));
	float D2		=saturate((d-dist._12.x)/(dist._22.x-dist._12.x));
	float delta2	=abs(dist._22.x-dist._12.x);			
	vec4 _12		=lerp(image._12,image._22,delta2*D2);
	vec4 _22		=lerp(image._22,image._12,delta2*(1-D2));

	vec4 f1			=lerp(_11,_21,xy.x);
	vec4 f2			=lerp(_12,_22,xy.x);
	
	float d1		=lerp(dist._11.x,dist._21.x,xy.x);
	float d2		=lerp(dist._12.x,dist._22.x,xy.x);
	
	float D			=saturate((d-d1)/(d2-d1));
	float delta		=abs(d2-d1);
#if 0
	f1				=lerp(f1,f2,delta*D);
	f2				=lerp(f2,f1,delta*(1.0-D));

	vec4 f			=lerp(f1,f2,xy.y);
#else
	vec4 F1			=lerp(f1,f2,delta*D);
	vec4 F2			=lerp(f2,f1,delta*(1.0-D));

	vec4 f			=lerp(F1,F2,xy.y);
#endif
	return f;
}

// Filter the texture, but bias towards the nearest depth values.
vec4 depthDependentFilteredImage(Texture2D imageTexture
								 ,Texture2D fallbackTexture
								 ,Texture2D depthTexture
								 ,int2 texDims
								 ,vec2 texc
								 ,vec2 depthMask
								 ,vec4 depthToLinFadeDistParams
								 ,float d)
{
#if 0
	return texture_clamp_lod(imageTexture,texc,0);
#else
	LookupQuad4 image		=GetLookupQuad(imageTexture		,texc,texDims);
	LookupQuad4 fallback	=GetLookupQuad(fallbackTexture	,texc,texDims);
	LookupQuad4 dist		=GetDepthLookupQuad(depthTexture,texc,texDims,depthToLinFadeDistParams);
	dist					=MaskDepth(dist,depthMask.xy);
	vec2 texc_unit			=texc*texDims-vec2(.5,.5);
	vec2 xy					=frac(texc_unit);
	return depthFilteredTexture(image
								,dist
								,xy
								,d);
#endif
}

vec3 nearFarDepthFilter(LookupQuad4 depth_Q,LookupQuad4 dist_Q,vec2 texDims,vec2 texc,vec2 dist)
{
	vec2 texc_unit	=texc*texDims-vec2(.5,.5);
	vec2 xy			=frac(texc_unit);
	// x = right, y = up, z = left, w = down
	vec3 f11		=depth_Q._11.xyz;
	vec3 f21		=depth_Q._21.xyz;
	vec3 f12		=depth_Q._12.xyz;
	vec3 f22		=depth_Q._22.xyz;

	vec2 d11		=dist_Q._11.xy;
	vec2 d21		=dist_Q._21.xy;
	vec2 d12		=dist_Q._12.xy;
	vec2 d22		=dist_Q._22.xy;

	// But now we modify these values:
	vec2 D1			=saturate((dist-d11)/(d21-d11));
	vec2 delta1		=abs(d21-d11);			
	vec2 D2			=saturate((dist-d12)/(d22-d12));
	vec2 delta2		=abs(d22-d12);
	
	vec2 d1			=lerp(d11,d21,xy.x);
	vec2 d2			=lerp(d12,d22,xy.x);
	
	vec2 D			=saturate((dist-d1)/(d2-d1));
	vec2 delta		=abs(d2-d1);
#if 1
	f11.xy				=lerp(f11.xy,f21.xy,delta1*D1);
	f21.xy				=lerp(f21.xy,f11.xy,delta1*(1-D1));
	f12.xy				=lerp(f12.xy,f22.xy,delta2*D2);
	f22.xy				=lerp(f22.xy,f12.xy,delta2*(1-D2));
#endif
	vec3 f1			=lerp(f11,f21,xy.x);
	vec3 f2			=lerp(f12,f22,xy.x);
#if 1
	f1.xy			=lerp(f1.xy,f2.xy,delta*D);
	f2.xy			=lerp(f2.xy,f1.xy,delta*(1.0-D));
#endif
	vec3 f			=lerp(f1,f2,xy.y);

	return f;
}

#define VOLUME_INSCATTER
#define SCREENSPACE_VOL
TwoColourCompositeOutput CompositeAtmospherics(vec2 texCoords
	, vec4 clip_pos
	, vec2 depth_texc
				,Texture2D farCloudTexture
				,Texture2D nearCloudTexture
				,Texture2D nearFarTexture
				,Texture2D loss2dTexture
				,Texture2D depthTexture
				//,int2 fullResDims
				,mat4 invViewProj
				,vec4 depthToLinFadeDistParams
				,vec2 lowResTexCoords
				,Texture3D screenSpaceInscVolumeTexture
				,Texture2D shadowTexture)
{
	TwoColourCompositeOutput res;
	//res.multiply=vec4(1,1,0.5,1);
	//res.add=vec4(texCoords.xy,0,1);
	// we only care about view.z, i.e. the third element of the vector.
	// so only dot-product the third row of invViewProj, with clip_pos.
#ifdef GLSL
	vec4 zrow					=vec4(invViewProj[2][0],invViewProj[3][1],invViewProj[3][2],invViewProj[3][3]);
#else
	vec4 zrow					=invViewProj._31_32_33_34;
#endif
	float sine					=dot(zrow,clip_pos);
	vec4 nearFarCloud			=texture_clamp_lod(nearFarTexture	,lowResTexCoords		,0);

	float depth					=texture_nearest_lod(depthTexture,depth_texc,0).x;
	//IMAGE_LOAD(depthTexture,fullres_depth_pos2).x;
	float dist					=depthToLinearDistance(depth	,depthToLinFadeDistParams);
	float dist_rt				=pow(dist,0.5);
	vec4 cloud					=texture_clamp_lod(farCloudTexture,lowResTexCoords,0);
	
	vec3 volumeTexCoords		= vec3(lowResTexCoords, dist_rt);
	vec4 shadow_lookup			= vec4(1.0, 1.0, 0, 0);
	//if (depth>0.0)
		shadow_lookup			=texture_clamp_lod(shadowTexture, lowResTexCoords, 0);
	float shadow				=shadow_lookup.x;
	vec2 loss_texc				=vec2(dist_rt,0.5*(1.f-sine));
	vec4 insc					=texture_clamp_lod(screenSpaceInscVolumeTexture,volumeTexCoords,0);
	float hiResInterp			=saturate((dist - nearFarCloud.y) / nearFarCloud.w);
	if(nearFarCloud.z>0.0)
	{
		vec4 cloudNear			=texture_clamp_lod(nearCloudTexture, lowResTexCoords, 0);
		cloud					=lerp(cloudNear, cloud, hiResInterp);
	}
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
													,Texture3D lossVolumeTexture
													,Texture2D loss2dTexture
													,int2 lowResDims
													,TEXTURE2DMS_FLOAT4 depthTextureMS
													,int numSamples
													,int2 fullResDims
													,mat4 invViewProj
													,vec4 viewportToTexRegionScaleBias
													,vec4 depthToLinFadeDistParams
													,vec4 fullResToLowResTransformXYWH
													,Texture3D screenSpaceInscVolumeTexture
													,Texture3D lightSpaceInscVolumeTexture
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
#if REVERSE_DEPTH==1
		float nearestDepth=0.0;
		float furthestDepth=1.0;
#else
		float nearestDepth=1.0;
		float furthestDepth=0.0;
#endif
		float depths[8];
		for(int k=0;k<numSamples;k++)
		{
			float d					=IMAGE_LOAD_MSAA(depthTextureMS,fullres_depth_pos2,k).x;
			depths[k]				=d;
#if REVERSE_DEPTH==1
			nearestDepth			=max(nearestDepth,d);
			furthestDepth			=min(furthestDepth,d);
#else
			nearestDepth			=min(nearestDepth,d);
			furthestDepth			=max(furthestDepth,d);
#endif
		}
		float nearestDist			=depthToLinearDistance(nearestDepth		,depthToLinFadeDistParams);
		float furthestDist			=depthToLinearDistance(furthestDepth	,depthToLinFadeDistParams);
		float distRange				=(abs(furthestDist-nearestDist));
		vec4 insc_far,insc_near,loss_far,loss_near;
		//LookupQuad4 lossFar_Q,lossNear_Q;
		//ExtractCompactedLookupQuads(lossFar_Q,lossNear_Q,lossTexture,lowResTexCoords,hiResDims);
		//LookupQuad4 inscDistFar_Q	=GetDepthLookupQuad(nearFarTexture	,lowResTexCoords,hiResDims,depthToLinFadeDistParams);
		//LookupQuad4 inscDistNear_Q	=MaskDepth(inscDistFar_Q,vec2(0,1.0));

		vec3 volumeTexCoordsFar		=vec3(lowResTexCoords,sqrt(furthestDist));
		insc_far					=texture_clamp_lod(screenSpaceInscVolumeTexture,volumeTexCoordsFar,0);
		vec3 volumeTexCoordsNear	=vec3(lowResTexCoords,sqrt(nearestDist));
		insc_near					=texture_clamp_lod(screenSpaceInscVolumeTexture,volumeTexCoordsNear,0);
		
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
#if REVERSE_DEPTH==1
			float u				=saturate(step(0.0,-hiresDepth));
#else
			float u				=saturate(step(1.0,hiresDepth));
#endif
			trueDist			=depthToLinearDistance(hiresDepth,depthToLinFadeDistParams);
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

#ifdef DEBUG_COMPOSITING
#endif
	}
	else
	{
#ifdef VOLUME_INSCATTER
		float d					=IMAGE_LOAD_MSAA(depthTextureMS,fullres_depth_pos2,0).x;
		float dist				=depthToLinearDistance(d		,depthToLinFadeDistParams);
#ifndef SCREENSPACE_VOL
		insc					=texture_wmc_lod(lightSpaceInscVolumeTexture,vec3(volume_texc.xy,sqrt(dist)),0);
#else
		vec3 volumeTexCoords	=vec3(lowResTexCoords,sqrt(dist));
		insc					=texture_clamp_lod(screenSpaceInscVolumeTexture,volumeTexCoords,0);
#endif
#else
		vec4 insc				= texture_clamp_lod(farInscatterTexture, lowResTexCoords, 0);
#endif
		insc					*=  cloud.a;
#if REVERSE_DEPTH==1
		insc					+=cloud;// *saturate(step(0.0, -low_res_depths.x));
#else
		insc					+=cloud;// *saturate(step(1.0, low_res_depths.x));
#endif
		res.add = insc;
		loss_texc.x = sqrt(dist);
		vec4 loss				=texture_clamp_lod(loss2dTexture,loss_texc,0);
		float shadow				=shadow_lookup.x;
		res.multiply			=loss *(cloud.a)*shadow;// *(1.0 - cloud.a);
	}
	res.add.a					= 1.0 - res.add.a;
#ifdef DEBUG_COMPOSITING
#endif
    return res;
}
#endif
