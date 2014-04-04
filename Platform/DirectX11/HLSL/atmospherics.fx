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

Texture2D rainbowLookupTexture;
Texture2D coronaLookupTexture;
Texture2D moistureTexture;

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


vec4 PS_Inscatter(atmosVertexOutput IN) : SV_TARGET
{
	int numSamples	=1;
	vec4 insc		=Inscatter(	 inscTexture
								,skylTexture
								,depthTexture
								,depthTextureMS
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
    return float4(insc.rgb*exposure,1.f);
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
vec4 PS_Inscatter_Far_NFDepth(atmosVertexOutput IN) : SV_TARGET
{
//	discard;
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
							,tanHalfFov,true,false);
	//vec4 res=vec4(1,0,0,0);
	res.rgb	*=exposure;
	return res;
}

vec4 PS_Inscatter_Near_NFDepth(atmosVertexOutput IN) : SV_TARGET
{
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

vec4 RainbowAndCorona(vec3 view,vec3 lightDir,vec2 texCoords)
{
	//return texture_clamp(coronaLookupTexture,IN.texCoords.xy);
	 //note: use a float for d here, since a half corrupts the corona
	float d=  -dot( lightDir,normalize(view ) 	);

	vec4 scattered	=texture_clamp(rainbowLookupTexture, vec2( dropletRadius, d));
	vec4 moisture	=1.0;//texture_clamp(moistureTexture,IN.texCoords);

	//(1 + d) will be clamped between 0 and 1 by the texture sampler
	// this gives up the dot product result in the range of [-1 to 0]
	// that is to say, an angle of 90 to 180 degrees
	vec4 coronaDiffracted = texture_clamp(coronaLookupTexture, vec2(dropletRadius, 1.0 + d));
	return (coronaDiffracted + scattered)*vec4(lightIrradiance/25.0,1.0)*rainbowIntensity*moisture.x;
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

	// NOTE: inefficient as we're calculating view in FastGodrays as well.
	vec3 view			=mul(invViewProj,vec4(IN.pos.xy,1.0,1.0)).xyz;
	res					+=saturate(res.a*2.0)*RainbowAndCorona(view,lightDir,IN.texCoords.xy);
	return vec4(res.rgb,1.0);
}

vec4 PS_NearGodrays(atmosVertexOutput IN) : SV_TARGET
{
	vec2 depth_texc		=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	vec4 depth_lookup	=depthTexture.Sample(clampSamplerState,depth_texc);
//	if(depth_lookup.z==0)
//		discard;
	float cloud_depth	=cloudDepthTexture.Sample(clampSamplerState,IN.texCoords.xy).x;
	float depth			=max(depth_lookup.y,cloud_depth);
	// Convert to true distance, in units of the fade distance (i.e. 1.0= at maximum fade):
	float solid_dist	=depthToFadeDistance(depth,IN.pos.xy,depthToLinFadeDistParams,tanHalfFov);
	vec4 res			=FastGodrays(cloudGodraysTexture,inscTexture,overcTexture,IN.pos,invViewProj,maxFadeDistanceMetres,solid_dist);
	
	vec3 view			=mul(invViewProj,vec4(IN.pos.xy,1.0,1.0)).xyz;
	res					+=saturate(res.a*2.0)*RainbowAndCorona(view,lightDir,IN.texCoords.xy);
	return vec4(res.rgb,1.0);
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

// An inscatter technique that expects an MSAA depth texture. Probably too inefficient for everyday use.
technique11 inscatter_msaa
{
    pass far
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Inscatter_Far_MSAA()));
    }
    pass near
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Inscatter_Near_MSAA()));
    }
}

// An inscatter technique that expects a depth texture containing near, far and edge components.
technique11 inscatter_nearfardepth
{
    pass far
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Inscatter_Far_NFDepth()));
    }
    pass near
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_5_0,PS_Inscatter_Near_NFDepth()));
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
BlendState MultiplyRGBABlend
{
	BlendEnable[0] = TRUE;
	SrcBlend = ZERO;
	DestBlend = SRC_COLOR;
    SrcBlendAlpha = ZERO;
    DestBlendAlpha = SRC_ALPHA;
};
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
		SetBlendState(AddBlendDontWriteAlpha, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_FastGodrays()));
    }
    pass near
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlendDontWriteAlpha, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_NearGodrays()));
    }
/*	pass rain_shadow
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(MoistureBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_RainShadowLoss()));
    }*/
}

technique11 near_depth_godrays
{
    pass godrays
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlendDontWriteAlpha, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_NearGodrays()));
    }
/*	pass rain_shadow
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(MoistureBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_NearRainShadow()));
    }*/
}
