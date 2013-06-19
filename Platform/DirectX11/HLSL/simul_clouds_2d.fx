#include "CppHlsl.hlsl"
#include "states.hlsl"
uniform sampler2D imageTexture;
uniform sampler2D coverageTexture1;
uniform sampler2D coverageTexture2;
uniform sampler2D lossTexture;
uniform sampler2D inscTexture;
uniform sampler2D skylTexture;
uniform sampler2D depthTexture;
SamplerState samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

#include "../../CrossPlatform/simul_2d_clouds.hs"

#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "simul_earthshadow.hlsl"

#include "../../CrossPlatform/simul_2d_clouds.sl"

struct a2v
{
    float3 position	: POSITION;
};

struct v2f
{
    float4 hPosition	: SV_POSITION;
    float4 clip_pos	: TEXCOORD3;
	vec2 texc_global	: TEXCOORD0;
	vec2 texc_detail	: TEXCOORD1;
	vec3 wPosition		: TEXCOORD2;
};

v2f MainVS(a2v IN)
{
	v2f OUT;
	vec3 pos		=IN.position.xyz;
	pos.z			+=origin.z;
	
	float Rh=planetRadius+origin.z;
	float dist=length(pos.xy);
	float vertical_shift=sqrt(Rh*Rh-dist*dist)-Rh;
	//pos.z+=vertical_shift;
	pos.xy			+=eyePosition.xy;
	OUT.clip_pos	=mul(worldViewProj,vec4(pos.xyz,1.0));
	OUT.hPosition	=OUT.clip_pos;
    OUT.wPosition	=pos.xyz;
    OUT.texc_global	=(pos.xy-origin.xy)/globalScale;
    OUT.texc_detail	=(pos.xy-origin.xy)/detailScale;
    return OUT;
}

float4 MainPS(v2f IN) : SV_TARGET
{
	vec3 depth_pos	=IN.clip_pos.xyz/IN.clip_pos.w;
	//depth_pos.z	=clip_pos.z;
	vec3 depth_texc	=0.5*(depth_pos+vec3(1.0,1.0,1.0));
	float4 result=Clouds2DPS(IN.texc_global,IN.texc_detail,IN.wPosition,depth_texc);
	return result;
}

technique11 simul_clouds_2d
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AlphaBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,MainPS()));
    }
}

struct a2v2
{
    float3 position	: POSITION;
	vec2 texc		: TEXCOORD0;
};

struct v2f2
{
    float4 hPosition	: SV_POSITION;
	vec2 texc			: TEXCOORD0;
};

v2f2 SimpleVS(idOnly IN)
{
	v2f2 OUT;
	float2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=float4(pos,0.0,1.0);
    OUT.texc	=0.5*(float2(1.0,1.0)+vec2(pos.x,pos.y));
	return OUT;
}
/*
v2f2 SimpleVS(a2v2 IN)
{
	v2f2 OUT;
    OUT.hPosition	=float4(IN.position.xy,1.0,1.0);
    OUT.texc		=IN.texc;
    return OUT;
}
*/

float4 SimplePS(v2f2 IN) : SV_TARGET
{
	return texture2D(imageTexture,IN.texc);
}

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float4 RandomPS(v2f2 IN) : SV_TARGET
{
    vec4 c=vec4(rand(IN.texc),rand(1.7*IN.texc),rand(0.11*IN.texc),rand(513.1*IN.texc));
    return c;
}

float4 DetailPS(v2f2 IN) : SV_TARGET
{
	vec4 result=vec4(0,0,0,0);
	vec2 texcoords=IN.texc;
	float mul=.5;
    for(int i=0;i<24;i++)
    {
		vec4 c=texture(imageTexture,texcoords);
		texcoords*=2.0;
		texcoords+=mul*vec2(0.2,0.2)*c.xy;
		result+=mul*c;
		mul*=persistence;
    }
    result.rgb=vec3(1.0,1.0,1.0);//=saturate(result*1.5);
	result.a=saturate(result.a-0.4)/0.4;
    return result;
}

float4 DetailLightingPS(v2f2 IN) : SV_TARGET
{
	vec4 c=texture(imageTexture,IN.texc);
	vec4 result=c;
	vec2 texcoords=IN.texc;
	float mul=0.5;
	vec2 offset=lightDir2d.xy/512.0;
	float dens_dist=0.0;
    for(int i=0;i<16;i++)
    {
		texcoords+=offset;
		vec4 v=texture(imageTexture,texcoords);
		dens_dist+=v.a;
		//if(v.a==0)
			//dens_dist*=0.9;
    }
    float l=c.a*exp(-dens_dist/2.0);
    return vec4(dens_dist/32.0,dens_dist/32.0,dens_dist/32.0,c.a);
}

technique11 simple
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,SimpleVS()));
		SetPixelShader(CompileShader(ps_4_0,SimplePS()));
    }
}

technique11 simul_random
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,SimpleVS()));
		SetPixelShader(CompileShader(ps_4_0,RandomPS()));
    }
}

technique11 simul_2d_cloud_detail
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,SimpleVS()));
		SetPixelShader(CompileShader(ps_4_0,DetailPS()));
    }
}


technique11 simul_2d_cloud_detail_lighting
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,SimpleVS()));
		SetPixelShader(CompileShader(ps_4_0,DetailLightingPS()));
    }
}



