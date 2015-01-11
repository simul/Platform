#ifndef ATMOSPHERICS_SL
#define ATMOSPHERICS_SL
#ifndef PI
#define PI (3.1415926536)
#endif
// Given a full-res, non-MS depth texture, and a half-res near far depth, 
void LossComposite(out vec3 farLoss,out vec3 nearLoss,Texture2D nearFarDepthTexture,vec4 viewportToTexRegionScaleBias,Texture2D lossTexture
	,mat4 invViewProj,vec2 texCoords,vec2 clip_pos,vec4 depthToLinFadeDistParams,vec2 tanHalfFov)
{
	float3 view		=mul(invViewProj,vec4(clip_pos.xy,1.0,1.0)).xyz;
	view			=normalize(view);
	vec2 depth_texc	=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	vec3 depth		=texture_clamp(nearFarDepthTexture,depth_texc).xyz;

	vec2 dist		=depthToFadeDistance(depth.xy,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
	float sine		=view.z;
	float texy		=0.5*(1.f-sine);
	vec2 texx		=pow(dist,0.5);
	farLoss			=texture_clamp_mirror(lossTexture,vec2(texx.x,texy)).rgb;
	nearLoss		=texture_clamp_mirror(lossTexture,vec2(texx.y,texy)).rgb;
	
}

void LossCompositeShadowed(out vec3 farLoss,out vec3 nearLoss,Texture2D nearFarDepthTexture,Texture2D cloudShadowTexture,vec4 viewportToTexRegionScaleBias,Texture2D lossTexture
	,mat4 invViewProj,vec2 texCoords,vec2 clip_pos,vec4 depthToLinFadeDistParams,float maxFadeDistanceMetres,vec2 tanHalfFov,mat4 worldspaceToShadowspaceMatrix,vec3 eyePos,float cloudShadowing
	,float cloudShadowSharpness)
{
	vec3 wOffset	=mul(invViewProj,vec4(clip_pos.xy,1.0,1.0)).xyz;
	vec3 view		=normalize(wOffset);
	vec2 depth_texc	=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	vec3 depth		=texture_clamp(nearFarDepthTexture,depth_texc).xyz;

	vec2 dist		=depthToFadeDistance(depth.xy,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
	vec4 shadow1	=GetSimpleIlluminationAt(cloudShadowTexture,worldspaceToShadowspaceMatrix,eyePos+dist.x*view*maxFadeDistanceMetres).x;
	vec4 shadow2	=GetSimpleIlluminationAt(cloudShadowTexture,worldspaceToShadowspaceMatrix,eyePos+dist.y*view*maxFadeDistanceMetres).x;
	float sine		=view.z;
	float texy		=0.5*(1.f-sine);
	vec2 texx		=pow(dist,0.5);
	// now shadow1 and shadow2 are from 0 (shadowed) to 1 (light).
	vec2 sharp_mul	=vec2(1.0,1.0);//+500.0*cloudShadowSharpness*(vec2(1.0,1.0)-texx);
	vec2 shadow		=saturate(vec2(0.5,0.5)+sharp_mul*vec2(0.5-shadow1.x,0.5-shadow2.x));
#ifdef REVERSE_DEPTH1
	shadow			*=cloudShadowing*(1.0-step(0.0,-depth.y));
#else
	shadow			*=cloudShadowing*(1.0-step(1.0,depth.y));
#endif
	shadow			=saturate(vec2(1.0,1.0)-shadow);
	farLoss			=shadow.x*texture_clamp_mirror(lossTexture,vec2(texx.x,texy)).rgb;
	nearLoss		=shadow.y*texture_clamp_mirror(lossTexture,vec2(texx.y,texy)).rgb;
}

vec3 AtmosphericsLoss(Texture2D depthTexture,vec4 viewportToTexRegionScaleBias,Texture2D lossTexture
	,mat4 invViewProj,vec2 texCoords,vec2 clip_pos,vec4 depthToLinFadeDistParams,vec2 tanHalfFov)
{
	float3 view		=mul(invViewProj,vec4(clip_pos.xy,1.0,1.0)).xyz;
	view			=normalize(view);
	vec2 depth_texc	=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	float depth		=texture_clamp(depthTexture,depth_texc).x;
	//discardOnFar(depth);
	float dist		=depthToFadeDistance(depth,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
	float sine		=view.z;
	vec2 texc2		=vec2(pow(dist,0.5),0.5*(1.f-sine));
	vec3 loss		=texture_clamp_mirror(lossTexture,texc2).rgb;
	return loss;
}

vec3 AtmosphericsLossMSAA(Texture2DMS<float4> depthTextureMS,uint i,vec4 viewportToTexRegionScaleBias,Texture2D lossTexture
	,mat4 invViewProj,vec2 texCoords,int2 depth_pos2,vec2 clip_pos,vec4 depthToLinFadeDistParams,vec2 tanHalfFov)
{
	float3 view	=mul(invViewProj,vec4(clip_pos.xy,1.0,1.0)).xyz;
	view		=normalize(view);
	float sine	=view.z;
	float depth	=depthTextureMS.Load(depth_pos2,i).x;
	float dist	=depthToFadeDistance(depth,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
	vec2 texc2	=vec2(pow(dist,0.5),0.5*(1.f-sine));
	vec3 loss	=texture_clamp_mirror(lossTexture,texc2).rgb;
	
	return loss;
}

void CalcInsc(	Texture2D inscTexture
				,Texture2D skylTexture
				,Texture2D illuminationTexture
				,float dist
				,float fade_texc_y
				,vec2 illum_texc
                ,out vec4 insc
                ,out vec3 skyl)
{
	vec2 fade_texc		=vec2(pow(dist,0.5f),fade_texc_y);
	vec4 illum_lookup	=texture_wrap_mirror_lod(illuminationTexture,illum_texc,0);
	vec2 nearFarTexc	=illum_lookup.xy;
	vec2 near_texc		=vec2(min(nearFarTexc.x,fade_texc.x),fade_texc.y);
	vec2 far_texc		=vec2(min(nearFarTexc.y,fade_texc.x),fade_texc.y);
	vec4 insc_near		=texture_clamp_mirror_lod(inscTexture,near_texc,0);
	vec4 insc_far		=texture_clamp_mirror_lod(inscTexture,far_texc,0);
	insc                =vec4(insc_far.rgb-insc_near.rgb,0.5*(insc_near.a+insc_far.a));
    skyl                =texture_clamp_mirror_lod(skylTexture,fade_texc,0).rgb;
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
				,fade_texc.y
				,illum_texc
				,insc
				,skyl);
#ifdef INFRARED
	vec3 colour			=skyl.rgb;
#else
	float cos0			=dot(view,lightDir);
	vec3 colour	    	=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour				+=skyl.rgb;
#endif
	return vec4(colour,1.0);
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
				,bool discardNear
				,bool nearPass)
{
	vec2 depth_texc		=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	vec4 depth_lookup	=texture_nearest_lod(depthTextureNF,depth_texc,0);
	float depth			=0.0;
	if(nearPass)
	{
		depth			=depth_lookup.y;
		if(depth_lookup.z==0)
		{
			if(discardNear)
			discard;
			return vec4(0,0,0,0);
		}
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
				,fade_texc.y
				,illum_texc
                ,insc
				,skyl);
	float cos0			=dot(view,lightDir);
#ifdef INFRARED
	vec3 colour			=skyl.rgb;
#else
	vec3 colour	    	=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour				+=skyl;
#endif
	return float4(colour.rgb,1.0);
}
	
struct FarNearOutput
{
	vec4 farColour SIMUL_RENDERTARGET_OUTPUT(0);
	vec4 nearColour SIMUL_RENDERTARGET_OUTPUT(1);
};

struct All8Output
{
	vec4 colour1 SIMUL_RENDERTARGET_OUTPUT(0);
	vec4 colour2 SIMUL_RENDERTARGET_OUTPUT(1);
	vec4 colour3 SIMUL_RENDERTARGET_OUTPUT(2);
	vec4 colour4 SIMUL_RENDERTARGET_OUTPUT(3);
	vec4 colour5 SIMUL_RENDERTARGET_OUTPUT(4);
	vec4 colour6 SIMUL_RENDERTARGET_OUTPUT(5);
	vec4 colour7 SIMUL_RENDERTARGET_OUTPUT(6);
	vec4 colour8 SIMUL_RENDERTARGET_OUTPUT(7);
};

// In depthTextureNF, x=far, y=near, z=edge
FarNearOutput Inscatter_Both(	Texture2D inscTexture
							,Texture2D skylTexture
							,Texture2D illuminationTexture
							,vec4 depth_lookup
							,vec2 texCoords
							,mat4 invViewProj
							,vec3 lightDir
							,float hazeEccentricity
							,vec3 mieRayleighRatio
							,vec4 viewportToTexRegionScaleBias
							,vec4 depthToLinFadeDistParams
							,vec2 tanHalfFov)
{
	vec2 depth			=depth_lookup.xy;
	vec2 clip_pos		=vec2(-1.0,1.0);
	clip_pos.x			+=2.0*texCoords.x;
	clip_pos.y			-=2.0*texCoords.y;
	vec3 view			=normalize(mul(invViewProj,vec4(clip_pos,1.0,1.0)).xyz);
	view				=normalize(view);
	vec2 offset[]		={	vec2(1.0,1.0),vec2(1.0,0.0),vec2(1.0,-1.0)
							,vec2(-1.0,1.0),vec2(-1.0,0.0),vec2(-1.0,-1.0)
							,vec2(0.0,1.0),vec2(0.0,-1.0),vec2(0.0,0.0)};
	float sine			=view.z;
	float2 fade_texc	=vec2(0,0.5f*(1.f-sine));
	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	
	
	vec2 dist			=depthToFadeDistance(depth.xy,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
	
	float cos0			=dot(view,lightDir);
	vec4 inscFar			=vec4(0,0,0,0);
	vec3 skylFar			=vec3(0,0,0);
	vec4 inscNear			=vec4(0,0,0,0);
	vec3 skylNear			=vec3(0,0,0);
	FarNearOutput fn;
    CalcInsc(	inscTexture
				,skylTexture
				,illuminationTexture
				,dist.x
				,fade_texc.y
				,illum_texc
                ,inscFar
				,skylFar);
#ifdef INFRARED
	vec3 colour				=skylFar.rgb;
	fn.farColour			=float4(colour,1.f);
	fn.nearColour			=float4(colour,1.f);
	return fn;
#else
	vec3 farColour	    =InscatterFunction(inscFar,hazeEccentricity,cos0,mieRayleighRatio);
	farColour			+=skylFar;
	fn.farColour		= vec4(farColour.rgb,1.0);
#endif
	if(depth_lookup.z!=0)
	{
		CalcInsc(	inscTexture
					,skylTexture
					,illuminationTexture
					,dist.y
					,fade_texc.y
					,illum_texc
					,inscNear
					,skylNear);
		vec3 nearColour	    =InscatterFunction(inscNear,hazeEccentricity,cos0,mieRayleighRatio);
		nearColour			+=skylNear;
		fn.nearColour		=vec4(nearColour.rgb,1.0);
	}
	else
		fn.nearColour		=fn.farColour;
	return fn;
}


// In depthTextureNF, x=far, y=near, z=edge
void Inscatter_All(out vec4 colours[8]
								,Texture2D inscTexture
								,Texture2D skylTexture
								,Texture2D illuminationTexture
								,Texture2D cloudShadowTexture
								,Texture3D cloudTexture
								,vec3 viewPosition
								,mat4 worldToShadowMatrix
								,vec4 depth_lookup
								,vec2 texCoords
								,mat4 invViewProj
								,vec3 lightDir
								,float hazeEccentricity
								,vec3 mieRayleighRatio
								,vec4 depthToLinFadeDistParams
								,float maxFadeDistanceMetres)
{
	vec2 clip_pos		=vec2(-1.0,1.0);
	clip_pos.x			+=2.0*texCoords.x;
	clip_pos.y			-=2.0*texCoords.y;
	vec3 view			=normalize(mul(invViewProj,vec4(clip_pos,1.0,1.0)).xyz);
	view				=normalize(view);
	float sine			=view.z;
	float2 fade_texc	=vec2(0,0.5f*(1.f-sine));
	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec4 illum_lookup	=texture_wrap_mirror(illuminationTexture,illum_texc);
	
	float cos0			=dot(view,lightDir);
	vec4 insc			=vec4(0,0,0,0);
	vec3 skyl			=vec3(0,0,0);
	
	float BetaRayleigh		=CalcRayleighBeta(cos0);
	float BetaMie			=HenyeyGreenstein(hazeEccentricity,cos0);
	for(int i=0;i<8;i++)
	{
		float dist			=pow(float(i)/7.0,2.0);
	float illum=0;
		for(int j=0;j<155;j++)
		{
			float distanceMetres=maxFadeDistanceMetres*pow((float(i)*float(j)/16.0)/7.0,2.0);
			vec3 worldPos		=viewPosition+distanceMetres*view;
			illum				+=GetCloudIlluminationAt( cloudTexture, worldToShadowMatrix,worldPos)/155.0;
		}

		fade_texc.x			=dist;
		vec2 nearFarTexc	=illum_lookup.xy;
		vec2 near_texc		=vec2(min(nearFarTexc.x,fade_texc.x),fade_texc.y);
		vec2 far_texc		=vec2(min(nearFarTexc.y,fade_texc.x),fade_texc.y);
		vec4 insc_near		=texture_clamp_mirror(inscTexture,near_texc);
		vec4 insc_far		=texture_clamp_mirror(inscTexture,far_texc);
		insc                =vec4(insc_far.rgb-insc_near.rgb,0.5*(insc_near.a+insc_far.a));
		skyl                =texture_clamp_mirror(skylTexture,fade_texc).rgb;
		
		vec3 colour	    =PrecalculatedInscatterFunction(insc,BetaRayleigh,BetaMie,mieRayleighRatio);
		colour			+=skyl;
		colours[i]		=vec4(colour,illum);
	}
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
						,fade_texc.y
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
					,fade_texc.y
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
#else
	vec3 colour	    	=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour				+=skyl;
#endif
	return float4(colour.rgb,1.0);
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
				,fade_texc.y
				,illum_texc
				,insc
				,skyl);
	float cos0			=dot(view,lightDir);
	float3 colour		=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour				+=skyl;
    return float4(colour.rgb,1.f);
}

// With this function we will create a 3D volume texture that encompasses the scattering for a frame,
// where the x-axis is azimuth around the light source, y-axis is angle from the light source (maximum = 180 deg)
// and z-axis is distance from the viewer.
void ScatteringVolume(	RWTexture3D<float4> targetVolume,int3 idx
						,Texture2D inscTexture
						,Texture2D skylTexture
						,Texture2D illuminationTexture
						,Texture2D cloudShadowTexture
						,mat4 worldspaceToShadowspaceMatrix
						,Texture3D cloudTexture
						,mat4 worldspaceToCloudspaceMatrix
						,vec3 eyePos
						,vec3 xAxis
						,vec3 yAxis
						,vec3 lightDir
						,int3 scatteringVolumeDims
						,float hazeEccentricity
						,vec3 mieRayleighRatio
						,float maxFadeDistanceMetres)
{
	// We must convert the idx values into a direction and distance in real space.
	float azimuth		=(float)idx.x/(float)scatteringVolumeDims.x*2.0*PI;
	float elevation		=(float)idx.y/(float)(scatteringVolumeDims.y-1)*PI;
	float se			=sin(elevation);
	float ce			=cos(elevation);
	float x				=sin(azimuth)*se;
	float y				=cos(azimuth)*se;
	vec3 dir			=x*xAxis+y*yAxis+lightDir*ce;

	float sine			=dir.z;
	float fade_texc_y	=0.5*(1.0-sine);
	vec2 illum_texc		=vec2(atan2(dir.x,dir.y)/(PI*2.0),fade_texc_y);
	vec4 colour			=vec4(0,0,0,0);
	vec4 last			=vec4(0,0,0,0);
	for(int i=0;i<scatteringVolumeDims.z;i++)
	{
		float dist			=pow((float)i/(float)(scatteringVolumeDims.z-1),2.0);
		vec4 insc;
		vec3 skyl;
		CalcInsc(	inscTexture
					,skylTexture
					,illuminationTexture
					,dist
					,fade_texc_y
					,illum_texc
					,insc
					,skyl);

	#ifdef INFRARED
		vec4 next		=vec4(skyl.rgb,1.0);
		float shadow	=1.0;
	#else
		float shadow	=GetCloudIlluminationAt(cloudTexture,worldspaceToCloudspaceMatrix,eyePos+dist*dir*maxFadeDistanceMetres).x;//worldspaceToShadowspaceMatrix
		float cos0		=ce;
		vec4 next	    =vec4(InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio),1.0);
		next.rgb		+=skyl.rgb;
	#endif
		colour			+=max(vec4(0,0,0,0),(next-last))*shadow;
		targetVolume[int3(idx.xy,i)]=colour;
		last			=next;
	}
}

#endif
