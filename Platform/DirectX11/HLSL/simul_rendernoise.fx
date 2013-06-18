#include "CppHlsl.hlsl"
Texture2D noise_texture;

SamplerState samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

uniform_buffer RendernoiseConstants R10
{
	int octaves;
	float persistence;
};

struct a2v
{
    float4 position  : POSITION;
    float2 texCoords  : TEXCOORD0;
};

struct v2f
{
    float4 hPosition  : SV_POSITION;
    float2 texCoords  : TEXCOORD0;
};

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

v2f MainVS(idOnly IN)
{
    v2f OUT;
	float2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=float4(pos,0.0,1.0);
    OUT.texCoords	=0.5*(float2(1.0,1.0)+vec2(pos.x,pos.y));
	return OUT;
}

float4 RandomPS(v2f IN) : SV_TARGET
{
	// Range from -1 to 1.
    vec4 c=2.0*vec4(rand(IN.texCoords),rand(1.7*IN.texCoords),rand(0.11*IN.texCoords),rand(513.1*IN.texCoords))-1.0;
    return c;
}

float4 MainPS(v2f IN) : SV_TARGET
{
	vec4 result=vec4(0,0,0,0);
	vec2 texcoords=IN.texCoords;
	float mul=.5;
	float total=0.0;
    for(int i=0;i<octaves;i++)
    {
		vec4 c=texture2D(noise_texture,texcoords);
		texcoords*=2.0;
		total+=mul;
		result+=mul*c;
		mul*=persistence;
    }
	// divide by total to get the range -1,1.
	result*=1.0/total;
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

