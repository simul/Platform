//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef ATMOSPHERICS_SL
#define ATMOSPHERICS_SL
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
	,mat4 invViewProj,vec2 texCoords,vec2 clip_pos,DepthInterpretationStruct depthInterpretationStruct,vec4 tanHalfFov)
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
	,mat4 invViewProj,vec2 texCoords,vec2 clip_pos,DepthInterpretationStruct depthInterpretationStruct,vec4 tanHalfFov)
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

float GetCloudIllum(Texture3D cloudTexture,SamplerState cloudSamplerState,vec3 texc,vec3 lightDirCloudspace,int clamp,float averageIllum)
{
	vec3 l			=lightDirCloudspace;
	// if light is shining downwards, and we are under the clouds (texc.z<0) we want to project upward towards the light.
	// if we are IN or above the clouds, we want to stay where we are.
	// Therefore, using max(0,-texc.z) will give zero for texc.z>0, and |texc.z| for texc.z<0.
	// dividing this by l.z will give us the projection distance up to the cloudbase, unless l.z<0
	//float zproject	=max(0.0,-texc.z)/max(l.z,0.0001)-max(0,texc.z-1.0)/min(l.z,-0.0001);
	float zproject	=max(0.0,-texc.z)/max(l.z,0.0001)*step(0,l.z)-max(0,texc.z-1.0)/min(l.z,-0.0001)*step(0,-l.z);
	texc			+=l*zproject;
	if(clamp!=0)
	{
		//
		float xproject	=max(0.0,-texc.x)/max(l.x,0.0001)*step(0,l.x)-max(0,texc.x-1.0)/min(l.x,-0.0001)*step(0,l.x);
		texc			+=l*xproject;
		//
		float yproject	=max(0.0,-texc.y)/max(l.y,0.0001)*step(0,l.y)-max(0,texc.y-1.0)/min(l.y,-0.0001)*step(0,l.y);
		texc			+=l*yproject;
	}
	float range=.05;
	// NOTE(NACHO): Changed to use the global state instead of passing it to the method (dealing with glsl)wrapSamplerState
	float light			=saturate((sample_3d_lod(cloudTexture,cloudSamplerState, texc, 0).x-1.0+range)/range);
	//float light			=saturate((sample_3d_lod(cloudTexture,wrapSamplerState, texc, 0).x-1.0+range)/range);
	//if(texc.z>0&&texc.z<10.0)
	//	light=0;
//	texel.x				=lerp(averageIllum,texel.x,exp(-abs(zproject/1000.0)));
	return saturate(light);
}


void CalcInsc(	Texture2D inscTexture
				,Texture2D skylTexture
				,float dist
				,float fade_texc_y
                ,out vec4 insc
                ,out vec3 skyl)
{
	vec2 fade_texc		=vec2(pow(dist,0.5f),fade_texc_y);
	insc				=texture_clamp_mirror_lod(inscTexture,fade_texc,0);
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
						,DepthInterpretationStruct depthInterpretationStruct
						,vec4 tanHalfFov)
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
				,dist
				,fade_texc.y
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
				,DepthInterpretationStruct depthInterpretationStruct
				,vec4 tanHalfFov
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
				,dist
				,fade_texc.y
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
