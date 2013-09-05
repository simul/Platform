#include "CppHlsl.hlsl"
#include "../../CrossPlatform/states.sl"
#include "../../CrossPlatform/noise.sl"
Texture2D noise_texture SIMUL_TEXTURE_REGISTER(0);
Texture3D random_texture_3d SIMUL_TEXTURE_REGISTER(1);
RWTexture3D<float4> targetTexture SIMUL_RWTEXTURE_REGISTER(0);

SamplerState samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

uniform_buffer RendernoiseConstants SIMUL_BUFFER_REGISTER(10)
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

[numthreads(8,8,8)]
void CS_Random3D(uint3 pos	: SV_DispatchThreadID )	//SV_DispatchThreadID gives the combined id in each dimension.
{
	uint3 dims;
	targetTexture.GetDimensions(dims.x,dims.y,dims.z);
	if(pos.x>=dims.x||pos.y>=dims.y||pos.z>=dims.z)
		return;
	vec3 texCoords	=(vec3(pos)+vec3(0.5,0.5,0.5))/vec3(dims);
	vec2 texc2		=texCoords.xy+dims.y*texCoords.z;
	// Range from -1 to 1.
    vec4 c=2.0*vec4(rand(texc2),rand(1.7*texc2),rand(0.11*texc2),rand(513.1*texc2))-vec4(1.0,1.0,1.0,1.0);
    targetTexture[pos]			= c;
}

[numthreads(8,8,8)]
void CS_Noise3D(uint3 pos	: SV_DispatchThreadID )	//SV_DispatchThreadID gives the combined id in each dimension.
{
	uint3 dims;
	targetTexture.GetDimensions(dims.x,dims.y,dims.z);
	if(pos.x>=dims.x||pos.y>=dims.y||pos.z>=dims.z)
		return;
	vec4 result		=vec4(0,0,0,0);
	vec3 texCoords	=(vec3(pos)+vec3(0.5,0.5,0.5))/vec3(dims);
	float mult		=0.5;
	float total		=0.0;
    for(int i=0;i<octaves;i++)
    {
		vec4 c		=texture_wrap_lod(random_texture_3d,texCoords,0);
		texCoords	*=2.0;
		total		+=mult;
		result		+=mult*c;
		mult		*=persistence;
    }
	// divide by total to get the range -1,1.
	result*=1.0/total;
	targetTexture[pos]			=result;
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

technique11 random_3d_compute
{
    pass p0
	{
		SetComputeShader(CompileShader(cs_5_0,CS_Random3D()));
    }
}


technique11 noise_3d_compute
{
    pass p0
	{
		SetComputeShader(CompileShader(cs_5_0,CS_Noise3D()));
    }
}
