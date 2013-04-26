#include "CppHlsl.hlsl"
Texture2D noise_texture;

SamplerState samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};
int octaves;
float persistence;

struct a2v
{
    float4 position  : POSITION;
    float2 texcoord  : TEXCOORD0;
};

struct v2f
{
    float4 position  : SV_POSITION;
    float2 texcoord  : TEXCOORD0;
};

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

v2f MainVS(a2v IN)
{
	v2f OUT;
	OUT.position = IN.position;
	OUT.texcoord = IN.texcoord;
    return OUT;
}

float4 RandomPS(v2f IN) : SV_TARGET
{
    vec4 c=vec4(rand(IN.texcoord),rand(1.7*IN.texcoord),rand(0.11*IN.texcoord),rand(513.1*IN.texcoord));
    return c;
}

float4 MainPS(v2f IN) : SV_TARGET
{
	vec4 result=vec4(0,0,0,0);
	vec2 texcoords=IN.texcoord;
	float mul=.5;
	vec4 hal=vec4(0.5,0.5,0.5,0.5);
    for(int i=0;i<octaves;i++)
    {
		vec4 c=texture2D(noise_texture,texcoords)-hal;
		texcoords*=2.0;
		result+=mul*c;
		mul*=persistence;
    }
	result*=0.5;
	result+=hal;
    result=saturate(result);
    return result;
}

DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
};

RasterizerState RenderNoCull
{
	CullMode = none;
};

BlendState NoBlend
{
	BlendEnable[0] = FALSE;
};

technique11 simul_random
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4(1.0f,1.0f,1.0f,1.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		//SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,RandomPS()));
    }
}

technique11 simul_noise_2d
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4(1.0f,1.0f,1.0f,1.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		//SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,MainPS()));
    }
}

