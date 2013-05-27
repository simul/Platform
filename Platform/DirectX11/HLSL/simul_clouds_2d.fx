#include "CppHlsl.hlsl"
#include "states.hlsl"
uniform sampler2D imageTexture;
uniform sampler2D coverageTexture1;
uniform sampler2D coverageTexture2;
uniform sampler2D lossTexture;
uniform sampler2D inscTexture;
uniform sampler2D skylTexture;
SamplerState samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

#include "../../CrossPlatform/simul_2d_clouds.hs"

#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "simul_earthshadow.hlsl"

#include "../../CrossPlatform/simul_2d_clouds.sl"

struct a2v
{
    float3 position	: POSITION;
};

struct v2f
{
    float4 hPosition	: SV_POSITION;
	vec2 texc_global	: TEXCOORD0;
	vec2 texc_detail	: TEXCOORD1;
	vec3 wPosition		: TEXCOORD2;
};

v2f MainVS(a2v IN)
{
	v2f OUT;
	vec3 pos		=IN.position.xyz;
	pos.z			+=origin.z;
	
	float Rh=planetRadius+origin.z;
	float dist=length(pos.xy);
	float vertical_shift=sqrt(Rh*Rh-dist*dist)-Rh;
	pos.z+=vertical_shift;
	pos.xy			+=eyePosition.xy;
	OUT.hPosition	=mul(worldViewProj,vec4(pos.xyz,1.0));
    OUT.wPosition	=pos.xyz;
    OUT.texc_global	=(pos.xy-origin.xy)/globalScale;
    OUT.texc_detail	=(pos.xy-origin.xy)/detailScale;
    return OUT;
}

float4 MainPS(v2f IN) : SV_TARGET
{
	return Clouds2DPS(IN.texc_global,IN.texc_detail,IN.wPosition);
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

struct a2v2
{
    float3 position	: POSITION;
	vec2 texc		: TEXCOORD0;
};

struct v2f2
{
    float4 hPosition	: SV_POSITION;
	vec2 texc			: TEXCOORD0;
};

v2f2 SimpleVS(a2v2 IN)
{
	v2f2 OUT;
    OUT.hPosition	=float4(IN.position.xy,1.0,1.0);
    OUT.texc		=IN.texc;
    return OUT;
}

float4 SimplePS(v2f2 IN) : SV_TARGET
{
	return texture2D(imageTexture,IN.texc);
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


