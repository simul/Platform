#include "CppHlsl.hlsl"
#include "../../CrossPlatform/depth.sl"
Texture2D imageTexture;
Texture2D depthTexture;
Texture2D<uint> glowTexture;
float4x4 worldViewProj	: WorldViewProjection;

SamplerState samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

uniform_buffer HdrConstants R9
{
	uniform float exposure=1.f;
	uniform float gamma=1.f/2.2f;
	uniform vec2 offset;
	uniform vec4 rect;
	uniform vec2 tanHalfFov;
	uniform float nearZ,farZ;
}

struct a2v
{
	uint vertex_id	: SV_VertexID;
    float4 position	: POSITION;
    float2 texcoord	: TEXCOORD0;
};

struct v2f
{
    float4 hPosition	: SV_POSITION;
    float2 texCoords	: TEXCOORD0;
};

v2f MainVS(idOnly IN)
{
	v2f OUT;
#if 1
	float2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=float4(pos,0.0,1.0);
	// Set to far plane so can use depth test as we want this geometry effectively at infinity
#ifdef REVERSE_DEPTH
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=0.5*(float2(1.0,1.0)+vec2(pos.x,-pos.y));

#else
    OUT.hPosition.xy=IN.position.xy;
	OUT.hPosition.z=0.05*IN.position.z;
	OUT.hPosition.w=0.5*IN.position.z+IN.position.w;
   // OUT.hPosition = mul(worldViewProj, float4(IN.position.xyz,1.0));
	OUT.texCoords = IN.texcoord;
#endif
    return OUT;
}

v2f QuadVS(idOnly IN)
{
    v2f OUT;
	vec2 poss[4]=
	{
		{ 1.0, 0.0},
		{ 1.0, 1.0},
		{ 0.0, 0.0},
		{ 0.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=vec4(rect.xy+rect.zw*pos,0.0,1.0);
	OUT.hPosition.z	=0.0; 
    OUT.texCoords	=pos;
	OUT.texCoords.y	=1.0-OUT.texCoords.y;
    return OUT;
}

vec4 ShowDepthPS(v2f IN) : SV_TARGET
{
	vec4 depth		=depthTexture.Sample(samplerState,IN.texCoords);
	float dist		=10.0*depthToDistance(depth.x,2.0*(IN.texCoords-0.5),nearZ,farZ,tanHalfFov);
    return vec4(dist,dist,dist,1.0);
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
	float4 c=imageTexture.Sample(samplerState,IN.texCoords);
	
	float4 glow=convertInt(IN.texCoords);
	c.rgb+=glow.rgb;
	c.rgb*=exposure;
	c.rgb=pow(c.rgb,gamma);
	c.rgb+=depthTexture.Sample(samplerState,IN.texCoords).rgb;
    return float4(c.rgb,1.f);
}

float4 GammaPS(v2f IN) : SV_TARGET
{
	float4 c=imageTexture.Sample(samplerState,IN.texCoords);
	c.rgb*=exposure;
	c.rgb=pow(c.rgb,gamma);
    return float4(c.rgb,1.f);
}

float4 DirectPS(v2f IN) : SV_TARGET
{
	float4 c=exposure*imageTexture.Sample(samplerState,IN.texCoords);
    return float4(c.rgba);
}

float4 GlowPS(v2f IN) : SV_TARGET
{
    // original image has double the resulution, so we sample 2x2
    vec4 c=vec4(0,0,0,0);
	c+=texture2D(imageTexture,IN.texCoords+offset/2.0);
	c+=texture2D(imageTexture,IN.texCoords-offset/2.0);
	vec2 offset2=offset;
	offset2.x=offset.x*-1.0;
	c+=texture2D(imageTexture,IN.texCoords+offset2/2.0);
	c+=texture2D(imageTexture,IN.texCoords-offset2/2.0);
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
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,DirectPS()));
    }
}


technique11 simul_gamma
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,GammaPS()));
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
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,TonemapPS()));
    }
}

technique11 simul_sky_blend
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
//		SetDepthStencilState( EnableDepth, 0 );
		SetBlendState(DoBlend, float4(1.0f,1.0f,1.0f,1.0f ), 0xFFFFFFFF );
		//SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,DirectPS()));
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
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,GlowPS()));
    }
}


technique11 show_depth
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,QuadVS()));
		SetPixelShader(CompileShader(ps_4_0,ShowDepthPS()));
    }
}

