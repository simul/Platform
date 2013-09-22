#include "dx9.hlsl"
#include "../../CrossPlatform/states.sl"
#include "../../CrossPlatform/noise.sl"

texture noiseTexture;
sampler2D noise_texture= sampler_state 
{
    Texture = <noiseTexture>;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
	AddressU = Wrap;
	AddressV = Wrap;
};
struct a2v
{
    float4 position  : POSITION;
    float2 texCoords  : TEXCOORD0;
};

struct v2f
{
    float4 position  : POSITION;
    float2 texCoords  : TEXCOORD0;
};

v2f MainVS(a2v IN)
{
	v2f OUT;
	OUT.position = IN.position;
	OUT.texCoords = IN.texCoords;
    return OUT;
}

float4 RandomPS(v2f IN) : COLOR
{
	// Range from -1 to 1.
    vec4 c=2.0*vec4(rand(IN.texCoords),rand(1.7*IN.texCoords),rand(0.11*IN.texCoords),rand(513.1*IN.texCoords))-1.0;
    return c;
}

float4 MainPS(v2f IN) : COLOR
{
    return Noise( noise_texture,IN.texCoords, persistence, octaves);
}

technique simul_rendernoise
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		VertexShader = compile vs_2_0 MainVS();
		PixelShader = compile ps_3_0 MainPS();
    }
}