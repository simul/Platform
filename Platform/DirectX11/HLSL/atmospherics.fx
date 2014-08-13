#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/SL/depth.sl"
#include "../../CrossPlatform/SL/simul_inscatter_fns.sl"
#include "../../CrossPlatform/SL/cloud_shadow.sl"
#include "../../CrossPlatform/SL/atmospherics.sl"
#include "../../CrossPlatform/SL/atmospherics_constants.sl"
#include "../../CrossPlatform/SL/colour_packing.sl"

Texture2D depthTexture;
Texture2DMS<float4> depthTextureMS;
Texture2D cloudDepthTexture;
Texture2D imageTexture;
Texture2D lossTexture;
Texture2D inscTexture;
Texture2D overcTexture;
Texture2D skylTexture;
Texture2D illuminationTexture;
Texture2D cloudShadowTexture;
Texture2D cloudNearFarTexture;
Texture2D cloudGodraysTexture;

Texture2D moistureTexture;

SamplerState samplerState: register(s1)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

#include "../../CrossPlatform/SL/cloud_shadow.sl"
#include "../../CrossPlatform/SL/godrays.sl"

#define pi (3.1415926536)

struct atmosVertexInput
{
	uint vertex_id			: SV_VertexID;
};

struct atmosVertexOutput
{
    float4 position			: SV_POSITION;
    float2 texCoords		: TEXCOORD0;
    float2 pos				: TEXCOORD1;
};

atmosVertexOutput VS_Atmos(atmosVertexInput IN)
{
	atmosVertexOutput OUT;
	float2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	OUT.pos			=poss[IN.vertex_id];
	OUT.position	=float4(OUT.pos,0.0,1.0);
	// Set to far plane so can use depth test as want this geometry effectively at infinity
#if REVERSE_DEPTH==1
	OUT.position.z	=0.0; 
#else
	OUT.position.z	=OUT.position.w; 
#endif
    OUT.texCoords	=0.5*(float2(1.0,1.0)+vec2(OUT.pos.x,-OUT.pos.y));
	OUT.texCoords	+=0.5*texelOffsets;
	return OUT;
}

uint4 PS_LossComposite(atmosVertexOutput IN) : SV_TARGET
{
	vec2 depth_texc	=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	vec3 farLoss,nearLoss;
	LossComposite(farLoss,nearLoss,depthTexture
					,viewportToTexRegionScaleBias
					,lossTexture
					,invViewProj
					,IN.texCoords
					,IN.pos
					,depthToLinFadeDistParams
					,tanHalfFov);
	//uint faru		=colour3_to_uint(farLoss);
	//uint nearu	=colour3_to_uint(nearLoss);
	uint2 faru		=colour3_to_uint2(farLoss);
	uint2 nearu		=colour3_to_uint2(nearLoss);
    return uint4(faru,nearu);
}

uint4 PS_LossCompositeShadowed(atmosVertexOutput IN) : SV_TARGET
{
	vec2 depth_texc	=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	vec3 farLoss,nearLoss;
	LossCompositeShadowed(farLoss,nearLoss,depthTexture,cloudShadowTexture
						,viewportToTexRegionScaleBias
						,lossTexture
						,invViewProj
						,IN.texCoords
						,IN.pos
						,depthToLinFadeDistParams
						,tanHalfFov
						,invShadowMatrix,viewPosition,cloudShadowing);
	uint2 faru		=colour3_to_uint2(farLoss);
	uint2 nearu		=colour3_to_uint2(nearLoss);
    return uint4(faru,nearu);
}

vec4 PS_Loss(atmosVertexOutput IN) : SV_TARGET
{
	vec2 depth_texc	=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	vec3 loss		=AtmosphericsLoss(depthTexture
									,viewportToTexRegionScaleBias
									,lossTexture
									,invViewProj
									,IN.texCoords
									,IN.pos
									,depthToLinFadeDistParams
									,tanHalfFov);
    return float4(loss.rgb,1.0);
}

vec4 PS_LossMSAA(atmosVertexOutput IN,uint sampleIndex:SV_SampleIndex) : SV_TARGET
{
	vec2 depth_texc	=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	int2 pos2;
	int numSamples;
	GetMSAACoordinates(depthTextureMS,depth_texc,pos2,numSamples);
	vec3 loss		=AtmosphericsLossMSAA(depthTextureMS
											,sampleIndex
											,viewportToTexRegionScaleBias
											,lossTexture
											,invViewProj
											,IN.texCoords
											,pos2
											,IN.pos
											,depthToLinFadeDistParams
											,tanHalfFov);
    return float4(loss.rgb,1.0);
}


vec4 PS_Inscatter(atmosVertexOutput IN) : SV_TARGET
{
	int numSamples	=1;
	vec4 insc		=Inscatter(	 inscTexture
								,skylTexture
								,depthTexture
								,numSamples
								,illuminationTexture
								,invViewProj
								,IN.texCoords
								,lightDir
								,hazeEccentricity
								,mieRayleighRatio
								,viewportToTexRegionScaleBias
								,depthToLinFadeDistParams
								,tanHalfFov
								,true
								,false);
    return float4(insc.rgb*exposure,1.0);
}

vec4 PS_InscatterMSAA(atmosVertexOutput IN) : SV_TARGET
{
	uint2 dims;
	int numSamples;
	depthTextureMS.GetDimensions(dims.x,dims.y,numSamples);
	int2 pos2=int2(IN.texCoords*vec2(dims.xy));
	vec4 res=InscatterMSAA(	inscTexture
							,skylTexture
							,illuminationTexture
							,depthTextureMS
							,numSamples
							,IN.texCoords
							,pos2
							,invViewProj
							,lightDir
							,hazeEccentricity
							,mieRayleighRatio
							,viewportToTexRegionScaleBias
							,depthToLinFadeDistParams
							,tanHalfFov
							,false
							,false);
	res.rgb	*=exposure;
	return res;
}


vec4 PS_Loss_Far(atmosVertexOutput IN) : SV_TARGET
{
	vec2 depth_texc	=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	int2 pos2;
	int numSamples;
	GetMSAACoordinates(depthTextureMS,depth_texc,pos2,numSamples);
	vec3 loss			=AtmosphericsLossMSAA(depthTextureMS
											,numSamples
											,viewportToTexRegionScaleBias
											,lossTexture
											,invViewProj
											,IN.texCoords
											,pos2
											,IN.pos
											,depthToLinFadeDistParams
											,tanHalfFov);
    return float4(loss.rgb,1.0);
}

vec4 PS_Loss_Near(atmosVertexOutput IN) : SV_TARGET
{
	vec2 depth_texc	=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	int2 pos2;
	int numSamples;
	GetMSAACoordinates(depthTextureMS,depth_texc,pos2,numSamples);
	vec3 loss		=AtmosphericsLossMSAA(depthTextureMS
											,numSamples
											,viewportToTexRegionScaleBias
											,lossTexture
											,invViewProj
											,IN.texCoords
											,pos2
											,IN.pos
											,depthToLinFadeDistParams
							,tanHalfFov);
    return float4(loss.rgb,1.0);
}

vec4 PS_Inscatter_Far_MSAA(atmosVertexOutput IN) : SV_TARGET
{
	uint2 dims;
	int numSamples;
	depthTextureMS.GetDimensions(dims.x,dims.y,numSamples);
	int2 pos2=int2(IN.texCoords*vec2(dims.xy));
	vec4 res=InscatterMSAA(	inscTexture
							,skylTexture
							,illuminationTexture
							,depthTextureMS
							,numSamples
							,IN.texCoords
							,pos2
							,invViewProj
							,lightDir
							,hazeEccentricity
							,mieRayleighRatio
							,viewportToTexRegionScaleBias
							,depthToLinFadeDistParams
							,tanHalfFov,true,false);
	res.rgb	*=exposure;
	return res;
}

vec4 PS_Inscatter_Near_MSAA(atmosVertexOutput IN) : SV_TARGET
{
	uint2 dims;
	int numSamples;
	depthTextureMS.GetDimensions(dims.x,dims.y,numSamples);
	int2 pos2=int2(IN.texCoords*vec2(dims.xy));
	vec4 res=InscatterMSAA(	inscTexture
							,skylTexture
							,illuminationTexture
							,depthTextureMS
							,numSamples
							,IN.texCoords
							,pos2
							,invViewProj
							,lightDir
							,hazeEccentricity
							,mieRayleighRatio
							,viewportToTexRegionScaleBias
							,depthToLinFadeDistParams
							,tanHalfFov,true,true);
	res.rgb	*=exposure;
	return res;
}
vec4 PS_Inscatter_Far_NFDepth(atmosVertexOutput IN) : SV_TARGET0
{
	vec4 res	=Inscatter_NFDepth(	inscTexture
									,skylTexture
									,illuminationTexture
									,depthTexture
									,IN.texCoords
									,invViewProj
									,lightDir
									,hazeEccentricity
									,mieRayleighRatio
									,viewportToTexRegionScaleBias
									,depthToLinFadeDistParams
									,tanHalfFov,true,false);
	res.rgb	*=exposure;
	return res;
}

vec4 PS_Inscatter_Near_NFDepth(atmosVertexOutput IN) : SV_TARGET0
{
	return vec4(1,0,1,1);
	vec4 res=Inscatter_NFDepth(	inscTexture
							,skylTexture
							,illuminationTexture
							,depthTexture
							,IN.texCoords
							,invViewProj
							,lightDir
							,hazeEccentricity
							,mieRayleighRatio
							,viewportToTexRegionScaleBias
							,depthToLinFadeDistParams
							,tanHalfFov,true,true);
	res.rgb	*=exposure;
	return res;
}

vec4 PS_Inscatter_Near_NFDepth1(atmosVertexOutput IN) : SV_TARGET1
{
	return vec4(0,1,0,1);
	vec4 res=Inscatter_NFDepth(	inscTexture
							,skylTexture
							,illuminationTexture
							,depthTexture
							,IN.texCoords
							,invViewProj
							,lightDir
							,hazeEccentricity
							,mieRayleighRatio
							,viewportToTexRegionScaleBias
							,depthToLinFadeDistParams
							,tanHalfFov,false,true);
	res.rgb	*=exposure;
	return res;
}

FarNearOutput PS_Inscatter_Both(atmosVertexOutput IN)
{
	vec2 depth_texc		=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	vec4 depth_lookup	=texture_nearest_lod(depthTexture,depth_texc,0);
	vec2 texCoords		=mixedResolutionTransformXYWH.xy+IN.texCoords.xy*mixedResolutionTransformXYWH.zw;
#if 0
	FarNearOutput fn;
	fn.farColour=vec4(texCoords,0,1);
	fn.nearColour=vec4(0,texCoords,1);
#else
	FarNearOutput fn	=Inscatter_Both(inscTexture
										,skylTexture
										,illuminationTexture
										,depth_lookup
										,texCoords
										,invViewProj
										,lightDir
										,hazeEccentricity
										,mieRayleighRatio
										,viewportToTexRegionScaleBias
										,depthToLinFadeDistParams
										,tanHalfFov);
#endif
	fn.farColour.rgb	*=exposure;
	fn.nearColour.rgb	*=exposure;
	return fn;
}
vec4 PS_FastGodrays(atmosVertexOutput IN) : SV_TARGET
{
	vec2 depth_texc		=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	vec4 depth_lookup	=depthTexture.Sample(clampSamplerState,depth_texc);
	float cloud_depth	=cloudDepthTexture.Sample(clampSamplerState,IN.texCoords.xy).x;
#if REVERSE_DEPTH==1
	float depth			=max(depth_lookup.x,cloud_depth);
#else
	float depth			=min(depth_lookup.x,cloud_depth);
#endif
	// Convert to true distance, in units of the fade distance (i.e. 1.0= at maximum fade):
	float solid_dist	=depthToFadeDistance(depth,IN.pos.xy,depthToLinFadeDistParams,tanHalfFov);
	vec4 res			=FastGodrays(cloudGodraysTexture,inscTexture,overcTexture,IN.pos,invViewProj,maxFadeDistanceMetres,solid_dist);

	// NOTE: inefficient as we're calculating view in FastGodrays as well.
	vec3 view			=mul(invViewProj,vec4(IN.pos.xy,1.0,1.0)).xyz;
	//res					+=saturate(res.a*2.0)*RainbowAndCorona(rainbowLookupTexture,coronaLookupTexture,dropletRadius,rainbowIntensity,lightIrradiance,view,lightDir,IN.texCoords.xy);
	return vec4(res.rgb,1.0);
}

vec4 PS_NearGodrays(atmosVertexOutput IN) : SV_TARGET
{
	vec2 depth_texc		=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	vec4 depth_lookup	=depthTexture.Sample(clampSamplerState,depth_texc);
	if(depth_lookup.z==0)
		discard;
	float cloud_depth	=cloudDepthTexture.Sample(clampSamplerState,IN.texCoords.xy).x;
#if REVERSE_DEPTH==1
	float depth			=max(depth_lookup.y,cloud_depth);
#else
	float depth			=min(depth_lookup.y,cloud_depth);
#endif
	// Convert to true distance, in units of the fade distance (i.e. 1.0= at maximum fade):
	float solid_dist	=depthToFadeDistance(depth,IN.pos.xy,depthToLinFadeDistParams,tanHalfFov);
	vec4 res			=FastGodrays(cloudGodraysTexture,inscTexture,overcTexture,IN.pos,invViewProj,maxFadeDistanceMetres,solid_dist);
	
	vec3 view			=mul(invViewProj,vec4(IN.pos.xy,1.0,1.0)).xyz;
	//res					+=saturate(res.a*2.0)*RainbowAndCorona(rainbowLookupTexture,coronaLookupTexture,dropletRadius,  rainbowIntensity,lightIrradiance,view,lightDir,IN.texCoords.xy);
	return vec4(res.rgb,1.0);
}

FarNearOutput PS_BothGodrays(atmosVertexOutput IN) : SV_TARGET
{
	FarNearOutput fn;
	vec2 depth_texc		=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	vec4 depth_lookup	=depthTexture.Sample(clampSamplerState,depth_texc);
	float cloud_depth	=cloudDepthTexture.Sample(clampSamplerState,IN.texCoords.xy).x;
	depth_lookup.y		=lerp(depth_lookup.x,depth_lookup.y,depth_lookup.z);
#if REVERSE_DEPTH==1
	vec2 depth			=max(depth_lookup.xy,cloud_depth);
#else
	vec2 depth			=min(depth_lookup.xy,cloud_depth);
#endif
	// Convert to true distance, in units of the fade distance (i.e. 1.0= at maximum fade):
	vec2 solid_dist		=depthToFadeDistance(depth.xy,IN.pos.xy,depthToLinFadeDistParams,tanHalfFov);
	fn.farColour		=FastGodrays(cloudGodraysTexture,inscTexture,overcTexture,IN.pos,invViewProj,maxFadeDistanceMetres,solid_dist.x);
	fn.nearColour		=FastGodrays(cloudGodraysTexture,inscTexture,overcTexture,IN.pos,invViewProj,maxFadeDistanceMetres,solid_dist.y);
	
	vec3 view			=mul(invViewProj,vec4(IN.pos.xy,1.0,1.0)).xyz;
	/*vec4 rc				=RainbowAndCorona(rainbowLookupTexture,coronaLookupTexture,dropletRadius,
										  rainbowIntensity,lightIrradiance,view,lightDir,IN.texCoords.xy);
	fn.farColour		+=saturate(fn.farColour.a*2.0)*rc;
	fn.nearColour		+=saturate(fn.nearColour.a*2.0)*rc;*/
	return fn;
}
vec4 PS_RainShadowLoss(atmosVertexOutput IN) : SV_TARGET
{
	vec2 depth_texc		=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	vec4 depth_lookup	=depthTexture.Sample(clampSamplerState,depth_texc);
	float cloud_depth	=cloudDepthTexture.Sample(clampSamplerState,IN.texCoords.xy).x;
	float depth			=max(depth_lookup.x,cloud_depth);
	// Convert to true distance, in units of the fade distance (i.e. 1.0= at maximum fade):
	float solid_dist	=depthToFadeDistance(depth,IN.pos.xy,depthToLinFadeDistParams,tanHalfFov);
	vec4 res			=RainShadowLoss(moistureTexture,IN.pos,invViewProj,viewPosition,worldToMoistureSpaceMatrix,maxFadeDistanceMetres,solid_dist);
	
	return res;
}

vec4 PS_NearRainShadow(atmosVertexOutput IN) : SV_TARGET
{
	vec2 depth_texc		=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	vec4 depth_lookup	=depthTexture.Sample(clampSamplerState,depth_texc);
	//.if(depth_lookup.z==0)
	//	discard;
	float cloud_depth	=cloudDepthTexture.Sample(clampSamplerState,IN.texCoords.xy).x;
	float depth			=max(depth_lookup.y,cloud_depth);
	// Convert to true distance, in units of the fade distance (i.e. 1.0= at maximum fade):
	float solid_dist	=depthToFadeDistance(depth,IN.pos.xy,depthToLinFadeDistParams,tanHalfFov);
	vec4 res			=RainShadowLoss(moistureTexture,IN.pos,invViewProj,viewPosition,worldToMoistureSpaceMatrix,maxFadeDistanceMetres,solid_dist);
	
	return res;
}
technique11 loss_composite
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_LossComposite()));
    }
}
technique11 loss_composite_shadowed
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_LossCompositeShadowed()));
    }
}
technique11 loss
{
    pass far
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(MultiplyBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Loss()));
    }
    pass near
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(MultiplyBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Loss()));
    }
}

technique11 inscatter
{
    pass far
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Inscatter()));
    }
    pass near
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Inscatter()));
    }
}

technique11 loss_msaa
{
    pass far
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(MultiplyBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Loss_Far()));
    }
    pass near
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(MultiplyBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Loss_Near()));
    }
}

// An inscatter technique that expects an MSAA depth texture. Probably too inefficient for everyday use.
technique11 inscatter_msaa
{
    pass far
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Inscatter_Far_MSAA()));
    }
    pass near
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Inscatter_Near_MSAA()));
    }
}
technique11 inscatter_far_near_2rts
{
	pass both
	{
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Inscatter_Both()));
	}
    pass far
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(DontBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Inscatter_Far_NFDepth()));
    }
    pass near1
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(DontBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Inscatter_Near_NFDepth1()));
    }
}
// An inscatter technique that expects a depth texture containing near, far and edge components.
technique11 inscatter_nearfardepth
{
    pass far
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Inscatter_Far_NFDepth()));
    }
    pass near
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Inscatter_Near_NFDepth()));
    }
}

fxgroup atmospherics_overlay
{
	technique11 standard
	{
		pass loss
		{
			SetRasterizerState( RenderNoCull );
			SetDepthStencilState( DisableDepth, 0 );
			SetBlendState(MultiplyBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
			//SetBlendState(AddBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
			SetGeometryShader(NULL);
			SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
			SetPixelShader(CompileShader(ps_5_0,PS_Loss()));
		}
		pass inscatter
		{
			SetRasterizerState( RenderNoCull );
			SetDepthStencilState( DisableDepth, 0 );
			SetBlendState(AddBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
			SetGeometryShader(NULL);
			SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
			SetPixelShader(CompileShader(ps_5_0,PS_Inscatter()));
		}
	}
	technique11 msaa
	{
		pass loss
		{
			SetRasterizerState( RenderNoCull );
			SetDepthStencilState( DisableDepth, 0 );
			SetBlendState(MultiplyBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
			//SetBlendState(AddBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
			SetGeometryShader(NULL);
			SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
			SetPixelShader(CompileShader(ps_5_0,PS_LossMSAA()));
		}
		pass inscatter
		{
			SetRasterizerState( RenderNoCull );
			SetDepthStencilState( DisableDepth, 0 );
			SetBlendState(AddBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
			SetGeometryShader(NULL);
			SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
			SetPixelShader(CompileShader(ps_5_0,PS_InscatterMSAA()));
		}
	}
}
BlendState MoistureBlend
{
	BlendEnable[0] = TRUE;
	SrcBlend = ONE;
	DestBlend = SRC_ALPHA;
    BlendOp = ADD;
    SrcBlendAlpha = ZERO;
    DestBlendAlpha = SRC_ALPHA;
    BlendOpAlpha = ADD;
    //RenderTargetWriteMask[0] = 0x0F;
};

technique11 fast_godrays
{
    pass far
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlendDontWriteAlpha, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_FastGodrays()));
    }
    pass near
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlendDontWriteAlpha, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_NearGodrays()));
    }
	pass both
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlendDontWriteAlpha, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_BothGodrays()));
    }
}

technique11 near_depth_godrays
{
    pass godrays
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlendDontWriteAlpha, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_NearGodrays()));
    }
/*	pass rain_shadow
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(MoistureBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_NearRainShadow()));
    }*/
}
