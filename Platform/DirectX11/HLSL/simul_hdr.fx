#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/depth.sl"
#include "../../CrossPlatform/hdr_constants.sl"
#include "../../CrossPlatform/mixed_resolution.sl"
Texture2D imageTexture;
Texture2DMS<float4> imageTextureMS;
Texture2D nearImageTexture;
Texture2D depthTexture;
Texture2DMS<float4> depthTextureMS;
Texture2D lowResDepthTexture;
Texture2D<uint> glowTexture;

Texture2D inscatterTexture;			// Far, or default inscatter
Texture2D nearInscatterTexture;		// Near inscatter.



struct a2v
{
	uint vertex_id	: SV_VertexID;
    float4 position	: POSITION;
    float2 texcoord	: TEXCOORD0;
};

struct v2f
{
    float4 hPosition	: SV_POSITION;
    float2 texCoords	: TEXCOORD0;
};

v2f MainVS(idOnly IN)
{
	v2f OUT;
	vec2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	vec2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=vec4(pos,0.0,1.0);
	// Set to far plane so can use depth test as we want this geometry effectively at infinity
#ifdef REVERSE_DEPTH
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=0.5*(vec2(1.0,1.0)+vec2(pos.x,-pos.y));
    return OUT;
}

v2f OffsetVS(idOnly IN)
{
	v2f OUT;
	vec2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	vec2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=vec4(pos/*+offset*/,0.0,1.0);
	// Set to far plane so can use depth test as we want this geometry effectively at infinity
#ifdef REVERSE_DEPTH
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=0.5*(vec2(1.0,1.0)+vec2(pos.x,-pos.y));
    return OUT;
}

v2f QuadVS(idOnly IN)
{
    v2f OUT;
	float2 poss[4]=
	{
		{ 1.0, 0.0},
		{ 1.0, 1.0},
		{ 0.0, 0.0},
		{ 0.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=vec4(pos,0.0,1.0);
	OUT.hPosition	=float4(vec2(2.0*rect.x-1.0,1.0-2.0*(rect.y+rect.w))+2.0*rect.zw*pos,0.0,1.0);
    OUT.texCoords	=vec2(pos.x,1.0-pos.y);
    return OUT;
}

vec4 ShowDepthPS(v2f IN) : SV_TARGET
{
	vec4 depth		=texture_clamp(depthTexture,IN.texCoords);
	float dist		=10.0*depthToFadeDistance(depth.x,2.0*(IN.texCoords-0.5),depthToLinFadeDistParams,tanHalfFov);
    return vec4(1,dist,dist,1.0);
}

vec4 convertInt(Texture2D<uint> glowTexture,vec2 texCoord)
{
	uint2 tex_dim;
	glowTexture.GetDimensions(tex_dim.x, tex_dim.y);

	uint2 location = uint2((uint)(tex_dim.x * texCoord.x), (uint)(tex_dim.y * texCoord.y));
	uint int_color = glowTexture[location];

	// Convert R11G11B10 to float3
	vec4 color;
	color.r = (float)(int_color >> 21) / 2047.0f;
	color.g = (float)((int_color >> 10) & 0x7ff) / 2047.0f;
	color.b = (float)(int_color & 0x0003ff) / 1023.0f;
	color.a = 1;

	return color;
}

/*
 The Filmic tone mapper is from: http://filmicgames.com/Downloads/GDC_2010/Uncharted2-Hdr-Lighting.pptx
These numbers DO NOT have the pow(x,1/2.2) baked in
*/
vec3 FilmicTone(vec3 x)
{
	float A				=0.22;	//Shoulder Strength
	float B				=0.30;	//Linear Strength
	float C				=0.10;	//Linear Angle
	float D				=0.20;	//Toe Strength
	float E				=0.01;	//Toe Numerator
	float F				=0.30;	//Toe Denominator
	//Note: E/F = Toe Angle
	float LinearWhite	=1.2;	//Linear White Point Value
	vec3 Fx				= ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F)) - E/F;
	vec3 Fw				= ((LinearWhite*(A*LinearWhite+C*B)+D*E)/(LinearWhite*(A*LinearWhite+B)+D*F)) - E/F;
	vec3 c				= Fx/Fw;
	return c;
}

vec4 GlowExposureGammaPS(v2f IN) : SV_TARGET
{
	vec4 c=texture_nearest_lod(imageTexture,IN.texCoords,0);
	vec4 glow=convertInt(glowTexture,IN.texCoords);
	c.rgb+=glow.rgb;
	c.rgb*=exposure;
	c.rgb=pow(c.rgb,gamma);
    return vec4(c.rgb,1.f);
}

vec4 ExposureGammaPS(v2f IN) : SV_TARGET
{
	vec4 c=texture_nearest_lod(imageTexture,IN.texCoords,0);
	c.rgb*=exposure;
	c.rgb=pow(c.rgb,gamma);
    return vec4(c.rgb,1.f);
}

// Scales input texture coordinates for distortion.
vec2 HmdWarp(vec2 texCoords)
{
	vec2 theta		= (texCoords - warpLensCentre) * warpScaleIn; // Scales to [-1, 1]
	float rSq		= theta.x*theta.x+theta.y*theta.y;
	vec2 rvector	= theta * (warpHmdWarpParam.x + warpHmdWarpParam.y * rSq +
								warpHmdWarpParam.z * rSq * rSq +
								warpHmdWarpParam.w * rSq * rSq * rSq);
	return warpLensCentre+rvector* warpScale ;
}

float4 WarpExposureGammaPS(v2f IN) : SV_Target
{
	vec2 tc = HmdWarp(IN.texCoords);
	//if(any(clamp(tc,warpScreenCentre-vec2(0.25,0.5),warpScreenCentre+vec2(0.25, 0.5))-tc))
	//	return vec4(0,0,0,0);
	vec4 c=texture_clamp(imageTexture,tc);
	c.rgb*=exposure;
	c.rgb=pow(c.rgb,gamma);
    return vec4(c.rgb,1.0);
}

vec4 DirectPS(v2f IN) : SV_TARGET
{
	vec4 c=exposure*texture_clamp(imageTexture,IN.texCoords);
    return vec4(c.rgba);
}

void UpdateNearestSample(	inout float MinDist,
							inout vec2 NearestUV,
							float Z,
							vec2 UV,
							float ZFull
							)
{
	float Dist = abs(Z - ZFull);
	if (Dist < MinDist)
	{
		MinDist = Dist;
		NearestUV = UV;
	}
}

bool IsSampleNearer(inout float MinDist,float Z,float ZFull)
{
	float Dist = abs(Z - ZFull);
	return (Dist < MinDist);
}

// texture_clamp_lod texture_nearest_lod
vec4 NearFarDepthCloudBlendPS(v2f IN) : SV_TARGET
{
	vec4 result	=NearFarDepthCloudBlend(IN.texCoords.xy,imageTexture,nearImageTexture,lowResDepthTexture,depthTextureMS,viewportToTexRegionScaleBias,depthToLinFadeDistParams
		,inscatterTexture,nearInscatterTexture);
	result.rgb	*=exposure;
	return result;
}
 
vec4 GlowPS(v2f IN) : SV_TARGET
{
    // original image has double the resulution, so we sample 2x2
    vec4 c=vec4(0,0,0,0);
	c+=texture_clamp(imageTexture,IN.texCoords+offset/2.0);
	c+=texture_clamp(imageTexture,IN.texCoords-offset/2.0);
	vec2 offset2=offset;
	offset2.x=offset.x*-1.0;
	c+=texture_clamp(imageTexture,IN.texCoords+offset2/2.0);
	c+=texture_clamp(imageTexture,IN.texCoords-offset2/2.0);
	c=c*exposure/4.0;
	c-=1.0*vec4(1.0,1.0,1.0,1.0);
	c=clamp(c,vec4(0.0,0.0,0.0,0.0),vec4(10.0,10.0,10.0,10.0));
    return c;
}

technique11 simul_direct
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,DirectPS()));
    }
}


technique11 exposure_gamma
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,ExposureGammaPS()));
    }
}
technique11 warp_exposure_gamma
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,OffsetVS()));
		SetPixelShader(CompileShader(ps_4_0,WarpExposureGammaPS()));
    }
}

technique11 warp_glow_exposure_gamma
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,OffsetVS()));
		SetPixelShader(CompileShader(ps_4_0,WarpExposureGammaPS()));
    }
}

technique11 glow_exposure_gamma
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,GlowExposureGammaPS()));
    }
}

technique11 far_near_depth_blend
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(CloudBufferBlend,vec4(1.0f,1.0f,1.0f,1.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,MainVS()));
		SetPixelShader(CompileShader(ps_5_0,NearFarDepthCloudBlendPS()));
    }
}

technique11 simul_glow
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend,vec4(1.0f,1.0f,1.0f,1.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,GlowPS()));
    }
}


technique11 show_depth
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend,vec4( 0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,QuadVS()));
		SetPixelShader(CompileShader(ps_4_0,ShowDepthPS()));
    }
}

