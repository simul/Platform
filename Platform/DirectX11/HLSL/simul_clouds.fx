#include "CppHLSL.hlsl"
#include "states.hlsl"
Texture2D nearFarTexture	: register(t3);
#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "../../CrossPlatform/simul_cloud_constants.sl"
#include "../../CrossPlatform/depth.sl"
#include "../../CrossPlatform/simul_clouds.sl"
#include "../../CrossPlatform/states.sl"
#include "../../CrossPlatform/earth_shadow_fade.sl"

StructuredBuffer<SmallLayerData> layersSB;

#define Z_VERTICAL 1
#ifndef WRAP_CLOUDS
	#define WRAP_CLOUDS 1
#endif
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
#ifdef REVERSE_DEPTH
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=0.5*(vec2(1.0,1.0)+vec2(pos.x,pos.y));
	return OUT;
}
RaytracePixelOutput PS_Raytrace(RaytraceVertexOutput IN)
{
	vec2 texCoords		=IN.texCoords.xy;
	texCoords.y		=1.0-texCoords.y;
	float dlookup		=sampleLod(depthTexture,clampSamplerState,viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias),0).r;
	vec4 clip_pos		=vec4(-1.f,-1.f,1.f,1.f);
	clip_pos.x		+=2.f*IN.texCoords.x;
	clip_pos.y		+=2.f*IN.texCoords.y;
	vec3 view		=normalize(mul(invViewProj,clip_pos).xyz);
	float cos0		=dot(lightDir.xyz,view.xyz);
	float sine		=view.z;
	vec3 n			=vec3(clip_pos.xy*tanHalfFov,1.0);
	n					=normalize(n);
	vec2 noise_texc_0	=mul(noiseMatrix,n.xy);

	float min_texc_z	=-fractalScale.z*1.5;
	float max_texc_z	=1.0-min_texc_z;

	float depth			=dlookup.r;
	float d				=depthToFadeDistance(depth,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
	vec4 colour			=vec4(0.0,0.0,0.0,1.0);
	vec2 fade_texc		=vec2(0.0,0.5*(1.0-sine));

	// Lookup in the illumination texture.
	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec4 illum_lookup	=texture_wrap_mirror(illuminationTexture,illum_texc);
	vec2 nearFarTexc	=illum_lookup.xy;

	float mean_z		=1.0;

	// This provides the range of texcoords that is lit.
	for(int i=0;i<layerCount;i++)
	//int i=40;
	{
		vec4 density			=vec4(0,0,0,0);
		const LayerData layer		=layers[i];
		float layerDist			=layer.layerDistance;
		float normLayerZ		=saturate(layerDist/maxFadeDistanceMetres);
		vec3 layerPos			=viewPos+layerDist*view;
		layerPos.z			-=layer.verticalShift;
		vec4 layerTexCoords;
		layerTexCoords.xyz		=(layerPos-cornerPos)*inverseScales;
		layerTexCoords.w		=0.2+0.8*saturate(layerTexCoords.z);
		//if(texCoords.z>max_texc_z)
		//	break;
		if(normLayerZ<=d&&layerTexCoords.z>=min_texc_z&&layerTexCoords.z<=max_texc_z)
		{
			vec2 noise_texc	=noise_texc_0*layerDist/fractalRepeatLength+layer.noiseOffset;
			vec3 noiseval		=layerTexCoords.w*noiseTexture.SampleLevel(wrapSamplerState,noise_texc.xy,0).xyz;
			density				=calcDensity(layerTexCoords.xyz,layer.layerFade,noiseval,fractalScale,cloud_interp);
		}
		if(density.z>0)
		{
			vec4 c	=calcColour(density,cos0,layerTexCoords.z,lightResponse,ambientColour);
			fade_texc.x	=sqrt(normLayerZ);
			float sh	=saturate((fade_texc.x-nearFarTexc.x)/0.1);
			// overcast effect:
			//sh		*=saturate(illum_lookup.z+layerTexCoords.z);
			c.rgb		*=sh;
			c.rgb		=applyFades(c.rgb,fade_texc,cos0,sh);
			colour		*=1.0-c.a;
			colour.rgb	+=c.rgb*c.a;
			// depth here:
			mean_z=lerp(mean_z,normLayerZ,depthMix*density.z);
		}
	}
	if(colour.a>=1.0)
	   discard;
	RaytracePixelOutput res;
    res.colour		=vec4(exposure*colour.rgb,colour.a);
	res.depth		=fadeDistanceToDepth(mean_z,depthToLinFadeDistParams,nearZ,farZ,clip_pos.xy,tanHalfFov);
	return res;
}

RaytracePixelOutput PS_RaytraceForward(RaytraceVertexOutput IN)
{
	vec2 texCoords		=IN.texCoords.xy;
	texCoords.y			=1.0-texCoords.y;
	RaytracePixelOutput p=RaytraceCloudsForward(cloudDensity1,cloudDensity2,noiseTexture,depthTexture,texCoords);

	return p;
}

struct vertexInputCS
{
    vec4 position			: POSITION;
    vec2 texCoords		: TEXCOORD0;
};

struct vertexOutputCS
{
    vec4 hPosition		: SV_POSITION;
    vec2 texCoords		: TEXCOORD0;
};

// Given texture position from texCoords, convert to a worldpos with shadowMatrix.
// Then, trace towards sun to find initial intersection with cloud volume
// Then trace down to find first intersection with clouds, if any.
vec4 PS_CloudShadow( vertexOutputCS IN):SV_TARGET
{
	return CloudShadow(cloudDensity1,cloudDensity2,IN.texCoords,shadowMatrix,cornerPos,inverseScales);
}

vec4 PS_ShadowNearFar( vertexOutputCS IN):SV_TARGET
{
	return CloudShadowNearFar(cloudShadowTexture,shadowTextureSize,IN.texCoords);
}

vec4 PS_SimpleRaytrace(RaytraceVertexOutput IN) : SV_TARGET
{
	vec2 texCoords		=IN.texCoords.xy;
	texCoords.y			=1.0-texCoords.y;
	vec4 r				=SimpleRaytraceCloudsForward(cloudDensity1,cloudDensity2,depthTexture,texCoords);
	return r;
}

vec4 PS_Raytrace3DNoise(RaytraceVertexOutput IN) : SV_TARGET
{
	vec2 texCoords		=IN.texCoords.xy;
	texCoords.y			=1.0-texCoords.y;
	vec4 dlookup		=sampleLod(depthTexture,clampSamplerState,viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias),0);
	vec4 pos			=vec4(-1.f,-1.f,1.f,1.f);
	pos.x				+=2.f*IN.texCoords.x;
	pos.y				+=2.f*IN.texCoords.y;
	vec3 view			=normalize(mul(invViewProj,pos).xyz);
	float cos0			=dot(lightDir.xyz,view.xyz);
	float sine			=view.z;
	vec3 n				=vec3(pos.xy*tanHalfFov,1.0);
	n					=normalize(n);
	vec2 noise_texc_0	=mul(noiseMatrix,n.xy);

	float min_texc_z	=-fractalScale.z*1.5;
	float max_texc_z	=1.0-min_texc_z;

	float depth			=dlookup.r;
	float d				=depthToFadeDistance(depth,pos.xy,depthToLinFadeDistParams,tanHalfFov);
	vec4 colour			=vec4(0.0,0.0,0.0,1.0);
	vec2 fade_texc		=vec2(0.0,0.5*(1.0-sine));

	// Lookup in the illumination texture.
	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec4 illum_lookup	=texture_wrap_mirror(illuminationTexture,illum_texc);
	vec2 nearFarTexc	=illum_lookup.xy;

	float mean_z		=1.0;
	float up			=step(0.1,sine);
	float down			=step(0.1,-sine);
	// Precalculate hg effects
	float hg_clouds		=HenyeyGreenstein(cloudEccentricity,cos0);
	float BetaRayleigh	=0.0596831*(1.0+cos0*cos0);
	float BetaMie		=HenyeyGreenstein(hazeEccentricity,cos0);
	// This provides the range of texcoords that is lit.
	for(int i=0;i<layerCount;i++)
	{
		vec4 density=vec4(0,0,0,0);
		const LayerData layer=layers[i];
		float dist=layer.layerDistance;
		float z=saturate(dist/maxFadeDistanceMetres);
		vec3 pos=viewPos+dist*view;
		pos.z-=layer.verticalShift;
		vec3 texCoords=(pos-cornerPos)*inverseScales;
	/*	if((texCoords.z-max_texc_z)*up>0.1)
			break;
		if((min_texc_z-texCoords.z)*down>0.1)
			break;*/
		if(z<=d&&texCoords.z>=min_texc_z&&texCoords.z<=max_texc_z)
		{
				vec3 noise_texc	=texCoords.xyz*noise3DTexcoordScale;
			float noise_factor	=0.2+0.8*saturate(texCoords.z);
			vec3 noiseval		=vec3(0.0,0.0,0.0);
			float mul=0.5;
			for(int j=0;j<2;j++)
			{
				noiseval		+=(noiseTexture3D.SampleLevel(wrapSamplerState,noise_texc,0).xyz)*mul;
				noise_texc		*=2.0;
				mul				*=noise3DPersistence;
			}
			noiseval			*=noise_factor;
			density				=calcDensity(texCoords.xyz,layer.layerFade,noiseval,fractalScale,cloud_interp);
		}
		if(density.z>0)
		{
			float brightness_factor=unshadowedBrightness(hg_clouds,texCoords.z,lightResponse);
			vec4 c	=calcColour2( density,hg_clouds,texCoords.z,lightResponse,ambientColour);
			
			fade_texc.x	=sqrt(z);
			float sh	=saturate((fade_texc.x-nearFarTexc.x)/0.1);
			c.rgb		*=sh;
			c.rgb		=applyFades2(c.rgb,fade_texc,BetaRayleigh,BetaMie,sh);
			colour.rgb	+=c.rgb*c.a*(colour.a);
			colour.a	*=(1.0-c.a);
			if(colour.a*brightness_factor<0.0001)
			{
				colour.a=0.0;
				mean_z=lerp(mean_z,z,depthMix);
				break;
			}
			// depth here:
			mean_z=lerp(mean_z,z,depthMix*density.z);
		}
		
	}
	if(colour.a>=1.0)
	   discard;
	RaytracePixelOutput res;
    res.colour		=vec4(exposure*colour.rgb,1.0-colour.a);
	res.depth		=fadeDistanceToDepth(mean_z,depthToLinFadeDistParams,nearZ,farZ,pos.xy,tanHalfFov);
	return res.colour;
}

struct vertexInput
{
    vec3 position			: POSITION;
	// Per-instance data:
	vec2 noiseOffset		: TEXCOORD0;
	vec2 elevationRange		: TEXCOORD1;
	float noiseScale		: TEXCOORD2;
	float layerFade			: TEXCOORD3;
	float layerDistance		: TEXCOORD4;
};


struct toPS
{
    vec4 hPosition		: SV_POSITION;
    vec2 noise_texc		: TEXCOORD0;
    vec4 texCoords		: TEXCOORD1;
	vec3 view				: TEXCOORD2;
    vec3 texCoordLightning: TEXCOORD3;
    vec2 fade_texc		: TEXCOORD4;
	float layerFade			: TEXCOORD5;
};

toPS VS_Main(vertexInput IN)
{
    toPS OUT;
	vec3 pos				=IN.position;
	if(pos.z<IN.elevationRange.x||pos.z>IN.elevationRange.y)
		pos*=0.f;
	OUT.view				=normalize(pos.xyz);
	float sine				=OUT.view.z;
	vec3 t1					=pos.xyz*IN.layerDistance;
	OUT.hPosition			=vec4(t1.xyz,1.0);
	vec2 noise_pos			=mul(noiseMatrix,pos.xy);
	OUT.noise_texc			=vec2(atan2(noise_pos.x,1.0),atan2(noise_pos.y,1.0));
	OUT.noise_texc			*=IN.noiseScale;
	OUT.noise_texc			+=IN.noiseOffset;

	vec3 wPos				=t1+viewPos;
	OUT.texCoords.xyz		=wPos-cornerPos;
	OUT.texCoords.xyz		*=inverseScales;
	OUT.texCoords.w			=saturate(OUT.texCoords.z);
	vec3 texCoordLightning	=(pos.xyz-illuminationOrigin.xyz)/illuminationScales.xyz;
	OUT.texCoordLightning	=texCoordLightning;
	
	float depth				=IN.layerDistance/maxFadeDistanceMetres;
		
	OUT.fade_texc			=vec2(sqrt(depth),0.5f*(1.f-sine));
	OUT.layerFade			=IN.layerFade;
    return OUT;
}
#define ELEV_STEPS 20

vec4 PS_Clouds( toPS IN): SV_TARGET
{
	vec3 noiseval=(noiseTexture.Sample(wrapSamplerState,IN.noise_texc.xy).xyz).xyz;
#ifdef DETAIL_NOISE
	noiseval+=(noiseTexture.Sample(wrapSamplerState,8.0*IN.noise_texc.xy).xyz)/2.0;
#endif
	noiseval*=IN.texCoords.w;
	vec3 view=normalize(IN.view);
	float cos0=dot(lightDir.xyz,view.xyz);
	vec4 final=calcUnfadedColour(IN.texCoords.xyz,IN.layerFade,noiseval,fractalScale,cloud_interp,lightResponse,ambientColour,cos0);
	final.rgb=applyFades(final.rgb,IN.fade_texc,cos0,earthshadowMultiplier);
    return final;
}

vec4 PS_Clouds3DNoise( toPS IN): SV_TARGET
{
	vec3 noise_texc	=IN.texCoords.xyz*noise3DTexcoordScale;
	vec3 noiseval		=vec3(0.0,0.0,0.0);
	float mul=0.5;
	for(int i=0;i<noise3DOctaves;i++)
	{
		noiseval		+=(noiseTexture3D.Sample(wrapSamplerState,noise_texc).xyz)*mul;
		noise_texc*=2.0;
		mul*=noise3DPersistence;
	}
	noiseval*=IN.texCoords.w;
	vec3 view=normalize(IN.view);
	float cos0=dot(lightDir.xyz,view.xyz);
	vec4 final=calcUnfadedColour(IN.texCoords.xyz,IN.layerFade,noiseval,fractalScale,cloud_interp,lightResponse,ambientColour,cos0);
	final.rgb=applyFades(final.rgb,IN.fade_texc,cos0,earthshadowMultiplier);
    return final;
}

vec4 PS_WithLightning(toPS IN): SV_TARGET
{
	vec3 noiseval=(noiseTexture.Sample(wrapSamplerState,IN.noise_texc.xy).xyz).xyz;
#ifdef DETAIL_NOISE
	noiseval+=(noiseTexture.Sample(wrapSamplerState,8.0*IN.noise_texc.xy).xyz)/2.0;
#endif
	noiseval*=IN.texCoords.w;
	vec3 view=normalize(IN.view);
	float cos0=dot(lightDir.xyz,view.xyz);
	vec4 final=calcUnfadedColour(IN.texCoords.xyz,IN.layerFade,noiseval,fractalScale,cloud_interp,lightResponse,ambientColour,cos0);

	vec4 lightning=lightningIlluminationTexture.Sample(lightningSamplerState,IN.texCoordLightning.xyz);

	float l=dot(lightningMultipliers,lightning);
	vec3 lightningC=l*lightningColour.xyz;
	final.rgb+=lightningColour.w*lightningC;
	
	final.rgb=applyFades(final.rgb,IN.fade_texc,cos0,earthshadowMultiplier);
	final.rgb*=final.a;

	final.rgb+=lightningC*(final.a+IN.layerFade);
    return final;
}
//uniform_buffer SingleLayerConstants SIMUL_BUFFER_REGISTER(12)
//{
	uniform vec4 rect;
//};

vertexOutputCS VS_FullScreen(idOnly IN)
{
	vertexOutputCS OUT;
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
#ifdef REVERSE_DEPTH
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=0.5*(vec2(1.0,1.0)+vec2(pos.x,-pos.y));
//OUT.texCoords	+=0.5*texelOffsets;
	return OUT;
}

vertexOutputCS VS_CrossSection(idOnly IN)
{
    vertexOutputCS OUT;
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
#ifdef REVERSE_DEPTH
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=pos;
    return OUT;
}

vec4 PS_Simple( vertexOutputCS IN):SV_TARGET
{
    return noiseTexture.Sample(wrapSamplerState,IN.texCoords.xy);
}

vec4 PS_ShowNoise( vertexOutputCS IN):SV_TARGET
{
    vec4 lookup=noiseTexture.Sample(wrapSamplerState,IN.texCoords.xy);
	return vec4(0.5*(lookup.rgb+1.0),1.0);
}

vec4 PS_ShowShadow( vertexOutputCS IN):SV_TARGET
{
	return ShowCloudShadow(cloudShadowTexture,nearFarTexture,IN.texCoords);
}

SamplerState crossSectionSamplerState
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Clamp;
};

#define CROSS_SECTION_STEPS 32

vec4 PS_CrossSection(vec2 texCoords,float yz)
{
	//vec3 texc=vec3(crossSectionOffset.x+IN.texCoords.x,yz*IN.texCoords.y,crossSectionOffset.z+(1.0-yz)*IN.texCoords.y);
	vec3 texc=vec3(texCoords.x,yz*texCoords.y,(1.0-yz)*texCoords.y);
	int i=0;
	vec3 accum=vec3(0.f,0.5f,1.f);
	texc.y+=0.5*(1.0-yz)/(float)CROSS_SECTION_STEPS;
	texc.z+=0.5*yz/(float)CROSS_SECTION_STEPS;
	for(i=0;i<CROSS_SECTION_STEPS;i++)
	{
		vec4 density=cloudDensity1.Sample(crossSectionSamplerState,texc);
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

vec4 PS_CrossSectionXZ( vertexOutputCS IN):SV_TARGET
{
    return PS_CrossSection(IN.texCoords,0.f);
}

vec4 PS_CrossSectionXY( vertexOutputCS IN): SV_TARGET
{
    return PS_CrossSection(IN.texCoords,1.f);
}

BlendState CloudBlend
{
	BlendEnable[0] = TRUE;
	SrcBlend = SRC_ALPHA;
	DestBlend = INV_SRC_ALPHA;
    BlendOp = ADD;
    SrcBlendAlpha = ZERO;
    DestBlendAlpha = INV_SRC_ALPHA;
    BlendOpAlpha = ADD;
    //RenderTargetWriteMask[0] = 0x0F;
};

technique11 simul_clouds
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetVertexShader(CompileShader(vs_5_0,VS_Main()));
		SetPixelShader(CompileShader(ps_5_0,PS_Clouds()));
    }
}

technique11 simul_raytrace
{
    pass p0 
    {
		SetDepthStencilState(WriteDepth,0);
        SetRasterizerState( RenderNoCull );
		SetVertexShader(CompileShader(vs_5_0,VS_Raytrace()));
		SetPixelShader(CompileShader(ps_5_0,PS_Raytrace()));
    }
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
		SetDepthStencilState(DisableDepth,0);
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

technique11 shadow_near_far
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_FullScreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_ShadowNearFar()));
    }
}

technique11 cross_section_xz
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetBlendState(NoBlend,vec4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_CrossSection()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_CrossSectionXZ()));
    }
}

technique11 cross_section_xy
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetBlendState(NoBlend,vec4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_CrossSection()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_CrossSectionXY()));
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