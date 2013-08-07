#include "CppHlsl.hlsl"
#include "states.hlsl"
uniform sampler2D imageTexture;
uniform sampler2D noiseTexture;
uniform sampler2D coverageTexture;
uniform sampler2D lossTexture;
uniform sampler2D inscTexture;
uniform sampler2D skylTexture;
uniform sampler2D depthTexture;
uniform sampler2D illuminationTexture;

SamplerState samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

#include "../../CrossPlatform/simul_2d_clouds.hs"

#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "../../CrossPlatform/earth_shadow_uniforms.sl"
#include "../../CrossPlatform/earth_shadow.sl"
#include "../../CrossPlatform/earth_shadow_fade.sl"

#include "../../CrossPlatform/simul_2d_clouds.sl"
#include "../../CrossPlatform/simul_2d_cloud_detail.sl"
#include "../../CrossPlatform/depth.sl"

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
	vec3 pos			=IN.position.xyz;
	pos.z				+=origin.z;
	float Rh			=planetRadius+origin.z;
	float dist			=length(pos.xy);
	float vertical_shift=sqrt(Rh*Rh-dist*dist)-Rh;
	pos.z				+=vertical_shift;
	pos.xy				+=eyePosition.xy;
	OUT.clip_pos		=mul(worldViewProj,vec4(pos.xyz,1.0));
	OUT.hPosition		=OUT.clip_pos;
    OUT.wPosition		=pos.xyz;
    return OUT;
}

float4 MainPS(v2f IN) : SV_TARGET
{
	vec3 depth_pos		=IN.clip_pos.xyz/IN.clip_pos.w;
	vec3 depth_texc		=0.5*(depth_pos+vec3(1.0,1.0,1.0));
	depth_texc.y		=1.0-depth_texc.y;
    float depth			=texture(depthTexture,viewportCoordToTexRegionCoord(depth_texc.xy,viewportToTexRegionScaleBias)).x;
#ifdef REVERSE_DEPTH
	if(depth>0.0)
		discard;
#else
	if(1.0>depth)
		discard;
#endif
	vec2 wOffset		=IN.wPosition.xy-origin.xy;
	vec2 noiseOffset	=fractalAmplitude*texture(noiseTexture,wOffset/100000.0);
    vec2 texc_global	=wOffset/globalScale;
    vec2 texc_detail	=wOffset/detailScale;
	//texc_detail		+=noiseOffset;
	float dist			=depthToDistance(depth,depth_pos.xy,nearZ,farZ,tanHalfFov);
	vec3 wEyeToPos		=IN.wPosition-eyePosition;
	vec4 ret			=Clouds2DPS_illum(texc_global,texc_detail,wEyeToPos,dist,cloudInterp,sunlight.rgb,lightDir.xyz,lightResponse);
	ret.rgb				*=exposure;
	return ret;
}

technique11 simul_clouds_2d
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AlphaBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,MainPS()));
    }
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
	OUT.hPosition	=float4(rect.xy+rect.zw*pos,0.0,1.0);
    OUT.texCoords	=pos;
    return OUT;
}

float4 SimplePS(v2f2 IN) : SV_TARGET
{
	return texture2D(imageTexture,IN.texCoords);
}

float4 CoveragePS(v2f2 IN) : SV_TARGET
{
	return Coverage(IN.texCoords,coverageOctaves,coveragePersistence,time,noiseTexture);
}

float4 ShowDetailTexturePS(v2f2 IN) : SV_TARGET
{
    vec4 detail				=texture2D(imageTexture,IN.texCoords);
	float opacity			=saturate(detail.a);
	vec3 colour				=vec4(0.5,0.5,1.0,1.0);
	if(opacity<=0)
		return vec4(colour,1.0);
	float light				=exp(-detail.r*extinction);
	float scattered_light	=light;//detail.a*exp(-light*Y(coverage)*32.0);
	colour					*=1.0-opacity;
	colour					+=opacity*sunlight*(lightResponse.y+lightResponse.x)*scattered_light;
    return vec4(colour,1.0);
}

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float4 RandomPS(v2f2 IN) : SV_TARGET
{
    vec4 c=vec4(rand(IN.texCoords),rand(1.7*IN.texCoords),rand(0.11*IN.texCoords),rand(513.1*IN.texCoords));
    return frac(c);
}

float4 DetailPS(v2f2 IN) : SV_TARGET
{
    return DetailDensity(IN.texCoords,imageTexture);
}

float4 DetailLightingPS(v2f2 IN) : SV_TARGET
{
    return DetailLighting(IN.texCoords,imageTexture);
}

technique11 simul_coverage
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
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
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
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
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,SimpleVS()));
		SetPixelShader(CompileShader(ps_4_0,ShowDetailTexturePS()));
    }
}

technique11 simul_random
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,FullScreenVS()));
		SetPixelShader(CompileShader(ps_4_0,RandomPS()));
    }
}

technique11 simul_2d_cloud_detail
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,FullScreenVS()));
		SetPixelShader(CompileShader(ps_4_0,DetailPS()));
    }
}


technique11 simul_2d_cloud_detail_lighting
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,FullScreenVS()));
		SetPixelShader(CompileShader(ps_4_0,DetailLightingPS()));
    }
}



