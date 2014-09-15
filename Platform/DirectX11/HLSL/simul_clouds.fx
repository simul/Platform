#include "CppHLSL.hlsl"
#include "states.hlsl"
Texture2D nearFarTexture : register(t3);
Texture2D cloudGodraysTexture;
Texture2D rainMapTexture;
TextureCube diffuseCubemap;
RWTexture2D<float> targetTexture1;
RWBuffer<float> occlusionBuffer;

#include "../../CrossPlatform/SL/simul_inscatter_fns.sl"
#include "../../CrossPlatform/SL/simul_cloud_constants.sl"
#include "../../CrossPlatform/SL/depth.sl"
#include "../../CrossPlatform/SL/simul_clouds.sl"
#include "../../CrossPlatform/SL/raytrace_new.sl"
#include "../../CrossPlatform/SL/states.sl"
#include "../../CrossPlatform/SL/earth_shadow_fade.sl"

#ifndef DETAIL_NOISE
	#define DETAIL_NOISE 1
#endif

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
	// Set to far plane so can use depth test as we want this geometry effectively at infinity
#if REVERSE_DEPTH==1
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=0.5*(vec2(1.0,1.0)+vec2(pos.x,-pos.y));
	return OUT;
}

RaytracePixelOutput PS_RaytraceForward(posTexVertexOutput IN)
{
	vec4 dlookup 			=texture_nearest_lod(depthTexture,viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias),0);
	vec2 texCoords			=IN.texCoords.xy;//mixedResTransformXYWH.xy+IN.texCoords.xy*mixedResTransformXYWH.zw;
	RaytracePixelOutput p	=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,true
									,dlookup
									,texCoords
									,false
									,true
									,false
									,true);
	if(texCoords.y>1.0)
		p.colour.r=1;
	if(p.colour.a>=1.0)
	   discard;
	return p;
}

RaytracePixelOutput PS_RaytraceBackground(posTexVertexOutput IN)
{
#if REVERSE_DEPTH==1
	vec4 dlookup			=vec4(0.0,0.0,0.0,0.0);
#else
	vec4 dlookup			=vec4(1.0,0.0,0.0,0.0);
#endif
	vec2 texCoords			=mixedResTransformXYWH.xy+IN.texCoords.xy*mixedResTransformXYWH.zw;
	RaytracePixelOutput p	=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,true
									,dlookup
									,texCoords
									,false
									,true
									,false
									,true);
	if(p.colour.a>=1.0)
	   discard;
	return p;
}

RaytracePixelOutput PS_RaytraceNearPass(posTexVertexOutput IN)
{
	vec4 dlookup 			=texture_nearest_lod(depthTexture,viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias),0);
	vec2 texCoords			=mixedResTransformXYWH.xy+IN.texCoords.xy*mixedResTransformXYWH.zw;
	RaytracePixelOutput p	=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,true
									,dlookup
									,texCoords
									,true
									,true
									,false
									,true);

	if(p.colour.a>=1.0)
	   discard;
	return p;
}

FarNearPixelOutput PS_RaytraceBothPasses(posTexVertexOutput IN)
{
	vec4 dlookup 			=texture_nearest_lod(depthTexture,viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias),0);
	vec2 texCoords			=mixedResTransformXYWH.xy+IN.texCoords.xy*mixedResTransformXYWH.zw;
	RaytracePixelOutput f	=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,true
									,dlookup
									,texCoords
									,false
									,true
									,false
									,true);
	RaytracePixelOutput n;
	if(dlookup.z>0)
	{
		n=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,true
									,dlookup
									,texCoords
									,true
									,true
									,false
									,true);
	}
	else
		n=f;
	if(f.colour.a>=1.0)
	   discard;
	FarNearPixelOutput fn;
	fn.farColour=f.colour;
	fn.nearColour=n.colour;
	fn.depth	=f.depth;
	return fn;
}

FarNearPixelOutput PS_RaytraceNoRainBothPasses(posTexVertexOutput IN)
{
	vec4 dlookup 			=texture_nearest_lod(depthTexture,viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias),0);
	vec2 texCoords			=mixedResTransformXYWH.xy+IN.texCoords.xy*mixedResTransformXYWH.zw;
	RaytracePixelOutput f	=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,true
									,dlookup
									,texCoords
									,false
									,true
									,false
									,false);
	RaytracePixelOutput n;
	if(dlookup.z>0)
	{
		n=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,true
									,dlookup
									,texCoords
									,true
									,true
									,false
									,false);
	}
	else
		n=f;
	if(f.colour.a>=1.0)
	   discard;
	FarNearPixelOutput fn;
	fn.farColour	=f.colour;
	fn.nearColour	=n.colour;
	fn.depth		=f.depth;
	return fn;
}
RaytracePixelOutput PS_RaytraceNoRainFar(posTexVertexOutput IN)
{
	vec4 dlookup 			=texture_nearest_lod(depthTexture,viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias),0);
	vec2 texCoords			=mixedResTransformXYWH.xy+IN.texCoords.xy*mixedResTransformXYWH.zw;
	RaytracePixelOutput f	=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,true
									,dlookup
									,texCoords
									,false
									,true
									,false
									,false);
	if(f.colour.a>=1.0)
	   discard;
	return f;
}
RaytracePixelOutput PS_RaytraceNoRainNear(posTexVertexOutput IN)
{
	vec4 dlookup 			=texture_nearest_lod(depthTexture,viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias),0);
	vec2 texCoords			=mixedResTransformXYWH.xy+IN.texCoords.xy*mixedResTransformXYWH.zw;
	if(dlookup.z<=0)
		discard;
	RaytracePixelOutput n=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,true
									,dlookup
									,texCoords
									,true
									,true
									,false
									,false);
	if(n.colour.a>=1.0)
	   discard;
	return n;
}


RaytracePixelOutput PS_RaytraceNoRainBackground(posTexVertexOutput IN)
{
#if REVERSE_DEPTH==1
	vec4 dlookup			=vec4(0.0,0.0,0.0,0.0);
#else
	vec4 dlookup			=vec4(1.0,0.0,0.0,0.0);
#endif
	vec2 texCoords			=mixedResTransformXYWH.xy+IN.texCoords.xy*mixedResTransformXYWH.zw;
	RaytracePixelOutput n=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,true
									,dlookup
									,texCoords
									,false
									,true
									,false
									,false);
	if(n.colour.a>=1.0)
	   discard;
	return n;
}


RaytracePixelOutput PS_RaytraceNew(posTexVertexOutput IN)
{
	vec2 texCoords			=IN.texCoords.xy;
	RaytracePixelOutput p	=RaytraceNew(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
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

RaytracePixelOutput PS_RaytraceNewNearPass(posTexVertexOutput IN)
{
	vec2 texCoords			=IN.texCoords.xy;
	RaytracePixelOutput p	=RaytraceNew(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
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

FarNearPixelOutput PS_RaytraceNewBothPasses(posTexVertexOutput IN)
{
	vec2 texCoords			=IN.texCoords.xy;
	RaytracePixelOutput f	=RaytraceNew(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,depthTexture
									,lightTableTexture
									,true
									,texCoords
									,true
									,true
									,false);
	RaytracePixelOutput n	=RaytraceNew(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,depthTexture
									,lightTableTexture
									,true
									,texCoords
									,true
									,true
									,false);
	f.colour.r=0.0;
	f.colour.a=0.5;
	if(f.colour.a>=1.0)
	   discard;
	FarNearPixelOutput fn;
	fn.farColour	=f.colour;
	fn.nearColour	=n.colour;
	fn.depth		=f.depth;
	return fn;
}

// Given texture position from texCoords, convert to a worldpos with shadowMatrix.
// Then, trace towards sun to find initial intersection with cloud volume
// Then trace down to find first intersection with clouds, if any.
vec4 PS_CloudShadow( posTexVertexOutput IN):SV_TARGET
{
	return CloudShadow(cloudDensity1,cloudDensity2,IN.texCoords,shadowMatrix,cornerPos,inverseScales);
}

vec4 PS_RainMap(posTexVertexOutput IN) : SV_TARGET
{
	float r=MakeRainMap(cloudDensity1,cloudDensity2,cloud_interp,IN.texCoords);
	return vec4(r,r,r,r);
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
float CloudsOcclusion(Texture3D cloudDensity1,Texture3D cloudDensity2,float cloud_interp,vec3 viewPos,vec3 directionToSun)
{
	return 0.f;
}
[numthreads(1,1,1)]
void CS_Occlusion(uint3 idx: SV_DispatchThreadID)
{
	occlusionBuffer[idx.x]=CloudsOcclusion(cloudDensity1,cloudDensity2,cloud_interp,viewPos,directionToSun);
}


vec4 PS_SimpleRaytrace(posTexVertexOutput IN) : SV_TARGET
{
	vec4 dlookup 			=texture_nearest_lod(depthTexture,viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias),0);
	vec2 texCoords			=mixedResTransformXYWH.xy+IN.texCoords.xy*mixedResTransformXYWH.zw;
	RaytracePixelOutput p	=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,false
									,dlookup
									,texCoords
									,false
									,false
									,false
									,false);
	if(p.colour.a>=1.0)
	   discard;
	return p.colour;
}

RaytracePixelOutput PS_Raytrace3DNoise(posTexVertexOutput IN)
{
	vec4 dlookup 			=texture_nearest_lod(depthTexture,viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias),0);
	vec2 texCoords			=mixedResTransformXYWH.xy+IN.texCoords.xy*mixedResTransformXYWH.zw;
	RaytracePixelOutput r	=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,true
									,dlookup
									,texCoords
									,false
									,true
									,true
									,true);
	if(r.colour.a>=1.0)
	   discard;
	return r;
}

RaytracePixelOutput PS_Raytrace3DNoiseNearPass(posTexVertexOutput IN)
{
	vec4 dlookup 			=texture_nearest_lod(depthTexture,viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias),0);
	vec2 texCoords			=mixedResTransformXYWH.xy+IN.texCoords.xy*mixedResTransformXYWH.zw;
	RaytracePixelOutput r	=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,true
									,dlookup
									,texCoords
									,true
									,true
									,true
									,true);
	if(r.colour.a>=1.0)
	   discard;
	return r;
}

FarNearPixelOutput PS_Raytrace3DNoiseBothPasses(posTexVertexOutput IN)
{
	vec4 dlookup 			=texture_nearest_lod(depthTexture,viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias),0);
	vec2 texCoords			=mixedResTransformXYWH.xy+IN.texCoords.xy*mixedResTransformXYWH.zw;
	RaytracePixelOutput f	=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,true
									,dlookup
									,texCoords
									,false
									,true
									,true
									,true);
	RaytracePixelOutput n;
	if(dlookup.z>0)
	{
		n	=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,true
									,dlookup
									,texCoords
									,true
									,true
									,true
									,true);
		//n.colour.a=.4;
		//n.colour.r=1;
	}
	else
		n=f;
	if(f.colour.a>=1.0)
	   discard;
	FarNearPixelOutput fn;
	fn.farColour=f.colour;
	fn.nearColour=n.colour;
	fn.depth	=f.depth;
	return fn;
}


RaytracePixelOutput PS_Raytrace3DNoiseBackground(posTexVertexOutput IN)
{
#if REVERSE_DEPTH==1
	vec4 dlookup			=vec4(0.0,0.0,0.0,0.0);
#else
	vec4 dlookup			=vec4(1.0,0.0,0.0,0.0);
#endif
	vec2 texCoords			=mixedResTransformXYWH.xy+IN.texCoords.xy*mixedResTransformXYWH.zw;
	RaytracePixelOutput r	=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,true
									,dlookup
									,texCoords
									,false
									,true
									,true
									,true);
	if(r.colour.a>=1.0)
	   discard;
	return r;
}


RaytracePixelOutput NoRain3DNoiseNear(vec4 dlookup,vec2 texCoords)
{
	RaytracePixelOutput n	=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,true
									,dlookup
									,texCoords
							,true
									,true
									,true
									,false);
	return n;
}

RaytracePixelOutput PS_Raytrace3DNoiseNoRainNear(posTexVertexOutput IN)
{
	vec4 dlookup 			=texture_nearest_lod(depthTexture,viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias),0);
	if(dlookup.z<=0)
		discard;
	vec2 texCoords			=mixedResTransformXYWH.xy+IN.texCoords.xy*mixedResTransformXYWH.zw;
	RaytracePixelOutput n	=NoRain3DNoiseNear(dlookup,texCoords);
	return n;
}

RaytracePixelOutput NoRain3DNoiseFar(vec4 dlookup,vec2 texCoords)
	{
	texCoords				=mixedResTransformXYWH.xy+texCoords.xy*mixedResTransformXYWH.zw;
	RaytracePixelOutput f	=RaytraceCloudsForward(
									cloudDensity1
									,cloudDensity2
									,rainMapTexture
									,noiseTexture
									,noiseTexture3D
									,lightTableTexture
									,true
									,dlookup
									,texCoords
								,false
									,true
									,true
									,false);
	return f;
}

RaytracePixelOutput PS_Raytrace3DNoiseNoRainFar(posTexVertexOutput IN)
{
	vec4 dlookup 			=texture_nearest_lod(depthTexture,viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias),0);
	vec2 texCoords			=mixedResTransformXYWH.xy+IN.texCoords.xy*mixedResTransformXYWH.zw;
	RaytracePixelOutput f	=NoRain3DNoiseFar(dlookup,texCoords);
	return f;
}

FarNearPixelOutput PS_Raytrace3DNoiseNoRainBothPasses(posTexVertexOutput IN)
{
	vec4 dlookup 			=texture_nearest_lod(depthTexture,viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias),0);
	vec2 texCoords			=mixedResTransformXYWH.xy+IN.texCoords.xy*mixedResTransformXYWH.zw;
	RaytracePixelOutput f	=NoRain3DNoiseFar(dlookup,texCoords);
	RaytracePixelOutput n;
	if(dlookup.z>0)
	{
		n	=NoRain3DNoiseNear(dlookup,texCoords);
	}
	else
		n=f;
	if(f.colour.a>=1.0)
	   discard;
	FarNearPixelOutput fn;
	fn.farColour=f.colour;
	fn.nearColour=n.colour;
	fn.depth	=f.depth;
	return fn;
}

RaytracePixelOutput PS_Raytrace3DNoiseNoRainBackground(posTexVertexOutput IN)
{
#if REVERSE_DEPTH==1
	vec4 dlookup			=vec4(0.0,0.0,0.0,0.0);
#else
	vec4 dlookup			=vec4(1.0,0.0,0.0,0.0);
#endif
	RaytracePixelOutput f	=NoRain3DNoiseFar(dlookup,IN.texCoords);
	return f;
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

#define CROSS_SECTION_STEPS 32
vec4 CrossSection(vec2 texCoords,float yz)
{
	vec3 texc=crossSectionOffset+vec3(texCoords.x,yz*texCoords.y,(1.0-yz)*texCoords.y);
	int i=0;
	vec3 accum=vec3(0.0,0.5,1.0);
	texc.y+=0.5*(1.0-yz)/(float)CROSS_SECTION_STEPS;
	texc.z+=0.5*yz/(float)CROSS_SECTION_STEPS;
	for(i=0;i<CROSS_SECTION_STEPS;i++)
	{
		vec4 density=texture_wwc(cloudDensity1,texc);
		vec3 colour=vec3(.5,.5,.5)*(lightResponse.x*density.y+lightResponse.y*density.x);
		colour.gb+=vec2(.125,.25)*(lightResponse.z*density.w);
		float opacity=density.z;
		colour*=opacity;
		accum*=1.0-opacity;
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

BlendState Blend1
{
	BlendEnable[0] = TRUE;
	BlendEnable[1] = FALSE;
	SrcBlend = ONE;
	DestBlend = SRC_ALPHA;
    BlendOp = ADD;
    SrcBlendAlpha = ZERO;
    DestBlendAlpha = SRC_ALPHA;
    BlendOpAlpha = ADD;
    //RenderTargetWriteMask[0] = 0x0F;
};

fxgroup raytrace
{
	technique11 full
	{
		pass far 
		{
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_RaytraceForward()));
		}
		pass near 
		{
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_RaytraceNearPass()));
		}
		pass both 
		{
			SetBlendState(Blend1,vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_RaytraceBothPasses()));
		}
		pass background 
		{
			SetBlendState(Blend1,vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_RaytraceBackground()));
		}
	}
	technique11 no_rain
	{
		pass far 
		{
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_RaytraceNoRainFar()));
		}
		pass near 
		{
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_RaytraceNoRainNear()));
		}
		pass both 
		{
			SetBlendState(Blend1,vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_RaytraceNoRainBothPasses()));
		}
		pass background 
		{
			SetBlendState(Blend1,vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_RaytraceNoRainBackground()));
		}
	}
	technique11 nonwrapping
	{
		pass far 
		{
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_RaytraceNew()));
		}
		pass near 
		{
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_RaytraceNewNearPass()));
		}
		pass both 
		{
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_RaytraceNewBothPasses()));
		}
		pass background 
		{
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_RaytraceNew()));
		}
	}
	technique11 simple
	{
		pass far 
		{
			SetDepthStencilState(DisableDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_SimpleRaytrace()));
		}
		pass background 
		{
			SetDepthStencilState(DisableDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_SimpleRaytrace()));
		}
	}
	technique11 noise3d
	{
		pass far 
		{
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_Raytrace3DNoise()));
		}
		pass near 
		{
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_Raytrace3DNoiseNearPass()));
		}
		pass both 
		{
			SetBlendState(Blend1,vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_Raytrace3DNoiseBothPasses()));
		}
		pass background 
		{
			SetBlendState(Blend1,vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_Raytrace3DNoiseBackground()));
		}
	}
	technique11 noise3d_no_rain
	{
		pass far 
		{
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_Raytrace3DNoiseNoRainFar()));
		}
		pass near 
		{
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_Raytrace3DNoiseNoRainNear()));
		}
		pass both 
		{
			SetBlendState(Blend1,vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_Raytrace3DNoiseNoRainBothPasses()));
		}
		pass background 
		{
			SetBlendState(Blend1,vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
			SetDepthStencilState(WriteDepth,0);
			SetRasterizerState( RenderNoCull );
			SetVertexShader(CompileShader(vs_5_0,VS_FullScreen()));
			SetPixelShader(CompileShader(ps_5_0,PS_Raytrace3DNoiseNoRainBackground()));
		}
	}
}

technique11 cloud_shadow
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetBlendState(NoBlend,vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_FullScreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_CloudShadow()));
    }
}


technique11 rain_map
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetBlendState(NoBlend,vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_FullScreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_RainMap()));
    }
}

technique11 godrays_accumulation
{
    pass p0
    {
		SetComputeShader(CompileShader(cs_5_0,CS_GodraysAccumulation()));
    }
}
technique11 occlusion
{
    pass p0
    {
		SetComputeShader(CompileShader(cs_5_0,CS_Occlusion()));
    }
}

technique11 moisture_accumulation
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
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
		SetBlendState(NoBlend,vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
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
		SetBlendState(DontBlend, vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
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
		SetBlendState(DontBlend, vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
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
		SetBlendState(DontBlend, vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_CrossSection()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_ShowShadow()));
    }
}