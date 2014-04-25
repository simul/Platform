#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/SL/simul_terrain_constants.sl"
#include "../../CrossPlatform/SL/states.sl"
#include "../../CrossPlatform/SL/cloud_shadow.sl"

Texture2D cloudShadowTexture;
Texture2DArray textureArray;

struct vertexInput
{
    float3 position			: POSITION;
    float3 normal			: TEXCOORD0;
    float2 texcoord			: TEXCOORD1;
    float offset			: TEXCOORD2;
};

struct vertexOutput
{
    float4 hPosition		: SV_POSITION;
    float4 normal			: TEXCOORD0;
    float2 texcoord			: TEXCOORD1;
    float4 wPosition		: TEXCOORD2;
};

vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
    OUT.hPosition	= mul(worldViewProj, float4(IN.position.xyz,1.f));
    OUT.wPosition	=float4(IN.position.xyz,1.f);
	OUT.texcoord	=vec2(IN.position.xy/2000.0);
    OUT.normal.xyz	=IN.normal;
    OUT.normal.a	=0.5;
    return OUT;
}

float4 PS_Main( vertexOutput IN) : SV_TARGET
{
	vec4 result;
	vec4 layer1	=textureArray.Sample(wwcSamplerState,vec3(IN.texcoord,0.0));
	vec4 layer2	=textureArray.Sample(wwcSamplerState,vec3(IN.texcoord,1.0));
	vec4 texel	=mix(layer1,layer2,clamp(1.0-IN.wPosition.z/100.0,0.0,1.0));
	vec2 light	=lightDir.z;

	light		*=GetSimpleIlluminationAt(cloudShadowTexture,invShadowMatrix,IN.wPosition.xyz);
	result.rgb	=texel.rgb*(ambientColour.rgb+light.x*sunlight.rgb);
	result.a	=1.0;
    return result;
}

technique11 simul_terrain
{
    pass base 
    {
		SetRasterizerState(RenderFrontfaceCull);
		SetDepthStencilState(EnableDepth,0);
		SetBlendState(DontBlend,float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Main()));
    }
}
