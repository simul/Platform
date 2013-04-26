#include "CppHLSL.hlsl"
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

struct vertexInput
{
    float3 position		: POSITION;
	// Per-instance data:
	vec2 noiseOffset	: TEXCOORD0;
	float noiseScale	: TEXCOORD1;
	float layerFade		: TEXCOORD2;
	float layerDistance	: TEXCOORD3;
};

struct vertexOutput
{
    float3 position			: POSITION;
	// Per-instance data:
	vec2 noiseOffset	: TEXCOORD0;
	float noiseScale	: TEXCOORD1;
	float layerFade		: TEXCOORD2;
	float layerDistance	: TEXCOORD3;
};

struct geomOutput
{
    float4 hPosition			: SV_POSITION;
    float2 noise_texc			: TEXCOORD0;
    float4 texCoords			: TEXCOORD1;
	float3 view					: TEXCOORD2;
    float3 texCoordLightning	: TEXCOORD3;
    float2 fade_texc			: TEXCOORD4;
	float layerFade				: TEXCOORD5;
};

vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
	OUT.position=IN.position;
	// Per-instance data:
	OUT.noiseOffset		=IN.noiseOffset;
	OUT.noiseScale		=IN.noiseScale;
	OUT.layerFade		=IN.layerFade;
	OUT.layerDistance	=IN.layerDistance;
    return OUT;
}
static const float pi=3.1415926536;

#define ELEV_STEPS 20

[maxvertexcount(53)]
void GS_Main(line vertexOutput input[2], inout TriangleStream<geomOutput> OutputStream)
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
	int e2=min((int)(a2*ELEV_STEPS/2+ELEV_STEPS/2)+2,ELEV_STEPS);

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
			vertexOutput IN=input[j];
			float3 pos=cosine*IN.position;
			pos.z=sine;
			float3 t1=pos.xyz*IN.layerDistance;
			float4 hpos=mul(worldViewProj,float4(t1.xyz,1.0));

			geomOutput OUT;
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

#define pi (3.1415926536f)
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


float4 PS_Clouds( geomOutput IN): SV_TARGET
{
	float3 noiseval=(noiseTexture.Sample(noiseSamplerState,IN.noise_texc).xyz).xyz;
#ifdef DETAIL_NOISE
	noiseval+=(noiseTexture.Sample(noiseSamplerState,8.0*IN.noise_texc).xyz)/2.0;
#endif
	noiseval*=IN.texCoords.w;
	float3 pos=IN.texCoords.xyz+fractalScale.xyz*noiseval;
	float4 density=cloudDensity1.Sample(cloudSamplerState,pos);
	float4 density2=cloudDensity2.Sample(cloudSamplerState,pos);

	density=lerp(density,density2,cloud_interp);
	density.z*=IN.layerFade;
	density.z=saturate(density.z*(1.f+alphaSharpness)-alphaSharpness);

	if(density.z<=0)
		discard;
   // return float4(1.0,1.0,1.0,density.z);
	float3 view=normalize(IN.view);
	float cos0=dot(lightDir.xyz,view.xyz);
	float Beta=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	
	float3 loss=skyLossTexture.Sample(fadeSamplerState,IN.fade_texc).rgb;
	float4 insc=skyInscatterTexture.Sample(fadeSamplerState,IN.fade_texc);
	float4 skyl=skylightTexture.Sample(fadeSamplerState,IN.fade_texc);
	float3 ambient=density.w*ambientColour.rgb;

	float opacity=density.z;
	float3 sunlightColour=lerp(sunlightColour1,sunlightColour2,saturate(IN.texCoords.z));
	float3 final=(density.y*Beta+lightResponse.y*density.x)*sunlightColour+ambient.rgb;
	float3 inscatter=earthshadowMultiplier*InscatterFunction(insc,cos0);

	final*=loss;
	final+=skyl.rgb+inscatter;
    return float4(final.rgb,opacity);
}

float4 PS_WithLightning(geomOutput IN): SV_TARGET
{
	float3 view=normalize(IN.view);
	float3 loss=skyLossTexture.Sample(fadeSamplerState,IN.fade_texc).rgb;
	float4 insc=skyInscatterTexture.Sample(fadeSamplerState,IN.fade_texc);
	float4 skyl=skylightTexture.Sample(fadeSamplerState,IN.fade_texc);
	float cos0=dot(lightDir.xyz,view.xyz);
	float Beta=HenyeyGreenstein(cloudEccentricity,cos0);
	float3 inscatter=earthshadowMultiplier*InscatterFunction(insc,cos0);
	float3 noiseval=(noiseTexture.Sample(noiseSamplerState,IN.noise_texc.xy).xyz).xyz;
#ifdef DETAIL_NOISE
	noiseval+=(noiseTexture.Sample(noiseSamplerState,8.0*IN.noise_texc.xy).xyz)/2.0;
#endif
	noiseval*=IN.texCoords.w;
	float3 pos=IN.texCoords.xyz+fractalScale.xyz*noiseval;
	float4 density=cloudDensity1.Sample(cloudSamplerState,pos);
	float4 density2=cloudDensity2.Sample(cloudSamplerState,pos);

	float4 lightning=lightningIlluminationTexture.Sample(lightningSamplerState,IN.texCoordLightning.xyz);

	density=lerp(density,density2,cloud_interp);
	density.z*=IN.layerFade;
	density.z=saturate(density.z*(1.f+alphaSharpness)-alphaSharpness);
	if(density.z<=0)
		discard;
	float3 ambient=density.w*ambientColour.rgb;

	float opacity=density.z;
	float l=dot(lightningMultipliers,lightning);
	float3 lightningC=l*lightningColour.xyz;
	float3 sunlightColour=lerp(sunlightColour1,sunlightColour2,saturate(IN.texCoords.z));
	float3 final=(density.y*Beta+lightResponse.y*density.x)*sunlightColour+ambient.rgb+lightningColour.w*lightningC;
	
	final*=loss.xyz;
	final+=skyl.rgb+inscatter.xyz;

	final*=opacity;

	final+=lightningC*(opacity+IN.layerFade);
    return float4(final.rgb,opacity);
}

float4 PS_CloudsLowDef( geomOutput IN): SV_TARGET
{
	float3 noiseval=(noiseTexture.Sample(noiseSamplerState,IN.noise_texc).xyz).xyz;

	float3 pos=IN.texCoords.xyz+fractalScale.xyz*noiseval;
	float4 density=cloudDensity1.Sample(cloudSamplerState,pos);
	float4 density2=cloudDensity2.Sample(cloudSamplerState,pos);

	density=lerp(density,density2,cloud_interp);
	density.z*=IN.layerFade;
	density.z=saturate(density.z*(1.f+alphaSharpness)-alphaSharpness);
	if(density.z<=0)
		discard;

	float3 view=normalize(IN.view);
	float cos0=dot(lightDir.xyz,view.xyz);
	float Beta=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	
	float3 loss=skyLossTexture.Sample(fadeSamplerState,IN.fade_texc).rgb;
	float4 insc=skyInscatterTexture.Sample(fadeSamplerState,IN.fade_texc);
	float4 skyl=skylightTexture.Sample(fadeSamplerState,IN.fade_texc);
	float3 ambient=density.w*ambientColour.rgb;

	float opacity=density.z;
	float3 sunlightColour=lerp(sunlightColour1,sunlightColour2,saturate(IN.texCoords.z));
	float3 final=(density.x*Beta+lightResponse.y*density.y)*sunlightColour+ambient.rgb;
	float3 inscatter=skyl.rgb+earthshadowMultiplier*InscatterFunction(insc,cos0);

	final*=loss;
	final+=inscatter;

    return float4(final.rgb,opacity);
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
    OUT.hPosition = mul(worldViewProj,IN.position);

	OUT.texCoords.xy=IN.texCoords;
	OUT.texCoords.z=1;
	//OUT.colour=IN.colour;
    return OUT;
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

DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
}; 

BlendState DoBlend
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

BlendState NoBlend
{
	BlendEnable[0] = FALSE;
};

RasterizerState RenderNoCull
{
	CullMode = none;
};

technique11 simul_clouds_lowdef
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
	//	SetBlendState(DoBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );

        SetGeometryShader(CompileShader(gs_4_0,GS_Main()));
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
		SetPixelShader(CompileShader(ps_4_0,PS_CloudsLowDef()));
    }
}

technique11 simul_clouds
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		//SetBlendState(DoBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(CompileShader(gs_5_0,GS_Main()));
		SetVertexShader(CompileShader(vs_5_0,VS_Main()));
		SetPixelShader(CompileShader(ps_5_0,PS_Clouds()));
    }
}

technique11 simul_clouds_and_lightning
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		//SetBlendState(DoBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(CompileShader(gs_5_0,GS_Main()));
		SetVertexShader(CompileShader(vs_5_0,VS_Main()));
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
