#include "CppHlsl.hlsl"
#include "states.hlsl"
uniform sampler2D imageTexture;
uniform sampler2D noiseTexture;
uniform sampler2D coverageTexture;
uniform sampler2D lossTexture;
uniform sampler2D inscTexture;
uniform sampler2D skylTexture;
uniform sampler2D depthTexture;
uniform Texture2DMS<float4> depthTextureMS;
uniform sampler2D illuminationTexture;
uniform sampler2D lightTableTexture;

SamplerState samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

#include "../../CrossPlatform/SL/simul_2d_clouds.hs"

#include "../../CrossPlatform/SL/simul_inscatter_fns.sl"
#include "../../CrossPlatform/SL/earth_shadow_uniforms.sl"
#include "../../CrossPlatform/SL/earth_shadow.sl"
#include "../../CrossPlatform/SL/earth_shadow_fade.sl"

#include "../../CrossPlatform/SL/simul_2d_clouds.sl"
#include "../../CrossPlatform/SL/simul_2d_cloud_detail.sl"
#include "../../CrossPlatform/SL/depth.sl"

struct a2v
{
    float3 position	: POSITION;
};

struct v2f
{
    float4 hPosition	: SV_POSITION;
    float4 clip_pos		: TEXCOORD0;
	vec3 wPosition		: TEXCOORD1;
};

v2f MainVS(a2v IN)
{
	v2f OUT;
	vec3 pos			=maxCloudDistanceMetres*IN.position.xyz;
	pos.z				+=origin.z;
	float Rh			=planetRadius+origin.z;
	float dist			=length(pos.xy);
	float vertical_shift=sqrt(Rh*Rh-dist*dist)-Rh;
	pos.z				+=vertical_shift;
	pos.xy				+=eyePosition.xy;
	OUT.clip_pos		=mul(worldViewProj,vec4(pos.xyz,1.0));
	// Prevent clipping:
#if REVERSE_DEPTH==1
	if(OUT.clip_pos.z<0)
		OUT.clip_pos.z=0;
#else
	if(OUT.clip_pos.z>OUT.clip_pos.w)
		OUT.clip_pos.z=OUT.clip_pos.w;
#endif
	OUT.hPosition		=OUT.clip_pos;
    OUT.wPosition		=pos.xyz;
    return OUT;
}

vec4 msaaPS(v2f IN) : SV_TARGET
{
	vec2 viewportTexCoords	=0.5*(vec2(1.0,1.0)+(IN.clip_pos.xy/IN.clip_pos.w));
	viewportTexCoords.y		=1.0-viewportTexCoords.y;
	uint2 depthDims;
	uint depthSamples;
	vec2 depthTexCoords=viewportCoordToTexRegionCoord(viewportTexCoords.xy,viewportToTexRegionScaleBias);
	depthTextureMS.GetDimensions(depthDims.x,depthDims.y,depthSamples);
	uint2 depth_pos2	=uint2(depthTexCoords.xy*vec2(depthDims.xy));
	float dlookup 		=depthTextureMS.Load(depth_pos2,0).r;
#if REVERSE_DEPTH==1
	if(dlookup!=0)
		discard;
#else
	// RVK: dlookup < 1.0 seems to not have the required accuracy.
	if(dlookup<0.9999999)
		discard;
#endif
	vec2 wOffset		=IN.wPosition.xy-origin.xy;
    vec2 texc_global	=wOffset/globalScale;
    vec2 texc_detail	=wOffset/detailScale;
	vec3 wEyeToPos		=IN.wPosition-eyePosition;
#if USE_LIGHT_TABLES==1
	float alt_texc		=IN.wPosition.z/maxAltitudeMetres;
	vec3 sun_irr		=texture_clamp_lod(lightTableTexture,vec2(alt_texc,0.5/3.0),0).rgb;
	vec3 moon_irr		=texture_clamp_lod(lightTableTexture,vec2(alt_texc,1.5/3.0),0).rgb;
	vec3 ambient_light	=texture_clamp_lod(lightTableTexture,vec2(alt_texc,2.5/3.0),0).rgb*lightResponse.w;
#else
	vec3 sun_irr		=sunlight.rgb;
	vec3 moon_irr		=moonlight.rgb;
	vec3 ambient_light	=ambientLight.rgb;
#endif
	vec4 ret			=Clouds2DPS_illum(imageTexture,coverageTexture
										,illuminationTexture
										,lossTexture
										,inscTexture
										,skylTexture
										,noiseTexture
										,texc_global,texc_detail
										,wEyeToPos
										,sun_irr
										,moon_irr
										,ambient_light.rgb
										,lightDir.xyz
										,lightResponse);

	ret.rgb				*=exposure;
	return ret;
}

vec4 MainPS(v2f IN) : SV_TARGET
{
	vec2 viewportTexCoords	=0.5*(vec2(1.0,1.0)+(IN.clip_pos.xy/IN.clip_pos.w));
	viewportTexCoords.y		=1.0-viewportTexCoords.y;
	uint2 depthDims;
	uint depthSamples;
	vec2 depthTexCoords	=viewportCoordToTexRegionCoord(viewportTexCoords.xy,viewportToTexRegionScaleBias);
	float dlookup 		=texture_clamp_lod(depthTexture,depthTexCoords,0);
#if REVERSE_DEPTH==1
	if(dlookup!=0)
		discard;
#else
	if(dlookup<0.999999)
		discard;
#endif
	vec2 wOffset		=IN.wPosition.xy-origin.xy;
    vec2 texc_global	=wOffset/globalScale;
    vec2 texc_detail	=wOffset/detailScale;
	vec3 wEyeToPos		=IN.wPosition-eyePosition;
#if USE_LIGHT_TABLES==1
	float alt_texc		=IN.wPosition.z/maxAltitudeMetres;
	vec3 sun_irr		=texture_clamp_lod(lightTableTexture,vec2(alt_texc,0.5/3.0),0).rgb;
	vec3 moon_irr		=texture_clamp_lod(lightTableTexture,vec2(alt_texc,1.5/3.0),0).rgb;
	vec3 ambient_light	=texture_clamp_lod(lightTableTexture,vec2(alt_texc,2.5/3.0),0).rgb*lightResponse.w;
#else
	vec3 sun_irr		=sunlight.rgb;
	vec3 moon_irr		=moonlight.rgb;
	vec3 ambient_light	=ambientLight.rgb;
#endif
	vec4 ret			=Clouds2DPS_illum(imageTexture
										,coverageTexture
										,illuminationTexture
										,lossTexture
										,inscTexture
										,skylTexture
										,noiseTexture
										,texc_global,texc_detail
										,wEyeToPos
										,sun_irr
										,moon_irr
										,ambient_light.rgb
										,lightDir.xyz
										,lightResponse);
	
	ret.rgb				*=exposure;
	return ret;
}

struct v2f2
{
    float4 hPosition	: SV_POSITION;
	vec2 texCoords		: TEXCOORD0;
};

v2f2 FullScreenVS(idOnly IN)
{
	v2f2 OUT;
	float2 poss[4]=
	{
		{ 1.0, 0.0},
		{ 1.0, 1.0},
		{ 0.0, 0.0},
		{ 0.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=float4(2.0*pos-vec2(1.0,1.0),0.0,1.0);
    OUT.texCoords	=pos;
    return OUT;
}

v2f2 SimpleVS(idOnly IN)
{
	v2f2 OUT;
	float2 poss[4]=
	{
		{ 1.0, 0.0},
		{ 1.0, 1.0},
		{ 0.0, 0.0},
		{ 0.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=float4(rect.xy+rect.zw*pos,0.0,1.0);
    OUT.texCoords	=pos;
    return OUT;
}

float4 SimplePS(v2f2 IN) : SV_TARGET
{
	return texture2D(imageTexture,.5+IN.texCoords);
}

float4 CoveragePS(v2f2 IN) : SV_TARGET
{
	return Coverage(IN.texCoords,humidity,diffusivity,coverageOctaves,coveragePersistence,time,noiseTexture,noiseTextureScale);
}

float4 ShowDetailTexturePS(v2f2 IN) : SV_TARGET
{
	return ShowDetailTexture(imageTexture,IN.texCoords,sunlight,lightResponse);
}

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float4 RandomPS(v2f2 IN) : SV_TARGET
{
    vec4 c	=vec4(rand(IN.texCoords),rand(1.7*IN.texCoords),rand(0.11*IN.texCoords),rand(513.1*IN.texCoords));
    return frac(c);
}

float4 DetailPS(v2f2 IN) : SV_TARGET
{
    return DetailDensity(IN.texCoords,imageTexture,amplitude);
}

float4 DetailLightingPS(v2f2 IN) : SV_TARGET
{
    return DetailLighting(IN.texCoords,imageTexture);
}

technique11 coverage
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,FullScreenVS()));
		SetPixelShader(CompileShader(ps_4_0,CoveragePS()));
    }
}

technique11 simple
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,SimpleVS()));
		SetPixelShader(CompileShader(ps_4_0,SimplePS()));
    }
}

technique11 show_detail_texture
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,SimpleVS()));
		SetPixelShader(CompileShader(ps_4_0,ShowDetailTexturePS()));
    }
}

technique11 random
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,FullScreenVS()));
		SetPixelShader(CompileShader(ps_4_0,RandomPS()));
    }
}

technique11 detail_density
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,FullScreenVS()));
		SetPixelShader(CompileShader(ps_4_0,DetailPS()));
    }
}

technique11 detail_lighting
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,FullScreenVS()));
		SetPixelShader(CompileShader(ps_4_0,DetailLightingPS()));
    }
}
BlendState AlphaBlendX
{
	BlendEnable[0] = TRUE;
	BlendEnable[1] = FALSE;
	SrcBlend = SRC_ALPHA;
	DestBlend = ZERO;
    BlendOp = ADD;
    SrcBlendAlpha = ZERO;
    DestBlendAlpha = INV_SRC_ALPHA;
    BlendOpAlpha = ADD;
    //RenderTargetWriteMask[0]	=0x07;
};

technique11 simul_clouds_2d_msaa
{
    pass p0
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(TestDepth,0);
		SetBlendState(AlphaBlendX,float4(0.0,0.0,0.0,0.0),0xFFFFFFFF);
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,MainVS()));
		SetPixelShader(CompileShader(ps_5_0,msaaPS()));
    }
}
technique11 simul_clouds_2d
{
    pass p0
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(TestDepth,0);
		SetBlendState(AlphaBlendX,float4(0.0,0.0,0.0,0.0),0xFFFFFFFF);
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,MainVS()));
		SetPixelShader(CompileShader(ps_5_0,MainPS()));
    }
}