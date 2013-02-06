#define uniform
#define vec2 float2
#define vec3 float3
#define vec4 float4
#define sampler1D texture1D
#define sampler2D texture2D
#define sampler3D texture3D
#define fract frac
#define texture(tex,texc) tex.Sample(samplerState,texc)
#define texture3D(tex,texc) tex.Sample(samplerState3d,texc)
SamplerState samplerState3d
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Wrap;
};

uniform int octaves;
uniform float persistence;
uniform float humidity;
uniform float time;
uniform vec3 noiseScale;

#include "../../CrossPlatform/simul_gpu_clouds.sl"

SamplerState samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
	AddressW = Clamp;
};

struct vertexInput
{
    float3 position		: POSITION;
    float2 texc			: TEXCOORD0;
};

struct vertexOutput
{
    float4 hPosition	: SV_POSITION;
	float2 texc			: TEXCOORD0;		
};

vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
    OUT.hPosition = float4(IN.position.xy,1.0,1.0);
	OUT.texc=IN.texc;
    return OUT;
}

float4 PS_Density(vertexOutput IN) : SV_TARGET
{
	vec3 densityspace_texcoord	=assemble3dTexcoord(IN.texc.xy);
	vec3 noisespace_texcoord	=densityspace_texcoord*noiseScale+vec3(1.0,1.0,0);
	float noise_val				=NoiseFunction(noisespace_texcoord,octaves,persistence,time);
	float hm					=humidity*GetHumidityMultiplier(densityspace_texcoord.z);
	float diffusivity			=0.02;
	float dens					=saturate((noise_val+hm-1.0)/diffusivity);
	dens						*=saturate(densityspace_texcoord.z/zPixel-0.5)*saturate((1.0-0.5*zPixel-densityspace_texcoord.z)/zPixel);
    return vec4(dens,0,0,1.0);
}

float4 PS_Lighting(vertexOutput IN) : SV_TARGET
{
    return float4(0,0,0,0);
}

float4 PS_Transform(vertexOutput IN) : SV_TARGET
{
    return float4(0,0,0,0);
}


//------------------------------------
// Technique
//------------------------------------
DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
};
BlendState DontBlend
{
	BlendEnable[0] = FALSE;
};
RasterizerState RenderNoCull { CullMode = none; };

technique11 simul_gpu_density
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Density()));
    }
}

technique11 simul_gpu_lighting
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Lighting()));
    }
}

technique11 simul_gpu_transform
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Transform()));
    }
}