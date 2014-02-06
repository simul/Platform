#include "CppHlsl.hlsl"
#include "states.hlsl"
sampler2D imageTexture SIMUL_TEXTURE_REGISTER(0);
Texture2DMS<float4> imageTextureMS SIMUL_TEXTURE_REGISTER(1);
TextureCube cubeTexture SIMUL_TEXTURE_REGISTER(2);

#define pi (3.1415926536)

uniform_buffer DebugConstants SIMUL_BUFFER_REGISTER(8)
{
	uniform mat4 worldViewProj;
	uniform int latitudes,longitudes;
	uniform float radius;
	uniform float multiplier;
};

cbuffer cbPerObject : register(b11)
{
	float4 rect;
};

struct a2v
{
    float3 position	: POSITION;
    float4 colour	: TEXCOORD0;
};

struct v2f
{
    float4 hPosition	: SV_POSITION;
    float4 colour		: TEXCOORD0;
};

v2f Debug2DVS(idOnly IN)
{
    v2f OUT;
	float2 poss[4]=
	{
		{ 1.0, 0.0},
		{ 1.0, 1.0},
		{ 0.0, 0.0},
		{ 0.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=float4(rect.xy+rect.zw*pos,0.0,1.0);
	OUT.hPosition.z	=0.0; 
	OUT.colour		=vec4(pos.x,1.0-pos.y,0,0);
    return OUT;
}

v2f DebugVS(a2v IN)
{
	v2f OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));
	OUT.colour = IN.colour;
    return OUT;
}

float4 DebugPS(v2f IN) : SV_TARGET
{
    return IN.colour;
}

vec4 TexturedPS(v2f IN) : SV_TARGET
{
	vec4 res=multiplier*texture_clamp(imageTexture,IN.colour.xy);
	return res;
}

vec4 TexturedMSPS(v2f IN) : SV_TARGET
{
	uint2 dims;
	uint numSamples;
	imageTextureMS.GetDimensions(dims.x,dims.y,numSamples);
	uint2 pos=uint2(IN.colour.xy*vec2(dims.xy));
	vec4 res=10000.0*imageTextureMS.Load(pos,0);
	return res;
}

struct vec3input
{
    float3 position	: POSITION;
};

v2f Vec3InputSignatureVS(vec3input IN)
{
	v2f OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));
	OUT.colour = float4(1.0,1.0,1.0,1.0);
    return OUT;
}

struct v2f_cubemap
{
    float4 hPosition	: SV_POSITION;
    float3 wDirection	: TEXCOORD0;
    float temp			: TEXCOORD1;
};

v2f_cubemap VS_DrawCubemap(vec3input IN) 
{
    v2f_cubemap OUT;
    OUT.hPosition	=mul(worldViewProj,float4(IN.position.xyz,1.0));
    OUT.wDirection	=normalize(IN.position.xyz);
    OUT.temp		=0.0;
    return OUT;
}

v2f_cubemap VS_DrawCubemapSphere(idOnly IN) 
{
    v2f_cubemap OUT;
	// we have (latitudes+1)*(longitudes+1)*2 id's
	uint vertex_id		=IN.vertex_id;
	uint latitude_strip	=vertex_id/(longitudes+1)/2;
	vertex_id			-=latitude_strip*(longitudes+1)*2;
	uint longitude		=vertex_id/2;
	vertex_id			-=longitude*2;
	float azimuth		=2.0*pi*float(longitude)/float(longitudes);
	float e				=float(latitude_strip+vertex_id)/float(latitudes+1);
	float elevation		=(e-0.5)*pi;
	vec3 pos			=radius*vec3(sin(azimuth)*cos(elevation),cos(azimuth)*cos(elevation),sin(elevation));
    OUT.hPosition		=mul(worldViewProj,vec4(pos.xyz,1.0));
    OUT.wDirection		=normalize(pos.xyz);
	OUT.temp			=elevation;
    return OUT;
}

SamplerState cubeSamplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Mirror;
	AddressV = Mirror;
	AddressW = Mirror;
};

float4 PS_DrawCubemap(v2f_cubemap IN): SV_TARGET
{
	//if(IN.wDirection.x<0)
	//	discard;
	vec3 view		=IN.wDirection.xyz;
	// Note: cubemap lookups are reversed, so we have to use -view.
	vec4 result		=cubeTexture.Sample(cubeSamplerState,-view);
	//result.rgb		=IN.temp;
	return vec4(result.rgb,1.f);
}


technique11 simul_debug
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,DebugVS()));
		SetPixelShader(CompileShader(ps_4_0,DebugPS()));
    }
}

technique11 textured
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,Debug2DVS()));
		SetPixelShader(CompileShader(ps_4_0,TexturedPS()));
    }
}

technique11 texturedMS
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,Debug2DVS()));
		SetPixelShader(CompileShader(ps_5_0,TexturedMSPS()));
    }
}

technique11 vec3_input_signature
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,Vec3InputSignatureVS()));
		SetPixelShader(CompileShader(ps_4_0,DebugPS()));
    }
}

technique11 draw_cubemap
{
    pass p0 
    {		
		SetRasterizerState( RenderBackfaceCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_DrawCubemap()));
		SetPixelShader(CompileShader(ps_4_0,PS_DrawCubemap()));
		SetDepthStencilState( EnableDepth, 0 );
		SetBlendState(DontBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
    }
}
technique11 draw_cubemap_sphere
{
    pass p0 
    {		
		SetRasterizerState( RenderBackfaceCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_DrawCubemapSphere()));
		SetPixelShader(CompileShader(ps_4_0	,PS_DrawCubemap()));
		SetDepthStencilState( EnableDepth, 0 );
		SetBlendState(DontBlend,vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
    }
}