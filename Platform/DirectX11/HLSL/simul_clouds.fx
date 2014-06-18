#include "CppHLSL.hlsl"
#include "states.hlsl"
Texture2D nearFarTexture	: register(t3);
Texture2D cloudGodraysTexture;
RWTexture2D<float> targetTexture1;
#include "../../CrossPlatform/SL/simul_inscatter_fns.sl"
#include "../../CrossPlatform/SL/simul_cloud_constants.sl"
#include "../../CrossPlatform/SL/depth.sl"
#include "../../CrossPlatform/SL/simul_clouds.sl"
#include "../../CrossPlatform/SL/states.sl"
#include "../../CrossPlatform/SL/earth_shadow_fade.sl"

StructuredBuffer<SmallLayerData> layersSB;

#ifndef DETAIL_NOISE
	#define DETAIL_NOISE 1
#endif

SamplerState lightningSamplerState
{
    Texture = <lightningIlluminationTexture>;
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Border;
	AddressV = Border;
	AddressW = Border;
};

struct RaytraceVertexOutput
{
    vec4 hPosition		: SV_POSITION;
	vec2 texCoords		: TEXCOORD0;
};

RaytraceVertexOutput VS_Raytrace(idOnly IN)
{
    RaytraceVertexOutput OUT;
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
#if REVERSE_DEPTH==1
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=0.5*(vec2(1.0,1.0)+vec2(pos.x,-pos.y));
	// transform to coordinates corresponding to the high-res buffer - when buffers are misaligned.
//	OUT.texCoords.xy			*=mixedResTransformXYWH.zw;
//	OUT.texCoords.xy			+=mixedResTransformXYWH.xy;
	return OUT;
}

RaytracePixelOutput PS_RaytraceForward(RaytraceVertexOutput IN)
{
	vec2 texCoords			=IN.texCoords.xy;
	//texCoords.y				=1.0-texCoords.y;
	// Now y is positive downwards.
	RaytracePixelOutput p	=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,noiseTexture
									,noiseTexture3D
									,depthTexture
									,lightTableTexture
									,true
									,texCoords
									,false
									,true
									,false);
	
	return p;
}

RaytracePixelOutput PS_RaytraceNearPass(RaytraceVertexOutput IN)
{
	vec2 texCoords			=IN.texCoords.xy;
	//texCoords.y				=1.0-texCoords.y;
	RaytracePixelOutput p	=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,noiseTexture
									,noiseTexture3D
									,depthTexture
									,lightTableTexture
									,true
									,texCoords
									,true
									,true
									,false);

	return p;
}

struct vertexInputCS
{
    vec4 position		: POSITION;
    vec2 texCoords		: TEXCOORD0;
};

// Given texture position from texCoords, convert to a worldpos with shadowMatrix.
// Then, trace towards sun to find initial intersection with cloud volume
// Then trace down to find first intersection with clouds, if any.
vec4 PS_CloudShadow( posTexVertexOutput IN):SV_TARGET
{
	return CloudShadow(cloudDensity1,cloudDensity2,IN.texCoords,shadowMatrix,cornerPos,inverseScales);
}

[numthreads(1,1,1)]
void CS_GodraysAccumulation(uint3 idx: SV_DispatchThreadID)
{
	GodraysAccumulation(targetTexture1,cloudShadowTexture,idx.x);
}

vec4 PS_MoistureAccumulation( posTexVertexOutput IN):SV_TARGET
{
	float m=MoistureAccumulation(cloudShadowTexture,shadowTextureSize,IN.texCoords);
	return vec4(m,m,m,m);
}

vec4 PS_SimpleRaytrace(RaytraceVertexOutput IN) : SV_TARGET
{
	vec2 texCoords		=IN.texCoords.xy;
	texCoords.y			=1.0-texCoords.y;
	vec4 r				=RaytraceCloudsForward(cloudDensity1,cloudDensity2,noiseTexture
									,noiseTexture3D,depthTexture,lightTableTexture,true,texCoords,false,false,false).colour;
	return r;
}

RaytracePixelOutput PS_Raytrace3DNoise(RaytraceVertexOutput IN)
{
	vec2 texCoords			=IN.texCoords.xy;
	texCoords.y				=1.0-texCoords.y;
	RaytracePixelOutput r	=RaytraceCloudsForward(cloudDensity1,cloudDensity2
									,noiseTexture
									,noiseTexture3D
									,depthTexture
									,lightTableTexture
									,true
									,texCoords
									,true
									,true
									,true);
	return r;
}

posTexVertexOutput VS_FullScreen(idOnly IN)
{
	posTexVertexOutput OUT;
	vec2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	vec2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=vec4(pos,0.0,1.0);
	// Set to far plane
#if REVERSE_DEPTH==1
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=0.5*(vec2(1.0,1.0)+vec2(pos.x,-pos.y));
//OUT.texCoords	+=0.5*texelOffsets;
	return OUT;
}

posTexVertexOutput VS_CrossSection(idOnly IN)
{
    posTexVertexOutput OUT;
	vec2 poss[4]=
	{
		{ 1.0, 0.0},
		{ 1.0, 1.0},
		{ 0.0, 0.0},
		{ 0.0, 1.0},
	};
	vec2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=vec4(rect.xy+rect.zw*pos,0.0,1.0);
	//OUT.hPosition	=vec4(pos,0.0,1.0);
	// Set to far plane so can use depth test as we want this geometry effectively at infinity
#if REVERSE_DEPTH==1
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=pos;
    return OUT;
}

vec4 PS_Simple( posTexVertexOutput IN):SV_TARGET
{
    return noiseTexture.Sample(wrapSamplerState,IN.texCoords.xy);
}

vec4 PS_ShowNoise( posTexVertexOutput IN):SV_TARGET
{
    vec4 lookup=noiseTexture.Sample(wrapSamplerState,IN.texCoords.xy);
	return vec4(0.5*(lookup.rgb+1.0),1.0);
}

vec4 PS_ShowShadow( posTexVertexOutput IN):SV_TARGET
{
	return ShowCloudShadow(cloudShadowTexture,cloudGodraysTexture,IN.texCoords);
}

SamplerState crossSectionSamplerState
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Clamp;
};

#define CROSS_SECTION_STEPS 32
vec4 CrossSection(vec2 texCoords,float yz)
{
	vec3 texc=crossSectionOffset+vec3(texCoords.x,yz*texCoords.y,(1.0-yz)*texCoords.y);
	int i=0;
	vec3 accum=vec3(0.f,0.5f,1.f);
	texc.y+=0.5*(1.0-yz)/(float)CROSS_SECTION_STEPS;
	texc.z+=0.5*yz/(float)CROSS_SECTION_STEPS;
	for(i=0;i<CROSS_SECTION_STEPS;i++)
	{
		vec4 density=texture_wwc(cloudDensity1,texc);
		vec3 colour=vec3(.5,.5,.5)*(lightResponse.x*density.y+lightResponse.y*density.x);
		colour.gb+=vec2(.125,.25)*(lightResponse.z*density.w);
		float opacity=density.z;
		colour*=opacity;
		accum*=1.f-opacity;
		accum+=colour;
		texc.y-=(1.0-yz)/(float)CROSS_SECTION_STEPS;
		texc.z+=yz/(float)CROSS_SECTION_STEPS;
	}
    return vec4(accum,1);
}

vec4 PS_CrossSection( posTexVertexOutput IN):SV_TARGET
{
    return CrossSection(IN.texCoords,yz);
}


technique11 simul_raytrace_forward
{
    pass p0 
    {
		SetDepthStencilState(WriteDepth,0);
        SetRasterizerState( RenderNoCull );
		SetVertexShader(CompileShader(vs_5_0,VS_Raytrace()));
		SetPixelShader(CompileShader(ps_5_0,PS_RaytraceForward()));
    }
}

technique11 raytrace_near_pass
{
    pass p0 
    {
		SetDepthStencilState(WriteDepth,0);
        SetRasterizerState( RenderNoCull );
		SetVertexShader(CompileShader(vs_5_0,VS_Raytrace()));
		SetPixelShader(CompileShader(ps_5_0,PS_RaytraceNearPass()));
    }
}

technique11 simul_simple_raytrace
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetVertexShader(CompileShader(vs_5_0,VS_Raytrace()));
		SetPixelShader(CompileShader(ps_5_0,PS_SimpleRaytrace()));
    }
}

technique11 simul_raytrace_3d_noise
{
    pass p0 
    {
		SetDepthStencilState(WriteDepth,0);
        SetRasterizerState( RenderNoCull );
		SetVertexShader(CompileShader(vs_5_0,VS_Raytrace()));
		SetPixelShader(CompileShader(ps_5_0,PS_Raytrace3DNoise()));
    }
}


technique11 cloud_shadow
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetBlendState(NoBlend,vec4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_FullScreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_CloudShadow()));
    }
}

technique11 godrays_accumulation
{
    pass p0
    {
		SetComputeShader(CompileShader(cs_5_0,CS_GodraysAccumulation()));
    }
}

technique11 moisture_accumulation
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_FullScreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_MoistureAccumulation()));
    }
}

technique11 cross_section
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetBlendState(NoBlend,vec4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_CrossSection()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_CrossSection()));
    }
}

technique11 simple
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_CrossSection()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Simple()));
    }
}

technique11 show_noise
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_CrossSection()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_ShowNoise()));
    }
}

technique11 show_shadow
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_CrossSection()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_ShowShadow()));
    }
}