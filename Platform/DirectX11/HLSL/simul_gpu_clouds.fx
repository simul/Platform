#define uniform
#define vec2 float2
#define vec3 float3
#define vec4 float4
#define sampler1D texture1D
#define sampler2D texture2D
#define sampler3D texture3D

SamplerState samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};
SamplerState samplerState3d
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Wrap;
};
SamplerState samplerState3d2
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Clamp;
};

#include "CppHLSL.hlsl"
uniform sampler2D input_light_texture;
uniform sampler3D density_texture;
uniform sampler3D light_texture;
uniform sampler3D ambient_texture;
uniform sampler3D volumeNoiseTexture;

#include "../../CrossPlatform/simul_gpu_clouds.sl"

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
    float4 outpos=mul(vertexMatrix,float4(IN.position.xy,1.0,1.0));
    OUT.hPosition=outpos;
	OUT.texc=0.5*float2(1.0+outpos.x,1.0-outpos.y);
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
static const float glow=0.01;
float4 PS_Lighting(vertexOutput IN) : SV_TARGET
{
	vec2 texcoord				=IN.texc.xy;//+texCoordOffset;
	vec4 previous_light			=texture2D(input_light_texture,texcoord.xy);
	vec3 lightspace_texcoord	=vec3(texcoord.xy,zPosition);
	vec3 densityspace_texcoord	=mul(transformMatrix,vec4(lightspace_texcoord,1.0)).xyz;
	float density				=texture3D2(density_texture,densityspace_texcoord).x;
	float direct_light			=previous_light.x*exp(-extinctions.x*density);
	float indirect_light		=previous_light.y*exp(-extinctions.y*density);
	indirect_light				+=(direct_light+indirect_light)*glow*density;//
    return						vec4(direct_light,indirect_light,0,0);
}

float4 PS_Transform(vertexOutput IN) : SV_TARGET
{
	vec3 densityspace_texcoord	=assemble3dTexcoord(IN.texc.xy);
	vec3 ambient_texcoord		=vec3(densityspace_texcoord.xy,1.0-zPixel/2.0-densityspace_texcoord.z);
	vec3 lightspace_texcoord	=mul(transformMatrix,vec4(densityspace_texcoord,1.0)).xyz;
	lightspace_texcoord.z		-=zPixel;
	vec2 light_lookup			=saturate(texture3D2(light_texture,lightspace_texcoord).xy);
	vec2 amb_texel				=texture3D2(ambient_texture,ambient_texcoord).xy;
	float ambient_lookup		=saturate(0.5*(amb_texel.x+amb_texel.y));
	float density				=saturate(texture3D2(density_texture,densityspace_texcoord).x);
	return						vec4(light_lookup.y,light_lookup.x,density,ambient_lookup);
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