#include "CppHLSL.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/simul_cloud_constants.sl"
#define Z_VERTICAL 1
#ifndef WRAP_CLOUDS
	#define WRAP_CLOUDS 1
#endif
#ifndef DETAIL_NOISE
	#define DETAIL_NOISE 1
#endif

Texture3D cloudDensity1;
Texture3D cloudDensity2;
SamplerState cloudSamplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Clamp;
};
SamplerState crossSectionSamplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Clamp;
};

Texture2D noiseTexture;
Texture3D noiseTexture3D;
SamplerState noiseSamplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Wrap;
};

Texture3D lightningIlluminationTexture;
SamplerState lightningSamplerState
{
    Texture = <lightningIlluminationTexture>;
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Border;
	AddressV = Border;
	AddressW = Border;
};
Texture2D skyLossTexture;
Texture2D skyInscatterTexture;
Texture2D skylightTexture;
SamplerState fadeSamplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
};

static const float pi=3.1415926536;

float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.f+g2-2.0*g*cos0;
	return (1.0-g2)/(4.0*pi*sqrt(u*u*u));
}

float3 InscatterFunction(float4 inscatter_factor,float cos0)
{
	float BetaRayleigh=0.0596831*(1.0+cos0*cos0);
	float BetaMie=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	float3 BetaTotal=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(float3(1.0,1.0,1.0)+inscatter_factor.a*mieRayleighRatio.xyz);
	float3 colour=BetaTotal*inscatter_factor.rgb;
	return colour;
}

float4 calcDensity(float3 texCoords,float layerFade,float3 noiseval)
{
	float3 pos=texCoords.xyz+fractalScale.xyz*noiseval;
	float4 density=cloudDensity1.SampleLevel(cloudSamplerState,pos,0);
	float4 density2=cloudDensity2.SampleLevel(cloudSamplerState,pos,0);

	density=lerp(density,density2,cloud_interp);
	density.z*=layerFade;
	density.z=saturate(density.z*(1.f+alphaSharpness)-alphaSharpness);
	return density;
}

float4 calcColour(float4 density,float cos0,float texz)
{
	float Beta=lightResponse.x*HenyeyGreenstein(cloudEccentricity*density.y,cos0);
	float3 ambient=density.w*ambientColour.rgb;
	float opacity=density.z;
	float3 sunlightColour=lerp(sunlightColour1,sunlightColour2,saturate(texz));
	float4 final;
	final.rgb=(density.y*Beta+lightResponse.y*density.x)*sunlightColour+ambient.rgb;
	final.a=opacity;
	return final;
}

float4 calcUnfadedColour(float3 texCoords,float layerFade,float3 noiseval,float cos0)
{
	float4 density=calcDensity(texCoords,layerFade,noiseval);
	if(density.z<=0)
		discard;
	float4 final=calcColour(density,cos0,texCoords.z);
	return final;
}
float3 applyFades(float3 final,float2 fade_texc,float cos0,float earthshadowMultiplier)
{
	float3 loss=skyLossTexture.SampleLevel		(fadeSamplerState,fade_texc,0).rgb;
	float4 insc=skyInscatterTexture.SampleLevel	(fadeSamplerState,fade_texc,0);
	float3 skyl=skylightTexture.SampleLevel		(fadeSamplerState,fade_texc,0).rgb;
	float3 inscatter=earthshadowMultiplier*InscatterFunction(insc,cos0);
	final*=loss;
	final+=skyl+inscatter;
    return final;
}

struct RaytraceVertexInput
{
    float3 position			: POSITION;
	float2 texCoords		: TEXCOORD0;
};

struct RaytraceVertexOutput
{
    float4 hPosition		: SV_POSITION;
	float2 texCoords		: TEXCOORD0;
};

RaytraceVertexOutput VS_Raytrace(RaytraceVertexInput IN)
{
    RaytraceVertexOutput OUT;
	OUT.hPosition =float4(IN.position.xy,0.0,1.0);
	OUT.texCoords=IN.texCoords;
	return OUT;
}

float4 PS_Raytrace(RaytraceVertexOutput IN) : SV_TARGET
{
	float4 pos=float4(-1.f,-1.f,1.f,1.f);
	pos.x+=2.f*IN.texCoords.x;
	pos.y+=2.f*IN.texCoords.y;
	float3 view=normalize(mul(invViewProj,pos).xyz);
	float4 colour=float4(0.0,0.0,0.0,1.0);
	float3 viewPos=float3(wrld[3][0],wrld[3][1],wrld[3][2]);
	float cos0=dot(lightDir.xyz,view.xyz);
	float sine=view.z;
	float3 noise_pos		=mul(noiseMatrix,float4(view.xyz,1.0f)).xyz;
	float2 noise_texc_0		=float2(atan2(noise_pos.x,noise_pos.z),atan2(noise_pos.y,noise_pos.z));

	float min_texc_z=-.1;//fractalScale.z*1.5;
	float max_texc_z=1.0-min_texc_z;
	//[unroll(120)]
	for(int i=0;i<layerCount;i++)
	{
		const LayerData layer=layers[i];
		float dist=layer.layerDistance;
		float3 pos=viewPos+dist*view;
		float3 texCoords=(pos-cornerPos)*inverseScales;
		if(texCoords.z<min_texc_z||texCoords.z>max_texc_z)
			continue;
		float2 noise_texc		=noise_texc_0*layer.noiseScale+layer.noiseOffset;
		float3 noiseval=(noiseTexture.SampleLevel(noiseSamplerState,noise_texc.xy,3).xyz).xyz;
#ifdef DETAIL_NOISE
		//noiseval+=(noiseTexture.SampleLevel(noiseSamplerState,8.0*noise_texc.xy,0).xyz)/2.0;
#endif
		float4 density=calcDensity(texCoords,1.0,noiseval);
		if(density.z<=0)
			continue;
		float4 c=calcColour(density,cos0,texCoords.z);
		float2 fade_texc=float2(sqrt(dist/300000.0),0.5f*(1.f-sine));
		c.rgb=applyFades(c.rgb,fade_texc,cos0,earthshadowMultiplier);
		colour*=(1.0-c.a);
		colour.rgb+=c.rgb*c.a;
	}
    return float4(colour.rgb,1.0-colour.a);
}

struct vertexInput
{
    float3 position			: POSITION;
	// Per-instance data:
	vec2 noiseOffset		: TEXCOORD0;
	vec2 elevationRange		: TEXCOORD1;
	float noiseScale		: TEXCOORD2;
	float layerFade			: TEXCOORD3;
	float layerDistance		: TEXCOORD4;
};

struct VStoGS
{
    float3 position			: POSITION;
	// Per-instance data:
	vec2 noiseOffset		: TEXCOORD0;
	float noiseScale		: TEXCOORD1;
	float layerFade			: TEXCOORD2;
	float layerDistance		: TEXCOORD3;
};

struct toPS
{
    float4 hPosition		: SV_POSITION;
    float2 noise_texc		: TEXCOORD0;
    float4 texCoords		: TEXCOORD1;
	float3 view				: TEXCOORD2;
    float3 texCoordLightning: TEXCOORD3;
    float2 fade_texc		: TEXCOORD4;
	float layerFade			: TEXCOORD5;
};

VStoGS VS_FeedToGS(vertexInput IN)
{
    VStoGS OUT;
	OUT.position=IN.position;
	// Per-instance data:
	OUT.noiseOffset		=IN.noiseOffset;
	OUT.noiseScale		=IN.noiseScale;
	OUT.layerFade		=IN.layerFade;
	OUT.layerDistance	=IN.layerDistance;
    return OUT;
}

toPS VS_Main(vertexInput IN)
{
    toPS OUT;
	float3 pos				=IN.position;
	if(pos.z<IN.elevationRange.x||pos.z>IN.elevationRange.y)
		pos*=0.f;
	OUT.view				=normalize(pos.xyz);
	float sine				=OUT.view.z;
	float3 t1				=pos.xyz*IN.layerDistance;
	OUT.hPosition			=mul(worldViewProj,float4(t1.xyz,1.0));
	float3 noise_pos		=mul(noiseMatrix,float4(pos.xyz,1.0f)).xyz;
	OUT.noise_texc			=float2(atan2(noise_pos.x,noise_pos.z),atan2(noise_pos.y,noise_pos.z));
	OUT.noise_texc			*=IN.noiseScale;
	OUT.noise_texc			+=IN.noiseOffset;
	
	float3 wPos				=mul(float4(t1,1.0f),wrld).xyz;
	OUT.texCoords.xyz		=wPos-cornerPos;
	OUT.texCoords.xyz		*=inverseScales;
	OUT.texCoords.w			=0.5f+0.5f*saturate(OUT.texCoords.z);
	float3 texCoordLightning=(pos.xyz-illuminationOrigin.xyz)/illuminationScales.xyz;
	OUT.texCoordLightning	=texCoordLightning;
	
	float depth				=IN.layerDistance/maxFadeDistanceMetres;
		
	OUT.fade_texc			=float2(sqrt(depth),0.5f*(1.f-sine));
	OUT.layerFade			=IN.layerFade;
    return OUT;
}
#define ELEV_STEPS 20

[maxvertexcount(53)]
void GS_Main(line VStoGS input[2], inout TriangleStream<toPS> OutputStream)
{
	/*if(input[0].texCoords.z>1.0&&input[1].texCoords.z>1.0&&input[2].texCoords.z>1.0)
		return;
	if(input[0].texCoords.z<0.0&&input[1].texCoords.z<0.0&&input[2].texCoords.z<0.0)
		return;*/
	// work out the start and end angles.
	float dh1=cornerPos.z-wrld._43;
	float dh2=dh1+1.0/inverseScales.z;
	float a1=atan(dh1/input[0].layerDistance)*2.0/pi;
	float a2=atan(dh2/input[0].layerDistance)*2.0/pi;
	int e1=max((int)(a1*ELEV_STEPS/2+ELEV_STEPS/2)-1,0);
	int e2=min((int)(a2*ELEV_STEPS/2+ELEV_STEPS/2)+3,ELEV_STEPS);

	for(int i=e1;i<e2+1;i++)
	{
		float angle=pi*(float)(i-ELEV_STEPS/2)/(float)ELEV_STEPS;
		float cosine=cos(angle);
		float sine=sin(angle);
		float3 pos[2];
		float3 t1[2];
		float4 hpos[2];
		for(int j = 0; j < 2; j++)
		{
			VStoGS IN=input[j];
			float3 pos=cosine*IN.position;
			pos.z=sine;
			float3 t1=pos.xyz*IN.layerDistance;
			float4 hpos=mul(worldViewProj,float4(t1.xyz,1.0));

			toPS OUT;
			OUT.hPosition			=hpos;
			float3 noise_pos		=mul(noiseMatrix,float4(pos.xyz,1.0f)).xyz;
			OUT.noise_texc			=float2(atan2(noise_pos.x,noise_pos.z),atan2(noise_pos.y,noise_pos.z));
			OUT.noise_texc			*=IN.noiseScale;
			OUT.noise_texc			+=IN.noiseOffset;
			
			OUT.view				=pos.xyz;
			float3 wPos				=mul(float4(t1,1.0f),wrld).xyz;
			OUT.texCoords.xyz		=wPos-cornerPos;
			OUT.texCoords.xyz		*=inverseScales;
			OUT.texCoords.w			=0.5f+0.5f*saturate(OUT.texCoords.z);
			float3 texCoordLightning=(pos.xyz-illuminationOrigin.xyz)/illuminationScales.xyz;
			OUT.texCoordLightning	=texCoordLightning;
			float3 view				=normalize(pos.xyz);
			
			float depth				=IN.layerDistance/maxFadeDistanceMetres;
			OUT.fade_texc			=float2(sqrt(depth),0.5f*(1.f-sine));
			OUT.layerFade			=IN.layerFade;
    		OutputStream.Append(OUT);
		}
	}
}
float4 PS_Clouds( toPS IN): SV_TARGET
{
	float3 noiseval=(noiseTexture.Sample(noiseSamplerState,IN.noise_texc.xy).xyz).xyz;
#ifdef DETAIL_NOISE
	noiseval+=(noiseTexture.Sample(noiseSamplerState,8.0*IN.noise_texc.xy).xyz)/2.0;
#endif
	noiseval*=IN.texCoords.w;
	float3 view=normalize(IN.view);
	float cos0=dot(lightDir.xyz,view.xyz);
	//float4 final=float4(0.5,0.5,0.5,0.05);
	float4 final=calcUnfadedColour(IN.texCoords.xyz,IN.layerFade,noiseval,cos0);
	final.rgb=applyFades(final.rgb,IN.fade_texc,cos0,earthshadowMultiplier);
    return final;
}

float4 PS_Clouds3DNoise( toPS IN): SV_TARGET
{
	float3 noise_texc	=IN.texCoords.xyz*noise3DTexcoordScale;
	float3 noiseval		=float3(0.0,0.0,0.0);
	float mul=0.5;
	for(int i=0;i<noise3DOctaves;i++)
	{
		noiseval		+=(noiseTexture3D.Sample(noiseSamplerState,noise_texc).xyz)*mul;
		noise_texc*=2.0;
		mul*=noise3DPersistence;
	}
	noiseval*=IN.texCoords.w;
	float3 view=normalize(IN.view);
	float cos0=dot(lightDir.xyz,view.xyz);
	float4 final=calcUnfadedColour(IN.texCoords.xyz,IN.layerFade,noiseval,cos0);
	final.rgb=applyFades(final.rgb,IN.fade_texc,cos0,earthshadowMultiplier);
    return final;
}

float4 PS_WithLightning(toPS IN): SV_TARGET
{
	float3 noiseval=(noiseTexture.Sample(noiseSamplerState,IN.noise_texc.xy).xyz).xyz;
#ifdef DETAIL_NOISE
	noiseval+=(noiseTexture.Sample(noiseSamplerState,8.0*IN.noise_texc.xy).xyz)/2.0;
#endif
	noiseval*=IN.texCoords.w;
	float3 view=normalize(IN.view);
	float cos0=dot(lightDir.xyz,view.xyz);
	float4 final=calcUnfadedColour(IN.texCoords.xyz,IN.layerFade,noiseval,cos0);

	float4 lightning=lightningIlluminationTexture.Sample(lightningSamplerState,IN.texCoordLightning.xyz);

	float l=dot(lightningMultipliers,lightning);
	float3 lightningC=l*lightningColour.xyz;
	final.rgb+=lightningColour.w*lightningC;
	
	final.rgb=applyFades(final.rgb,IN.fade_texc,cos0,earthshadowMultiplier);
	final.rgb*=final.a;

	final.rgb+=lightningC*(final.a+IN.layerFade);
    return final;
}

struct vertexInputCS
{
    float4 position			: POSITION;
    float2 texCoords		: TEXCOORD0;
};

struct vertexOutputCS
{
    float4 hPosition		: SV_POSITION;
    float3 texCoords		: TEXCOORD0;
};

vertexOutputCS VS_CrossSection(vertexInputCS IN)
{
    vertexOutputCS OUT;
    OUT.hPosition	=float4(IN.position.xy,1.0,1.0);
	OUT.texCoords.xy=IN.texCoords;
	OUT.texCoords.z=1;
    return OUT;
}

float4 PS_Simple( vertexOutputCS IN):SV_TARGET
{
    return noiseTexture.Sample(crossSectionSamplerState,IN.texCoords.xy);
}

#define CROSS_SECTION_STEPS 32
float4 PS_CrossSectionXZ( vertexOutputCS IN):SV_TARGET
{
	float3 texc=float3(crossSectionOffset.x+IN.texCoords.x,0,crossSectionOffset.z+1.0-IN.texCoords.y);
	int i=0;
	float3 accum=float3(0.f,0.5f,1.f);
	texc.y+=.5f/(float)CROSS_SECTION_STEPS;
	for(i=0;i<CROSS_SECTION_STEPS;i++)
	{
		float4 density=cloudDensity1.Sample(crossSectionSamplerState,texc);
		float3 colour=float3(.5,.5,.5)*(lightResponse.x*density.y+lightResponse.y*density.x);
		colour.gb+=float2(.125,.25)*(lightResponse.z*density.w);
		float opacity=density.z;
		colour*=opacity;
		accum*=1.f-opacity;
		accum+=colour;
		texc.y+=1.f/(float)CROSS_SECTION_STEPS;
	}
    return float4(accum,1);
}

float4 PS_CrossSectionXY( vertexOutputCS IN): SV_TARGET
{
	float3 texc=float3(crossSectionOffset.x+IN.texCoords.x,crossSectionOffset.y+IN.texCoords.y,0);
	int i=0;
	float3 accum=float3(0.f,0.5f,1.f);
	texc.z+=.5f/(float)CROSS_SECTION_STEPS;
	for(i=0;i<CROSS_SECTION_STEPS;i++)
	{
		float4 density=cloudDensity1.Sample(crossSectionSamplerState,texc);
		float3 colour=float3(.5,.5,.5)*(lightResponse.x*density.y+lightResponse.y*density.x);
		colour.gb+=float2(.125,.25)*(lightResponse.z*density.w);
		float opacity=density.z;//+.05f;
		colour*=opacity;
		accum*=1.f-opacity;
		accum+=colour;
		texc.z+=1.f/(float)CROSS_SECTION_STEPS;
	}
    return float4(accum,1);
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
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetVertexShader(CompileShader(vs_5_0,VS_Raytrace()));
		SetPixelShader(CompileShader(ps_5_0,PS_Raytrace()));
    }
}

technique11 simul_clouds_gs
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		//SetBlendState(CloudBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(CompileShader(gs_5_0,GS_Main()));
		SetVertexShader(CompileShader(vs_5_0,VS_FeedToGS()));
		SetPixelShader(CompileShader(ps_5_0,PS_Clouds()));
    }
}

technique11 simul_clouds_3d_noise
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		//SetBlendState(CloudBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(CompileShader(gs_5_0,GS_Main()));
		SetVertexShader(CompileShader(vs_5_0,VS_FeedToGS()));
		SetPixelShader(CompileShader(ps_5_0,PS_Clouds3DNoise()));
    }
}

technique11 simul_clouds_and_lightning
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		//SetBlendState(CloudBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(CompileShader(gs_5_0,GS_Main()));
		SetVertexShader(CompileShader(vs_5_0,VS_FeedToGS()));
		SetPixelShader(CompileShader(ps_5_0,PS_WithLightning()));
    }
}

technique11 cross_section_xz
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetBlendState(NoBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
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
		SetBlendState(NoBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
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
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_CrossSection()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Simple()));
    }
}