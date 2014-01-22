#include "dx9.hlsl"
#include "../../CrossPlatform/states.sl"
#include "../../CrossPlatform/noise.sl"
#include "../../CrossPlatform/noise_constants.sl"

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

vec4 RandomPS(vertexOutputPosTexc IN) : COLOR
{
	// Range from -1 to 1.
    vec4 c=2.0*vec4(rand(IN.texCoords),rand(1.7*IN.texCoords),rand(0.11*IN.texCoords),rand(513.1*IN.texCoords))-1.0;
    return c;
}

vec4 MainPS(vertexOutputPosTexc IN) : COLOR
{
    vec4 res=0.5*(Noise(noise_texture,IN.texCoords, persistence, octaves)+vec4(1.0,1.0,1.0,1.0));
	return res;
}

technique random
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		VertexShader = compile vs_3_0 VS_FullScreen();
		PixelShader = compile ps_3_0 RandomPS();
    }
}

technique noise
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		VertexShader = compile vs_3_0 VS_FullScreen();
		PixelShader = compile ps_3_0 MainPS();
    }
}