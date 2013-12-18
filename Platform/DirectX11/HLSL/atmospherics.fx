#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/depth.sl"
#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "../../CrossPlatform/atmospherics.sl"
#include "../../CrossPlatform/atmospherics_constants.sl"

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

SamplerState samplerState: register(s1)
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

#include "../../CrossPlatform/cloud_shadow.sl"
#include "../../CrossPlatform/godrays.sl"

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
#ifdef REVERSE_DEPTH
	OUT.position.z	=0.0; 
#else
	OUT.position.z	=OUT.position.w; 
#endif
    OUT.texCoords	=0.5*(float2(1.0,1.0)+vec2(OUT.pos.x,-OUT.pos.y));
	OUT.texCoords	+=0.5*texelOffsets;
	return OUT;
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
    return float4(loss.rgb,1.f);
}

vec4 PS_Inscatter(atmosVertexOutput IN) : SV_TARGET
{
	vec2 clip_pos		=vec2(-1.f,1.f);
	clip_pos.x			+=2.0*IN.texCoords.x;
	clip_pos.y			-=2.0*IN.texCoords.y;
	vec3 insc			=AtmosphericsInsc(depthTexture
										,illuminationTexture
										,inscTexture
										,skylTexture
										,invViewProj
										,IN.texCoords
										,clip_pos.xy
										,viewportToTexRegionScaleBias
										,depthToLinFadeDistParams
										,tanHalfFov
										,hazeEccentricity
										,lightDir
										,mieRayleighRatio);
    return float4(insc.rgb*exposure,1.f);
}

vec4 PS_LossMSAA(atmosVertexOutput IN) : SV_TARGET
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
    return float4(loss.rgb,1.f);
}

vec4 PS_InscatterMSAA(atmosVertexOutput IN) : SV_TARGET
{
	vec2 clip_pos		=vec2(-1.f,1.f);
	clip_pos.x			+=2.0*IN.texCoords.x;
	clip_pos.y			-=2.0*IN.texCoords.y;
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
    return float4(loss.rgb,1.f);
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
    return float4(loss.rgb,1.f);
}

vec4 PS_Inscatter_Far(atmosVertexOutput IN) : SV_TARGET
{
	vec2 clip_pos		=vec2(-1.f,1.f);
	clip_pos.x			+=2.0*IN.texCoords.x;
	clip_pos.y			-=2.0*IN.texCoords.y;
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

vec4 PS_Inscatter_Near(atmosVertexOutput IN) : SV_TARGET
{
	vec2 clip_pos		=vec2(-1.f,1.f);
	clip_pos.x			+=2.0*IN.texCoords.x;
	clip_pos.y			-=2.0*IN.texCoords.y;
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
	
vec4 PS_FastGodrays(atmosVertexOutput IN) : SV_TARGET
{
	vec2 depth_texc		=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	vec4 depth_lookup	=depthTexture.Sample(clampSamplerState,depth_texc);
	float cloud_depth	=cloudDepthTexture.Sample(clampSamplerState,IN.texCoords.xy).x;
	float depth			=max(depth_lookup.x,cloud_depth);
	// Convert to true distance, in units of the fade distance (i.e. 1.0= at maximum fade):
	float solid_dist	=depthToFadeDistance(depth,IN.pos.xy,depthToLinFadeDistParams,tanHalfFov);
	vec4 res			=FastGodrays(cloudGodraysTexture,inscTexture,overcTexture,IN.pos,invViewProj,maxFadeDistanceMetres,solid_dist);
	return res;
}

vec4 PS_NearGodrays(atmosVertexOutput IN) : SV_TARGET
{
	vec2 depth_texc		=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	vec4 depth_lookup	=depthTexture.Sample(clampSamplerState,depth_texc);
	if(depth_lookup.z==0)
		discard;
	float cloud_depth	=cloudDepthTexture.Sample(clampSamplerState,IN.texCoords.xy).x;
	float depth			=max(depth_lookup.y,cloud_depth);
	// Convert to true distance, in units of the fade distance (i.e. 1.0= at maximum fade):
	float solid_dist	=depthToFadeDistance(depth,IN.pos.xy,depthToLinFadeDistParams,tanHalfFov);
	vec4 res			=FastGodrays(cloudGodraysTexture,inscTexture,overcTexture,IN.pos,invViewProj,maxFadeDistanceMetres,solid_dist);
	
	return res;
}

technique11 loss
{
    pass far
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(MultiplyBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Loss()));
    }
    pass near
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(MultiplyBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
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
		SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Inscatter()));
    }
    pass near
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
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
		SetBlendState(MultiplyBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Loss_Far()));
    }
    pass near
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(MultiplyBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Loss_Near()));
    }
}

technique11 inscatter_msaa
{
    pass far
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Inscatter_Far()));
    }
    pass near
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Inscatter_Near()));
    }
}


technique11 simul_atmospherics_overlay
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(MultiplyBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		//SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Loss()));
    }
    pass p1
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Inscatter()));
    }
}


technique11 simul_atmospherics_overlay_msaa
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(MultiplyBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		//SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_LossMSAA()));
    }
    pass p1
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_InscatterMSAA()));
    }
}

technique11 fast_godrays
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlendDontWriteAlpha, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_FastGodrays()));
    }
}

technique11 near_depth_godrays
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlendDontWriteAlpha, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_NearGodrays()));
    }
}