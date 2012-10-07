#ifndef MAX_FADE_DISTANCE_METRES
	#define MAX_FADE_DISTANCE_METRES (300000.f)
#endif
#ifndef Z_VERTICAL
	#define Y_VERTICAL 1
#endif
#ifndef WRAP_CLOUDS
	#define WRAP_CLOUDS 1
#endif
#ifndef DETAIL_NOISE
	#define DETAIL_NOISE 1
#endif
#ifndef FADE_MODE
	#define FADE_MODE 1
#endif

cbuffer cbPerObject : register( b0 )
{
	matrix		worldViewProj		: packoffset( c0 );
	matrix		world				: packoffset( c4 );
};

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
SamplerState fadeSamplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
};
cbuffer cbUser : register(b2)
{
	float4 eyePosition			: packoffset(c0);
	float4 lightResponse		: packoffset(c1);
	float3 lightDir				: packoffset(c2);
	float3 skylightColour		: packoffset(c3);
	float3 sunlightColour		: packoffset(c4);
	float4 fractalScale			: packoffset(c5);
	float interp				: packoffset(c6);

	float4 lightningMultipliers	: packoffset(c7);
	float4 lightningColour		: packoffset(c8);
	float3 illuminationOrigin	: packoffset(c9);
	float3 illuminationScales	: packoffset(c10);
	float hazeEccentricity		: packoffset(c11);
	float3 mieRayleighRatio		: packoffset(c12);
	float fadeInterp			: packoffset(c13);
	float cloudEccentricity		: packoffset(c14);
	float alphaSharpness		: packoffset(c15);
	float3 crossSectionOffset	: packoffset(c16);
};

struct vertexInput
{
    float3 position			: POSITION;
    float3 texCoords		: TEXCOORD0;
    float layerFade			: TEXCOORD1;
    float2 texCoordsNoise	: TEXCOORD2;
};

struct vertexOutput
{
    float4 hPosition			: SV_POSITION;
    float2 texCoordsNoise		: TEXCOORD0;
	float layerFade				: TEXCOORD1;
    float4 texCoords			: TEXCOORD2;
	float3 wPosition			: TEXCOORD3;
    float3 texCoordLightning	: TEXCOORD4;
    float2 fade_texc			: TEXCOORD5;
    float2 texCoordsNoise2		: TEXCOORD6;
};

vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
    OUT.hPosition = mul( worldViewProj,float4(IN.position.xyz,1.0));
	OUT.texCoords.xyz=IN.texCoords;
	OUT.texCoords.w=0.5f+0.5f*saturate(IN.texCoords.z);
	const float c=fractalScale.w;
	OUT.texCoordsNoise=IN.texCoordsNoise;
	OUT.texCoordsNoise2=IN.texCoordsNoise*8;
	OUT.wPosition=(IN.position.xyz-eyePosition.xyz);
	OUT.layerFade=IN.layerFade;
	// Note position.xzy is used!
	float3 texCoordLightning=(IN.position.xzy-illuminationOrigin.xyz)/illuminationScales.xyz;
	OUT.texCoordLightning=texCoordLightning;
	float3 view=normalize(OUT.wPosition.xyz);
#ifdef Y_VERTICAL
	float sine=view.y;
#else
	float sine=view.z;
#endif
// Fade mode ZERO - fade values come from the vertex. So we pass them on to the pixel shader:
#if FADE_MODE==0
    OUT.loss			=IN.loss;
    OUT.inscatter		=IN.inscatter;
#endif
// Fade mode ONE - fade is calculated from the fade textures. So we send a texture coordinate:
#if FADE_MODE==1
	float maxd=1.f;
	float depth=length(OUT.wPosition.xyz)/MAX_FADE_DISTANCE_METRES;
	//OUT.fade_texc=float2(length(OUT.wPosition.xyz)/MAX_FADE_DISTANCE_METRES,0.5f*(1.f-sine));
	OUT.fade_texc=float2(sqrt(depth/maxd),0.5f*(1.f-sine));
#endif
    return OUT;
}

#define pi (3.1415926536f)
float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.f+g2-2.0*g*cos0;
	return 0.5*0.079577+0.5*(1.0-g2)/(4.0*pi*sqrt(u*u*u));
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
	static const float3 noise_offset=float3(0.49803921568627452,0.49803921568627452,0.49803921568627452);

float4 PS_WithLightning(vertexOutput IN): SV_TARGET
{
	float3 view=normalize(IN.wPosition);
	float3 loss=skyLossTexture.Sample(fadeSamplerState,IN.fade_texc).rgb;
	float4 insc=skyInscatterTexture.Sample(fadeSamplerState,IN.fade_texc);
	float cos0=dot(lightDir.xyz,view.xyz);
	float Beta=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	float3 inscatter=InscatterFunction(insc,cos0);
	float3 noiseval=(noiseTexture.Sample(noiseSamplerState,IN.texCoordsNoise.xy).xyz-noise_offset).xyz;
#ifdef DETAIL_NOISE
	noiseval+=(noiseTexture.Sample(noiseSamplerState,IN.texCoordsNoise.xy*8).xyz-noise_offset)/2.0;
	noiseval*=IN.texCoords.w;
#endif
	float3 pos=IN.texCoords.xyz+fractalScale.xyz*noiseval;
	float4 density=cloudDensity1.Sample(cloudSamplerState,pos);
	float4 density2=cloudDensity2.Sample(cloudSamplerState,pos);

	float4 lightning=lightningIlluminationTexture.Sample(lightningSamplerState,IN.texCoordLightning.xyz);

	density=lerp(density,density2,interp);
	density.z*=IN.layerFade;
	density.z=saturate(density.z*(1.f+alphaSharpness)-alphaSharpness);
	if(density.z<=0)
		discard;
	float3 ambient=density.w*skylightColour.rgb;

	float opacity=density.z;
	float l=dot(lightningMultipliers,lightning);
	float3 lightningC=l*lightningColour.xyz;
	float3 final=(density.y*Beta+lightResponse.y*density.x)*sunlightColour+ambient.rgb+lightningColour.w*lightningC;
	
	final*=loss.xyz;
	final+=inscatter.xyz;

	final*=opacity;

	final+=lightningC*(opacity+IN.layerFade);
    return float4(final.rgb,opacity);
}

float4 PS_CloudsLowDef( vertexOutput IN): SV_TARGET
{
	float3 noiseval=(noiseTexture.Sample(noiseSamplerState,IN.texCoordsNoise).xyz-noise_offset).xyz;

	float3 pos=IN.texCoords.xyz+fractalScale.xyz*noiseval;
	float4 density=cloudDensity1.Sample(cloudSamplerState,pos);
	float4 density2=cloudDensity2.Sample(cloudSamplerState,pos);

	density=lerp(density,density2,interp);
	density.z*=IN.layerFade;
	density.z=saturate(density.z*(1.f+alphaSharpness)-alphaSharpness);
	if(density.z<=0)
		discard;

	float3 view=normalize(IN.wPosition);
	float cos0=dot(lightDir.xyz,view.xyz);
	float Beta=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	
	float3 loss=skyLossTexture.Sample(fadeSamplerState,IN.fade_texc).rgb;
	float4 insc=skyInscatterTexture.Sample(fadeSamplerState,IN.fade_texc);
	float3 ambient=density.w*skylightColour.rgb;

	float opacity=density.z;
	float3 final=(density.x*Beta+lightResponse.y*density.y)*sunlightColour+ambient.rgb;
	float3 inscatter=InscatterFunction(insc,cos0);

	final*=loss;
	final+=inscatter;

    return float4(final.rgb,opacity);
}

float4 PS_Clouds( vertexOutput IN): SV_TARGET
{
	float3 noiseval=(noiseTexture.Sample(noiseSamplerState,IN.texCoordsNoise).xyz-noise_offset).xyz;
#ifdef DETAIL_NOISE
	noiseval+=(noiseTexture.Sample(noiseSamplerState,IN.texCoordsNoise2).xyz-noise_offset)/2.0;
	noiseval*=IN.texCoords.w;
#endif
	float3 pos=IN.texCoords.xyz+fractalScale.xyz*noiseval;
	float4 density=cloudDensity1.Sample(cloudSamplerState,pos);
	float4 density2=cloudDensity2.Sample(cloudSamplerState,pos);

	density=lerp(density,density2,interp);
	density.z*=IN.layerFade;
	density.z=saturate(density.z*(1.f+alphaSharpness)-alphaSharpness);
	if(density.z<=0)
		discard;

	float3 view=normalize(IN.wPosition);
	float cos0=dot(lightDir.xyz,view.xyz);
	float Beta=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	
	float3 loss=skyLossTexture.Sample(fadeSamplerState,IN.fade_texc).rgb;
	float4 insc=skyInscatterTexture.Sample(fadeSamplerState,IN.fade_texc);
	float3 ambient=density.w*skylightColour.rgb;

	float opacity=density.z;
	float3 final=(density.y*Beta+lightResponse.y*density.x)*sunlightColour+ambient.rgb;
	float3 inscatter=InscatterFunction(insc,cos0);

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

#define CROSS_SECTION_STEPS 1
float4 PS_CrossSectionXZ( vertexOutputCS IN):SV_TARGET
{
	float3 texc=float3(crossSectionOffset.x+IN.texCoords.x,0,crossSectionOffset.z+IN.texCoords.y);
	int i=0;
	float3 accum=float3(0.f,0.5f,1.f);
	texc.y+=.5f/(float)CROSS_SECTION_STEPS;
	for(i=0;i<CROSS_SECTION_STEPS;i++)
	{
		float4 density=cloudDensity1.Sample(crossSectionSamplerState,texc);
		float3 colour=float3(.5,.5,.5)*(lightResponse.x*density.y+lightResponse.y*density.z);
		colour.gb+=float2(.125,.25)*(lightResponse.z*density.w);
		float opacity=density.x;
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
		float3 colour=float3(.5,.5,.5)*(lightResponse.x*density.y+lightResponse.y*density.z);
		colour.gb+=float2(.125,.25)*(lightResponse.z*density.w);
		float opacity=density.x;//+.05f;
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
    RenderTargetWriteMask[0] = 0x0F;
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
		SetBlendState(DoBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );

        SetGeometryShader(NULL);
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
		SetBlendState(DoBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );

        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
		SetPixelShader(CompileShader(ps_4_0,PS_Clouds()));
    }
}

technique11 simul_clouds_and_lightning
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetBlendState(DoBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
		SetPixelShader(CompileShader(ps_4_0,PS_WithLightning()));
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
