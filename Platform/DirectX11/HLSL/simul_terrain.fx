#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/SL/simul_terrain_constants.sl"
#include "../../CrossPlatform/SL/states.sl"
#include "../../CrossPlatform/SL/cloud_shadow.sl"

Texture2D cloudShadowTexture;
Texture2DArray textureArray;
Texture2D grassTexture;

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
    OUT.hPosition	= mul(worldViewProj, float4(IN.position.xyz,1.0));
    OUT.wPosition	=float4(IN.position.xyz,1.0);
	OUT.texcoord	=vec2(IN.position.xy/2000.0);
    OUT.normal.xyz	=IN.normal;
    OUT.normal.a	=0.5;
    return OUT;
}
struct testOutput
{
    float4 hPosition		: SV_POSITION;
    float2 texCoords		: TEXCOORD0;
};
testOutput VS_TestAlphaToCoverage(idOnly IN)
{
	testOutput OUT;
	vec2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	vec2 pos		=poss[IN.vertex_id];
    OUT.hPosition	=mul(worldViewProj,float4(1000.0*pos.x,0.0,1500+1000.0*pos.y,1.0));
	OUT.texCoords	=0.5*(vec2(1.0,1.0)+vec2(pos.x,pos.y));
	return OUT;
}

vec4 PS_TestAlphaToCoverage(testOutput IN): SV_TARGET
{
	vec4 texColor	=texture_wrap(grassTexture,IN.texCoords);

    // clip will kill the pixel if argument is negative.  We clip if alpha too small.
    //clip(texColor.a - .5);
	//texColor.a		=texColor.a*0.5;
	return texColor;
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

	float from_lightning_centre_km	=0.001*length(IN.wPosition.xy-lightningCentre.xy);
	vec3 lightning				=lightningColour.rgb*saturate(1.0/pow(from_lightning_centre_km+.0001,2.0));
	result.rgb					+=lightning;
    return result;
}

technique11 simul_terrain
{
    pass base 
    {
		SetRasterizerState(RenderFrontfaceCull);
		SetDepthStencilState(EnableDepth,0);
		SetBlendState(DontBlend,float4(0.0, 0.0, 0.0, 0.0), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Main()));
    }
}

technique11 test_alpha_to_coverage
{
    pass base 
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(EnableDepth,0);
		SetBlendState(AlphaToCoverageBlend,float4(0.0, 0.0, 0.0, 0.0), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_TestAlphaToCoverage()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_TestAlphaToCoverage()));
    }
}
