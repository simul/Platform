#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/rain_constants.sl"
#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "../../CrossPlatform/depth.sl"
#include "../../CrossPlatform/noise.sl"
texture3D randomTexture3D;
TextureCube cubeTexture;
texture2D rainTexture;
texture2D showTexture;
// The RESOLVED depth texture at full resolution
texture2D depthTexture;
SamplerState rainSampler
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

struct vertexInputRenderRainTexture
{
    float4 position			: POSITION;
    float2 texCoords		: TEXCOORD0;
};

struct vertexInput
{
    float3 position		: POSITION;
    float4 texCoords	: TEXCOORD0;
};

struct posOnly
{
    float3 position		: POSITION;
	uint vertex_id			: SV_VertexID;
};

struct vertexOutput
{
    float4 hPosition	: SV_POSITION;
    float4 texCoords	: TEXCOORD0;		/// z is intensity!
    float3 viewDir		: TEXCOORD1;
};

struct rainVertexOutput
{
    float4 position			: SV_POSITION;
    float2 clip_pos			: TEXCOORD1;
    float2 texCoords		: TEXCOORD0;
};

struct particleVertexOutput
{
    float4 position			:SV_POSITION;
	float pointSize			:PSIZE;
	float brightness		:TEXCOORD0;
	vec3 view				:TEXCOORD1;
};

struct particleGeometryOutput
{
    float4 position			:SV_POSITION;
    float2 texCoords		:TEXCOORD0;
	float brightness		:TEXCOORD1;
	vec3 view				:TEXCOORD2;
};

vec3 Frac(vec3 pos,float scale)
{
	vec3 unity		=vec3(1.0,1.0,1.0);
	return scale*(2.0*frac(0.5*(pos/scale+unity))-unity);
}

posTexVertexOutput VS_ShowTexture(idOnly id)
{
    return VS_ScreenQuad(id,rect);
}

float4 PS_ShowTexture(posTexVertexOutput In): SV_TARGET
{
    return texture_wrap_lod(showTexture,In.texCoords,0);
}

particleVertexOutput VS_Particles(posOnly IN)
{
	particleVertexOutput OUT;
	vec3 particlePos	=IN.position.xyz;
	particlePos			*=30.0;
	particlePos			-=viewPos;
	particlePos.z		-=offset;
	particlePos			=Frac(particlePos,30.0);
	float p				=flurryRate*phase;
	vec3 pos			=particlePos+IN.position.xyz;
	vec3 rand1			=randomTexture3D.SampleLevel(wrapSamplerState,pos/40.0,0).xyz;
	vec3 rand2			=randomTexture3D.SampleLevel(wrapSamplerState,pos/40.0*5.0,0).xyz;
	particlePos			+=1.5*flurry*rand1;
	particlePos			+=.3*flurry*rand2;
	OUT.position		=mul(worldViewProj,float4(particlePos.xyz,1.0));
	OUT.view			=normalize(particlePos.xyz);
	OUT.pointSize		=snowSize*(1.0+0.2*rand2.y);
	OUT.brightness		=(float)frac(viewPos.x);//60.1/length(clip_pos-viewPos);
	return OUT;
}

cbuffer cbImmutable
{
    float4 g_positions[4] : packoffset(c0) =
    {
        float4( 0.5,-0.5,0,0),
        float4( 0.5, 0.5,0,0),
        float4(-0.5,-0.5,0,0),
        float4(-0.5, 0.5,0,0),
    };
    float2 g_texcoords[4] : packoffset(c4) = 
    { 
        float2(1,0), 
        float2(1,1),
        float2(0,0),
        float2(0,1),
    };
};

[maxvertexcount(4)]
void GS_Particles(point particleVertexOutput input[1], inout TriangleStream<particleGeometryOutput> SpriteStream)
{
    particleGeometryOutput	output;
	vec4 pos				=input[0].position;
	// Emit two new triangles
	[unroll]for(int i=0;i<4;i++)
	{
        output.position		=pos+float4(g_positions[i].xy*input[0].pointSize,0,0); 
        //output.Position.y *= g_fAspectRatio; // Correct for the screen aspect ratio, since the sprite is calculated in eye space
		output.texCoords	=g_texcoords[i];
        output.brightness	=input[0].brightness;  
        output.view			=input[0].view;     
		SpriteStream.Append(output);
    }
    SpriteStream.RestartStrip();
}

float4 PS_Particles(particleGeometryOutput IN): SV_TARGET
{
	float4 result	=cubeTexture.Sample(wrapSamplerState,-IN.view);
	vec2 pos		=IN.texCoords*2.0-1.0;
	float radius	=intensity*length(pos.xy);
	float opacity	=saturate(intensity-radius)/.5;
	return float4(2*result.rgb,opacity);
}

rainVertexOutput VS_FullScreen(idOnly IN)
{
	rainVertexOutput OUT;
	float2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	OUT.clip_pos	=poss[IN.vertex_id];
	OUT.position	=float4(OUT.clip_pos,0.0,1.0);
	// Set to far plane
#ifdef REVERSE_DEPTH
	OUT.position.z	=0.0; 
#else
	OUT.position.z	=OUT.position.w; 
#endif
    OUT.texCoords	=0.5*(float2(1.0,1.0)+vec2(OUT.clip_pos.x,-OUT.clip_pos.y));
//OUT.texCoords	+=0.5*texelOffsets;
	return OUT;
}

#define NUM (8)

float4 PS_RenderRainTexture(rainVertexOutput IN): SV_TARGET
{
	float r=0;
	float2 t=IN.texCoords.xy;
	for(int i=0;i<NUM;i++)
	{
		r+=saturate(rand(frac(t.xy))-0.99)*12.0;
		t.y+=1.0/64.0;
	}
	r=saturate(r);
	float4 result=float4(r,r,r,r);
    return result;
}

float4 PS_RenderRandomTexture(rainVertexOutput IN): SV_TARGET
{
	float r=0;
    vec4 result=vec4(rand(IN.texCoords),rand(1.7*IN.texCoords),rand(0.11*IN.texCoords),rand(513.1*IN.texCoords));
	result=result*2.0-vec4(1.0,1.0,1.0,1.0);
    return result;
}

float4 PS_Overlay(rainVertexOutput IN) : SV_TARGET
{
	vec3 view				=normalize(mul(invViewProj,vec4(IN.clip_pos.xy,1.0,1.0)).xyz);
	
	vec2 depth_texc			=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	float depth				=texture_clamp(depthTexture,depth_texc).x;
	float dist				=depthToFadeDistance(depth,IN.clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);

	vec3 light				=cubeTexture.Sample(wrapSamplerState,-view).rgb;
	float br				=4.0;
	vec4 result				=vec4(light.rgb,0);
	vec2 texc				=vec2(atan2(view.x,view.y)/(2.0*pi)*5.0,view.z);
	float layer_distance	=nearRainDistance;
	float mult				=4.0;
	float step_range		=layer_distance;
	for(int i=0;i<3;i++)
	{
		vec2 layer_texc		=vec2(texc.x,texc.y+offset);
		vec4 r				=rainTexture.Sample(wrapSamplerState,layer_texc.xy);
		float a				=(br*(saturate(r.x+intensity-1.0)));
		a					*=saturate((dist-layer_distance)/step_range);
		texc				*=mult;
		layer_distance		*=mult;
		step_range			*=mult;
		br					*=0.5;
		result.a			+=a;
	}
	result.a=saturate(result.a);

	return result;
}

technique11 simul_rain
{
    pass p0 
    {
		SetRasterizerState(RenderNoCull);
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_FullScreen()));
		SetPixelShader(CompileShader(ps_4_0,PS_Overlay()));
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(AlphaBlend,float4(0.0f,0.0f,0.0f,0.0f),0xFFFFFFFF);
    }
}

technique11 simul_particles
{
    pass p0 
    {
		SetRasterizerState(RenderNoCull);
        SetGeometryShader(CompileShader(gs_4_0,GS_Particles()));
		SetVertexShader(CompileShader(vs_4_0,VS_Particles()));
		SetPixelShader(CompileShader(ps_4_0,PS_Particles()));
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(AlphaBlend,float4(0.0f,0.0f,0.0f,0.0f),0xFFFFFFFF);
    }
}

technique11 create_rain_texture
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_FullScreen()));
		SetPixelShader(CompileShader(ps_4_0,PS_RenderRainTexture()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}

technique11 create_random_texture
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_FullScreen()));
		SetPixelShader(CompileShader(ps_4_0,PS_RenderRandomTexture()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}


technique11 show_texture
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_ShowTexture()));
		SetPixelShader(CompileShader(ps_4_0,PS_ShowTexture()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}
