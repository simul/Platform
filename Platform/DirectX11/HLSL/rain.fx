#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/rain_constants.sl"
#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "../../CrossPlatform/depth.sl"
#include "../../CrossPlatform/noise.sl"
texture3D randomTexture3D;
// Same as transformedParticle, but with semantics
struct particleVertexOutput
{
    float4 position0		:SV_POSITION;
    float4 position1		:TEXCOORD2;
	float pointSize			:PSIZE;
	float brightness		:TEXCOORD0;
	vec3 view				:TEXCOORD1;
	float fade				:TEXCOORD3;
};

texture2D randomTexture;
TextureCube cubeTexture;
texture2D rainTexture;
texture2D showTexture;
// The RESOLVED depth texture at full resolution
texture2D depthTexture;

StructuredBuffer<vec3> positions;
RWStructuredBuffer<float> targetVertexBuffer;
RWStructuredBuffer<TransformedParticle> transformedParticlesRW;
StructuredBuffer<TransformedParticle> transformedParticles;

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

struct particleGeometryOutput
{
    float4 position			:SV_POSITION;
    float2 texCoords		:TEXCOORD0;
	float brightness		:TEXCOORD1;
	vec3 view				:TEXCOORD2;
	float fade				:TEXCOORD3;
};

vec3 Frac(vec3 pos,float scale)
{
	vec3 unity		=vec3(1.0,1.0,1.0);
	return scale*(2.0*frac(0.5*(pos/scale+unity))-unity);
}

vec3 Frac(vec3 pos,vec3 p1,float scale)
{
	vec3 unity	=vec3(1.0,1.0,1.0);
	vec3 p2		=scale*(2.0*frac(0.5*(p1/scale+unity))-unity);
	pos			+=p2-p1;
	return pos;
}

posTexVertexOutput VS_ShowTexture(idOnly id)
{
    return VS_ScreenQuad(id,rect);
}

vec4 PS_ShowTexture(posTexVertexOutput In): SV_TARGET
{
    return texture_wrap_lod(showTexture,In.texCoords,0);
}

void transf(out TransformedParticle p,in vec3 position,int i)
{
	vec3 particlePos	=position.xyz;
	particlePos			*=10.0;
	float sc			=1.0+0.7*rand3(position.xyz);
	vec3 pp1			=particlePos-viewPos[1].xyz+offset[1].xyz*sc;
	particlePos			-=viewPos[i].xyz;
	particlePos			+=offset[i].xyz*sc;
	particlePos			=Frac(particlePos,pp1,10.0);
	float ph			=flurryRate*phase;
	vec3 rand1			=randomTexture3D.SampleLevel(wrapSamplerState,particlePos/100.0,0).xyz;
	vec3 rand2			=randomTexture3D.SampleLevel(wrapSamplerState,particlePos/100.0*5.0,0).xyz;
	particlePos			+=2.5*flurry*rand1;
	particlePos			+=.7*flurry*rand2;
	p.position			=mul(worldViewProj[i],vec4(particlePos.xyz,1.0));
	p.view				=normalize(particlePos.xyz);
	p.pointSize			=snowSize*(1.0+0.4*rand2.y);
	p.brightness		=1.0;
	float dist			=length(particlePos.xyz-viewPos[1].xyz);
	p.fade				=saturate(1000.0/dist);///length(clip_pos-viewPos);
}

[numthreads(10,10,10)]
void CS_MakeVertexBuffer(uint3 idx	: SV_DispatchThreadID )
{
	vec3 r						=vec3(idx)/40.0;
	vec3 pos					=vec3(rand3(r),rand3(11.01*r),rand3(587.087*r));
	pos							*=2.0;
	pos							-=vec3(1.0,1.0,1.0);
	int i						=idx.z*1600+idx.y*40+idx.x;
	targetVertexBuffer[i*3]		=pos.x;
	targetVertexBuffer[i*3+1]	=pos.y;
	targetVertexBuffer[i*3+2]	=pos.z;
}

particleVertexOutput VS_Particles(posOnly IN)
{
	particleVertexOutput OUT;
	TransformedParticle p0;
	transf(p0,IN.position,0);
	TransformedParticle p1;
	transf(p1,IN.position,1);
	
    OUT.position0	=p0.position;
    OUT.position1	=p1.position;
	OUT.pointSize	=p1.pointSize;
	OUT.brightness	=p1.brightness;
	OUT.fade		=p1.fade;
	OUT.view		=p1.view;
	return OUT;
}

cbuffer cbImmutable
{
    float4 g_positions[4] : packoffset(c0) =
    {
        float4(-0.5,-0.5,0,0),
        float4( 0.5,-0.5,0,0),
        float4(-0.5, 0.5,0,0),
        float4( 0.5, 0.5,0,0),
    };
    float2 g_texcoords[4] : packoffset(c4) = 
    { 
        float2(0,0),
        float2(1,0), 
        float2(0,1),
        float2(1,1),
    };
};

[maxvertexcount(6)]
void GS_Particles(point particleVertexOutput input[1], inout TriangleStream<particleGeometryOutput> SpriteStream)
{
    particleGeometryOutput	output;
	// Emit four new triangles.

	// The two centres of the streak positions.
	vec4 pos1=input[0].position0;
	vec4 pos2=input[0].position1;
	
	if(pos1.y/pos1.w>pos2.y/pos2.w)
	{
		vec4 pos_temp=pos2;
		pos2=pos1;
		pos1=pos_temp;
	}
	float sz=input[0].pointSize;
	vec2 diff	=pos2.yx-pos1.yx;
	diff.y		=-diff.y;
	diff		=sz*diff/(0.0001+length(diff));

	output.brightness	=input[0].brightness;  
	output.fade			=input[0].fade;  
	output.view			=input[0].view;

	vec4 side			=vec4(diff/2.0,0,0);
#if 0
	{
		output.position		=pos1+side; 
		output.texCoords	=g_texcoords[0];
		SpriteStream.Append(output);
		output.position		=pos1-side; 
		output.texCoords	=g_texcoords[1];
		SpriteStream.Append(output);
		output.position		=pos2+side; 
		output.texCoords	=g_texcoords[2];
		SpriteStream.Append(output);
		output.position		=pos2-side; 
		output.texCoords	=g_texcoords[3];
		SpriteStream.Append(output);
	}
#else
	if(pos1.x/pos1.w<=pos2.x/pos2.w)
	{
		// bottom-left quadrant:
		output.position		=pos1+vec4(g_positions[0].xy*sz,0,0); 
		output.texCoords	=g_texcoords[0];
		SpriteStream.Append(output);
		output.position		=pos1+vec4(g_positions[1].xy*sz,0,0); 
		output.texCoords	=g_texcoords[1];
		SpriteStream.Append(output);
		output.position		=pos1+vec4(g_positions[2].xy*sz,0,0); 
		output.texCoords	=g_texcoords[2];
		SpriteStream.Append(output);
		output.position		=pos2+vec4(g_positions[1].xy*sz,0,0);  
		output.texCoords	=g_texcoords[1];
		SpriteStream.Append(output);
		output.position		=pos2+vec4(g_positions[2].xy*sz,0,0); 
		output.texCoords	=g_texcoords[2];
		SpriteStream.Append(output);
		output.position		=pos2+vec4(g_positions[3].xy*sz,0,0); 
		output.texCoords	=g_texcoords[3];
		SpriteStream.Append(output);
	}
	else
	{
		// bottom-left quadrant:
		output.position		=pos2+vec4(g_positions[2].xy*sz,0,0); 
		output.texCoords	=g_texcoords[2];
		SpriteStream.Append(output);
		output.position		=pos2+vec4(g_positions[3].xy*sz,0,0); 
		output.texCoords	=g_texcoords[3];
		SpriteStream.Append(output);
		output.position		=pos2+vec4(g_positions[0].xy*sz,0,0); 
		output.texCoords	=g_texcoords[0];
		SpriteStream.Append(output);
		output.position		=pos1+vec4(g_positions[3].xy*sz,0,0); 
		output.texCoords	=g_texcoords[3];
		SpriteStream.Append(output);
		output.position		=pos1+vec4(g_positions[0].xy*sz,0,0); 
		output.texCoords	=g_texcoords[0];
		SpriteStream.Append(output);
		output.position		=pos1+vec4(g_positions[1].xy*sz,0,0); 
		output.texCoords	=g_texcoords[1];
		SpriteStream.Append(output);
	}
#endif
    SpriteStream.RestartStrip();
}

vec4 PS_Particles(particleGeometryOutput IN): SV_TARGET
{
	vec4 result		=IN.brightness*cubeTexture.Sample(wrapSamplerState,-IN.view);
	vec2 pos		=IN.texCoords*2.0-1.0;
	float radius	=length(pos.xy);
	float angle		=atan2(pos.x,pos.y);
	//float spoke		=fract(angle/pi*3.0)-0.5;
	float opacity	=IN.fade*saturate(1.0-radius);//-spoke*spoke);
	return (vec4(result.rgb,opacity));
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
	OUT.position	=vec4(OUT.clip_pos,0.0,1.0);
	// Set to far plane
#ifdef REVERSE_DEPTH
	OUT.position.z	=0.0; 
#else
	OUT.position.z	=OUT.position.w; 
#endif
    OUT.texCoords	=0.5*(vec2(1.0,1.0)+vec2(OUT.clip_pos.x,-OUT.clip_pos.y));
//OUT.texCoords	+=0.5*texelOffsets;
	return OUT;
}

#define NUM (1)

float4 PS_RenderRainTexture(rainVertexOutput IN): SV_TARGET
{
	float r=0;
	vec2 t=IN.texCoords.xy;
	for(int i=0;i<NUM;i++)
	{
		r+=saturate(rand(frac(t.xy))-0.97)*12.0;
		t.y+=1.0/64.0;
	}
	r=saturate(r);
	float4 result=vec4(r,r,r,r);
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
	vec3 view				=normalize(mul(invViewProj[1],vec4(IN.clip_pos.xy,1.0,1.0)).xyz);
	
	vec2 depth_texc			=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	float depth				=texture_clamp(depthTexture,depth_texc).x;
	float dist				=depthToFadeDistance(depth,IN.clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);

	vec3 light				=cubeTexture.Sample(wrapSamplerState,-view).rgb;
	vec4 result				=vec4(light.rgb,0);
	vec2 texc				=vec2(atan2(view.x,view.y)/(2.0*pi)*5.0,tan(view.z*pi/2.0));
	float layer_distance	=nearRainDistance;
	float mult				=2.0;
	float step_range		=layer_distance;
	float br				=1.0-abs(view.z*view.z);
	for(int i=0;i<4;i++)
	{
		vec2 layer_texc		=vec2(texc.x,texc.y-.5*offset[1].z);
		vec4 r				=rainTexture.Sample(wrapSamplerState,layer_texc.xy);
		float a				=(br*(saturate(r.x+intensity-1.0)));
		//a					*=saturate(10000.0*(dist-layer_distance)/step_range);
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
		SetBlendState(AddAlphaBlend,float4(0.0f,0.0f,0.0f,0.0f),0xFFFFFFFF);
    }
}

// Transform to screen space.
technique11 make_vertex_buffer
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_MakeVertexBuffer()));
    }
}

technique11 simul_particles
{
    pass p0 
    {
		SetRasterizerState(RenderNoCull);
		//SetRasterizerState( wireframeRasterizer );
        SetGeometryShader(CompileShader(gs_4_0,GS_Particles()));
		SetVertexShader(CompileShader(vs_4_0,VS_Particles()));
		SetPixelShader(CompileShader(ps_4_0,PS_Particles()));
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(AddAlphaBlend,float4(0.0f,0.0f,0.0f,0.0f),0xFFFFFFFF);
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
