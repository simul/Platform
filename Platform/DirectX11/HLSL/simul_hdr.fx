#include "CppHlsl.hlsl"
Texture2D imageTexture;
Texture2D<uint> glowTexture;
float4x4 worldViewProj	: WorldViewProjection;

SamplerState samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

float exposure=1.f;
float gamma=1.f/2.2f;
float2 offset;

struct a2v
{
    float4 position	: POSITION;
    float2 texcoord	: TEXCOORD0;
};

struct v2f
{
    float4 hPosition	: SV_POSITION;
    float2 texcoord		: TEXCOORD0;
};

v2f TonemapVS(a2v IN)
{
	v2f OUT;
    OUT.hPosition.xy=IN.position.xy;
	OUT.hPosition.z=0.05*IN.position.z;
	OUT.hPosition.w=0.5*IN.position.z+IN.position.w;
    OUT.hPosition = mul(worldViewProj, float4(IN.position.xyz,1.0));
	OUT.texcoord = IN.texcoord;
    return OUT;
}

float4 convertInt(float2 texCoord)
{
	uint2 tex_dim;
	glowTexture.GetDimensions(tex_dim.x, tex_dim.y);

	uint2 location = uint2((uint)(tex_dim.x * texCoord.x), (uint)(tex_dim.y * texCoord.y));
	uint int_color = glowTexture[location];

	// Convert R11G11B10 to float3
	float4 color;
	color.r = (float)(int_color >> 21) / 2047.0f;
	color.g = (float)((int_color >> 10) & 0x7ff) / 2047.0f;
	color.b = (float)(int_color & 0x0003ff) / 1023.0f;
	color.a = 1;

	return color;
}



float4 TonemapPS(v2f IN) : SV_TARGET
{
	float4 c=imageTexture.Sample(samplerState,IN.texcoord);
	float4 glow=convertInt(IN.texcoord);
	c.rgb+=glow.rgb;
	c.rgb*=exposure;
	c.rgb=pow(c.rgb,gamma);
    return float4(c.rgb,1.f);
}

float4 DirectPS(v2f IN) : SV_TARGET
{
	float4 c=imageTexture.Sample(samplerState,IN.texcoord);
    return float4(c.rgb,1.f);
}

float4 SkyOverStarsPS(v2f IN) : SV_TARGET
{
	float4 c=imageTexture.Sample(samplerState,IN.texcoord);
    return float4(c.rgba);
}

float4 GlowPS(v2f IN) : SV_TARGET
{
    // original image has double the resulution, so we sample 2x2
    vec4 c=vec4(0,0,0,0);
	c+=texture2D(imageTexture,IN.texcoord+offset/2.0);
	c+=texture2D(imageTexture,IN.texcoord-offset/2.0);
	vec2 offset2=offset;
	offset2.x=offset.x*-1.0;
	c+=texture2D(imageTexture,IN.texcoord+offset2/2.0);
	c+=texture2D(imageTexture,IN.texcoord-offset2/2.0);
	c=c*exposure/4.0;
	c-=1.0*vec4(1.0,1.0,1.0,1.0);
	c=clamp(c,vec4(0.0,0.0,0.0,0.0),vec4(10.0,10.0,10.0,10.0));
    return c;
}


DepthStencilState EnableDepth
{
	DepthEnable = TRUE;
	DepthWriteMask = ALL;
#ifdef REVERSE_DEPTH
	DepthFunc = GREATER_EQUAL;
#else
	DepthFunc = LESS_EQUAL;
#endif
};

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

BlendState DoBlend
{
	BlendEnable[0] = TRUE;
	SrcBlend = ONE;
	DestBlend = SRC_ALPHA;
	RenderTargetWriteMask[0]=7;
};

technique11 simul_direct
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,TonemapVS()));
		SetPixelShader(CompileShader(ps_4_0,DirectPS()));
    }
}


technique11 simul_tonemap
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,TonemapVS()));
		SetPixelShader(CompileShader(ps_4_0,TonemapPS()));
    }
}

technique11 simul_sky_over_stars
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
//		SetDepthStencilState( DisableDepth, 0 );
		SetDepthStencilState( EnableDepth, 0 );
		SetBlendState(DoBlend, float4(1.0f,1.0f,1.0f,1.0f ), 0xFFFFFFFF );
		//SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,TonemapVS()));
		SetPixelShader(CompileShader(ps_4_0,SkyOverStarsPS()));
    }
}

technique11 simul_glow
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend, float4(1.0f,1.0f,1.0f,1.0f ), 0xFFFFFFFF );
		//SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,TonemapVS()));
		SetPixelShader(CompileShader(ps_4_0,GlowPS()));
    }
}


