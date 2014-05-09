#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/SL/solid_constants.sl"

Texture2D diffuseTexture;

struct vertexInput
{
    vec3 position		: POSITION;
    vec2 texCoords		: TEXCOORD0;
    vec3 normal			: TEXCOORD1;
};

struct vertexOutput
{
    vec4 hPosition		: SV_POSITION;
    vec2 texCoords		: TEXCOORD0;
    vec3 normal			: TEXCOORD1;
};

vertexOutput VS_Solid(vertexInput IN)
{
    vertexOutput OUT;
    OUT.hPosition	=mul(worldViewProj, float4(IN.position.xyz,1.f));
	OUT.texCoords	=IN.texCoords;
    OUT.normal.xyz	=IN.normal;
    return OUT;
}

float4 PS_Solid(vertexOutput IN) : SV_TARGET
{
    vec4 c = texture_wrap(diffuseTexture,vec2(IN.texCoords.x,1.0-IN.texCoords.y));
	c.a=1.0;
	float light=saturate(IN.normal.z);
	c.rgb*=light;
    return c;
}

technique11 solid
{
    pass base 
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(EnableDepth,0);
		SetBlendState(DontBlend,float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Solid()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Solid()));
    }
}
