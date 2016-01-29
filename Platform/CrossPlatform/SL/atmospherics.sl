//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef ATMOSPHERICS_SL
#define ATMOSPHERICS_SL
#include "earth_shadow.sl"
#ifndef PI
#define PI (3.1415926536)
#endif

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

// Given a full-res, non-MS depth texture, and a half-res near far depth, 
void LossComposite(out vec3 farLoss,out vec3 nearLoss,Texture2D nearFarDepthTexture,vec4 viewportToTexRegionScaleBias,Texture2D lossTexture
	,mat4 invViewProj,vec2 texCoords,vec2 clip_pos,DepthIntepretationStruct depthInterpretationStruct,vec2 tanHalfFov)
{
	vec3 view		=mul(invViewProj,vec4(clip_pos.xy,1.0,1.0)).xyz;
	view			=normalize(view);
	vec2 depth_texc	=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	vec3 depth		=texture_clamp(nearFarDepthTexture,depth_texc).xyz;

	vec2 dist		=depthToFadeDistance(depth.xy,clip_pos.xy,depthInterpretationStruct,tanHalfFov);
	float sine		=view.z;
	float texy		=0.5*(1.f-sine);
	vec2 texx		=pow(dist,vec2(0.5,0.5));
	farLoss			=texture_clamp_mirror(lossTexture,vec2(texx.x,texy)).rgb;
	nearLoss		=texture_clamp_mirror(lossTexture,vec2(texx.y,texy)).rgb;
}

vec3 AtmosphericsLoss(Texture2D depthTexture,vec4 viewportToTexRegionScaleBias,Texture2D lossTexture
	,mat4 invViewProj,vec2 texCoords,vec2 clip_pos,DepthIntepretationStruct depthInterpretationStruct,vec2 tanHalfFov)
{
	vec3 view		=mul(invViewProj,vec4(clip_pos.xy,1.0,1.0)).xyz;
	view			=normalize(view);
	vec2 depth_texc	=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	float depth		=texture_clamp(depthTexture,depth_texc).x;
	//discardOnFar(depth);
	float dist		=depthToFadeDistance(depth,clip_pos.xy,depthInterpretationStruct,tanHalfFov);
	float sine		=view.z;
	vec2 texc2		=vec2(pow(dist,0.5),0.5*(1.f-sine));
	vec3 loss		=texture_clamp_mirror(lossTexture,texc2).rgb;
	return loss;
}

float GetCloudIllum(Texture3D cloudTexture, SamplerState cloudSamplerState,vec3 texc, vec3 lightDirCloudspace)
{
	vec3 l				=lightDirCloudspace;
	float a				=max(0.0,-texc.z);
	l					*=a/max(l.z,0.0001);
	texc				+=l;
	vec4 texel			=sample_3d_lod(cloudTexture,cloudSamplerState, texc, 0);
	//float above			=saturate(texc.z-1.0);
	//texel.y				+=above;
	return saturate(texel.x);
}

void Loss_All(		out vec4 colours[8]
					,Texture2D lossTexture
					,Texture3D cloudTexture
					,SamplerState cloudSamplerState
					,vec3 viewPosition
					,mat4 worldToCloudMatrix
					,vec2 texCoords
					,mat4 invViewProj
					,vec3 lightDir
					,float maxFadeDistanceMetres)
{
	//./v.ec2 depth_texc	=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	//vec3 depth		=texture_clamp(nearFarDepthTexture,depth_texc).xyz;

	vec2 clip_pos			=vec2(-1.0,1.0);
	clip_pos.x				+=2.0*texCoords.x;
	clip_pos.y				-=2.0*texCoords.y;
	vec3 view				=normalize(mul(invViewProj,vec4(clip_pos,1.0,1.0)).xyz);
	view					=normalize(view);
	float sine				=view.z;
	vec2 fade_texc			=vec2(0,0.5f*(1.f-sine));
	//vec2 nearFarDist		=depthToFadeDistance(depth.xy,clip_pos.xy,depthInterpretationStruct,tanHalfFov);
	////float il				=0.0;
	vec3 prev_loss			=vec3(1,1,1);
	vec3 lightDirCloudspace	=normalize(mul(worldToCloudMatrix,vec4(lightDir,0.0)).xyz);
	//vec3 viewCloudspace		=(mul(worldToCloudMatrix,vec4(view,0.0)).xyz);
	//vec3 viewposCloudspace	=mul(worldToCloudMatrix,vec4(viewPosition,1.0)).xyz;
	for(int i=0;i<8;i++)
	{
		fade_texc.x			=float(i)/7.0;
		float dist			=pow(fade_texc.x,2.0);
		float solid_dist	=dist;//nearFarDist.y;
		float distanceMetres=maxFadeDistanceMetres*solid_dist;
		vec3 wpos			=viewPosition+distanceMetres*view;
		vec3 texc			=mul(worldToCloudMatrix,vec4(wpos,1.0)).xyz;//viewposCloudspace+distanceMetres*viewCloudspace;
		float shadow1		=GetCloudIllum(cloudTexture,cloudSamplerState,texc,lightDirCloudspace);
		//float sharp_mul		=1.0;//+500.0*cloudShadowSharpness*(vec2(1.0,1.0)-texx);
		float shadow		=1.0-shadow1;//saturate(0.5+sharp_mul*(0.5-shadow1.x));

		shadow				=saturate(1.0-shadow);//*step(solid_dist,dist)
		vec3 loss			=shadow*texture_clamp_mirror(lossTexture,fade_texc).rgb;
		colours[i]			=vec4(loss,1.0);
		prev_loss			=loss;
	}
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
	insc			=texture_clamp_mirror_lod(inscTexture,fade_texc,0);
    skyl                =texture_clamp_mirror_lod(skylTexture,fade_texc,0).rgb;
}

vec4 Inscatter(	Texture2D inscTexture
						,Texture2D skylTexture
						,Texture2D depthTexture
						,Texture2D illuminationTexture
						,mat4 invViewProj
						,vec2 texCoords
						,vec3 lightDir
						,float hazeEccentricity
						,vec3 mieRayleighRatio
						,vec4 viewportToTexRegionScaleBias
						,DepthIntepretationStruct depthInterpretationStruct
						,vec2 tanHalfFov)
{
	vec2 clip_pos	=vec2(-1.0,1.0);
	clip_pos.x		+=2.0*texCoords.x;
	clip_pos.y		-=2.0*texCoords.y;
	vec3 view		=normalize(mul(invViewProj,vec4(clip_pos,1.0,1.0)).xyz);
	view			=normalize(view);

	vec2 depth_texc		=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	vec4 depth_lookup	=texture_clamp(depthTexture,depth_texc);
	float dist			=depthToFadeDistance(depth_lookup.x,clip_pos.xy,depthInterpretationStruct,tanHalfFov);
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
				,DepthIntepretationStruct depthInterpretationStruct
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
	float sine			=view.z;
	vec2 fade_texc	=vec2(0,0.5f*(1.f-sine));
	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	
	
	float dist			=depthToFadeDistance(depth,clip_pos.xy,depthInterpretationStruct,tanHalfFov);
	
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
	return vec4(colour.rgb,1.0);
}
	
struct FarNearOutput
{
	vec4 farColour SIMUL_RENDERTARGET_OUTPUT(0);
	vec4 nearColour SIMUL_RENDERTARGET_OUTPUT(1);
};

#endif
