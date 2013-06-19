#include "CppHlsl.hlsl"
#include "states.hlsl"
uniform_buffer RainConstants R10
{
	float4x4 worldViewProj;
	float4 lightColour;
	float3 lightDir;
	float offset;
	float intensity;
};

texture2D rainTexture;
SamplerState rainSampler
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

struct vertexInputRenderRainTexture
{
    float4 position			: POSITION;
    float2 texCoords		: TEXCOORD0;
};

struct vertexInput
{
    float3 position		: POSITION;
    float4 texCoords	: TEXCOORD0;
};

struct vertexOutput
{
    float4 hPosition	: SV_POSITION;
    float4 texCoords	: TEXCOORD0;		/// z is intensity!
    float3 viewDir		: TEXCOORD1;
};
#define pi (3.1415926536f)

float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.f+g2-2.f*g*cos0;
	return (1.f-g2)/(4.f*pi*sqrt(u*u*u));
}

float rand(float2 co)
{
    return frac(sin(dot(co.xy ,float2(12.9898,78.233))) * 43758.5453);
}

vertexOutput VS_RenderRainTexture(idOnly IN) 
{
	vertexOutput OUT;
	float2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=float4(pos,1.0,1.0);
	float2 texc2 = .5*(float2(1.0,1.0)+vec2(pos.x,pos.y));
    OUT.texCoords	= float4(texc2, 0,0);
	OUT.viewDir	= float3(0,0,0);
	return OUT;
}
/*
vertexOutput VS_RenderRainTexture(vertexInputRenderRainTexture IN) 
{
    vertexOutput OUT;
    OUT.hPosition=float4(IN.position.xy,1.0,1.0);
    OUT.texCoords=float4(IN.texCoords,0,0);
    OUT.viewDir	=float3(0,0,0);
    return OUT;
}
*/

float4 PS_RenderRainTexture(vertexOutput IN): SV_TARGET
{
	float r=0;
	float2 t=IN.texCoords.xy;
	for(int i=0;i<32;i++)
	{
		r+=rand(frac(t.xy));
		t.y+=1.0/512.0;
	}
	r/=32.0;
	float4 result=float4(r,r,r,r);
    return result;
}
    
vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
    OUT.hPosition=mul(worldViewProj, float4(IN.position.xyz , 1.0));
	OUT.texCoords=IN.texCoords;
	OUT.viewDir=normalize(IN.position.xyz);
    return OUT;
}

float4 PS_Main(vertexOutput IN): SV_TARGET
{
	float2 offs=float2(0,offset);
	float4 fade=IN.texCoords.z;
	IN.texCoords.x*=4;
	IN.texCoords.y*=9;
	float4 colour=saturate(rainTexture.Sample(rainSampler,IN.texCoords.xy+offs)+intensity-1.0);
	IN.texCoords.xy*=1.7;
	colour+=0.7f*saturate(rainTexture.Sample(rainSampler,IN.texCoords.xy+2*offs)+intensity-1.0);
	IN.texCoords.xy*=4;
	colour+=0.4f*saturate(rainTexture.Sample(rainSampler,IN.texCoords.xy+4*offs)+intensity-1.0);
	IN.texCoords.xy*=4;
	colour+=0.2f*saturate(rainTexture.Sample(rainSampler,IN.texCoords.xy+8*offs)+intensity-1.0);
    colour=fade*saturate(colour);
	colour.a=colour.r*intensity;
	float cos0=dot(lightDir.xyz,normalize(IN.viewDir.xyz));
    colour*=lightColour*(0.1+HenyeyGreenstein(0.8,cos0));
    return colour;
}

technique11 simul_rain
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
		SetPixelShader(CompileShader(ps_4_0,PS_Main()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}

technique11 create_rain_texture
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_RenderRainTexture()));
		SetPixelShader(CompileShader(ps_4_0,PS_RenderRainTexture()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}
