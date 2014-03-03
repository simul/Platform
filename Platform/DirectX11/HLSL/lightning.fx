#include "CppHlsl.hlsl"
#include "../../CrossPlatform/lightning_constants.sl"
#include "states.hlsl"

Texture2D lightningTexture;

struct vertexOutput
{
    vec4 hPosition	: SV_POSITION;
	vec4 texCoords	: TEXCOORD0;
};

vertexOutput VS_Main(LightningVertexInput IN)
{
    vertexOutput OUT;
    OUT.hPosition	=mul(worldViewProj, vec4(IN.position.xyz , 1.0));
	OUT.texCoords	=IN.texCoords;
    return OUT;
}

float4 PS_Main(vertexOutput IN): SV_TARGET
{
	float4 colour=vec4(1.0,1.0,0,.5);//lightningTexture.Sample(clampSamplerState,IN.texCoords.xy);
    return colour;
}

technique11 lightning
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState(RenderNoCull);
		SetBlendState(DoBlend,vec4(0.0f,0.0f,0.0f,0.0f),0xFFFFFFFF);
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Main()));
		SetPixelShader(CompileShader(ps_5_0,PS_Main()));
    }
}

