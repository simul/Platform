#include "CppHlsl.hlsl"
#include "../../CrossPlatform/depth.sl"
#include "states.hlsl"
Texture2D imageTexture;
Texture2D depthTexture;
Texture2D cloudDepthTexture;
Texture2D<uint> glowTexture;
float4x4 worldViewProj	: WorldViewProjection;

SIMUL_CONSTANT_BUFFER(HdrConstants,9)
	uniform float exposure=1.f;
	uniform float gamma=1.f/2.2f;
	uniform vec2 offset;
	uniform vec4 rect;
	uniform vec2 tanHalfFov;
	uniform float nearZ,farZ;
	uniform vec3 depthToLinFadeDistParams;
SIMUL_CONSTANT_BUFFER_END

struct a2v
{
	uint vertex_id	: SV_VertexID;
    float4 position	: POSITION;
    float2 texcoord	: TEXCOORD0;
};

struct v2f
{
    float4 hPosition: SV_POSITION;
    float2 texCoords: TEXCOORD0;
};

v2f MainVS(idOnly IN)
{
	v2f OUT;
	vec2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	vec2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=vec4(pos,0.0,1.0);
	// Set to far plane so can use depth test as we want this geometry effectively at infinity
#ifdef REVERSE_DEPTH
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=0.5*(vec2(1.0,1.0)+vec2(pos.x,-pos.y));
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
	vec4 depth		=texture_clamp(depthTexture,IN.texCoords);
	float dist		=10.0*depthToFadeDistance(depth.x,2.0*(IN.texCoords-0.5),depthToLinFadeDistParams,tanHalfFov);
    return vec4(1,dist,dist,1.0);
}

vec4 convertInt(vec2 texCoord)
{
	uint2 tex_dim;
	glowTexture.GetDimensions(tex_dim.x, tex_dim.y);

	uint2 location = uint2((uint)(tex_dim.x * texCoord.x), (uint)(tex_dim.y * texCoord.y));
	uint int_color = glowTexture[location];

	// Convert R11G11B10 to float3
	vec4 color;
	color.r = (float)(int_color >> 21) / 2047.0f;
	color.g = (float)((int_color >> 10) & 0x7ff) / 2047.0f;
	color.b = (float)(int_color & 0x0003ff) / 1023.0f;
	color.a = 1;

	return color;
}

vec4 TonemapPS(v2f IN) : SV_TARGET
{
	vec4 c=texture_clamp(imageTexture,IN.texCoords);
	vec4 glow=convertInt(IN.texCoords);
	c.rgb+=glow.rgb;
	c.rgb*=exposure;
	c.rgb=pow(c.rgb,gamma);
    return vec4(c.rgb,1.f);
}

vec4 GammaPS(v2f IN) : SV_TARGET
{
	vec4 c=texture_clamp(imageTexture,IN.texCoords);
	c.rgb*=exposure;
	c.rgb=pow(c.rgb,gamma);
    return vec4(c.rgb,1.f);
}

vec4 DirectPS(v2f IN) : SV_TARGET
{
	vec4 c=exposure*texture_clamp(imageTexture,IN.texCoords);
    return vec4(c.rgba);
}

// Blend the low-res clouds into the scene, using a hi-res depth texture.
// depthTexture.x is the depth value.
// imageTexture.a is the depth that was used to calculate the particular cloud value.

// the blend is 1.0, SRC_ALPHA.
vec4 CloudBlendPS(v2f IN) : SV_TARGET
{
	vec4 c		=texture_nearest_lod(imageTexture,IN.texCoords,0);
	float d1	=texture_nearest_lod(depthTexture,IN.texCoords,0).x;
	float d2	=texture_nearest_lod(cloudDepthTexture,IN.texCoords,0).x;
	//depthToFadeDistance(d1,texture_nearest_lod,depthToLinFadeDistParams,tanHalf);
	float a		=c.a;
	vec3 rgb	=c.rgb*exposure;
	float blend	=saturate((d1-d2)*10000.0);//(1.0-c.a)*
	vec4 res	=vec4(rgb,c.a);
	//res			=lerp(res,vec4(0,0,0,1.0),blend);
    return res;
}

vec4 GlowPS(v2f IN) : SV_TARGET
{
    // original image has double the resulution, so we sample 2x2
    vec4 c=vec4(0,0,0,0);
	c+=texture_clamp(imageTexture,IN.texCoords+offset/2.0);
	c+=texture_clamp(imageTexture,IN.texCoords-offset/2.0);
	vec2 offset2=offset;
	offset2.x=offset.x*-1.0;
	c+=texture_clamp(imageTexture,IN.texCoords+offset2/2.0);
	c+=texture_clamp(imageTexture,IN.texCoords-offset2/2.0);
	c=c*exposure/4.0;
	c-=1.0*vec4(1.0,1.0,1.0,1.0);
	c=clamp(c,vec4(0.0,0.0,0.0,0.0),vec4(10.0,10.0,10.0,10.0));
    return c;
}

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
		SetBlendState(CloudBufferBlend,vec4(1.0f,1.0f,1.0f,1.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,CloudBlendPS()));
    }
}

technique11 simul_glow
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend,vec4(1.0f,1.0f,1.0f,1.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,GlowPS()));
    }
}


technique11 show_depth
{
    pass p0
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(NoBlend,vec4( 0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,QuadVS()));
		SetPixelShader(CompileShader(ps_4_0,ShowDepthPS()));
    }
}

