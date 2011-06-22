cbuffer cbPerObject : register(b0)
{
	matrix worldViewProj : packoffset(c0);
};

Texture2D skyTexture1;
SamplerState samplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Mirror;
};
Texture2D skyTexture2;

Texture2D flareTexture;
SamplerState flareSamplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

//------------------------------------
// Parameters 
//------------------------------------
float4 eyePosition : EYEPOSITION_WORLDSPACE;
float4 lightDir : Direction;
float4 mieRayleighRatio;
float hazeEccentricity;
float skyInterp;
float altitudeTexCoord;
#define pi (3.1415926536f)

float3 sunlightColour;
//------------------------------------
// Structures 
//------------------------------------
struct vertexInput
{
    float3 position			: POSITION;
};

struct vertexOutput
{
    float4 hPosition		: SV_POSITION;
    float3 wDirection		: TEXCOORD0;
};

//------------------------------------
// Vertex Shader 
//------------------------------------
vertexOutput VS_Main(vertexInput IN) 
{
    vertexOutput OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));
    OUT.wDirection=normalize(IN.position.xyz);
    return OUT;
}

float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	return 0.5*0.079577+0.5*(1.f-g2)/(4.f*pi*pow(1.f+g2-2.f*g*cos0,1.5f));
}

float3 InscatterFunction(float4 inscatter_factor,float cos0)
{
	float BetaRayleigh=0.0596831f*(1.f+cos0*cos0);
	float BetaMie=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	float3 BetaTotal=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(float3(1,1,1)+inscatter_factor.a*mieRayleighRatio.xyz);
	float3 colour=BetaTotal*inscatter_factor.rgb;
	return colour;
}

float4 PS_Main( vertexOutput IN): SV_TARGET
{
	float3 view=normalize(IN.wDirection.xyz);
#ifdef Z_VERTICAL
	float sine	=view.z;
#else
	float sine	=view.y;
#endif
	float2 texcoord	=float2(0.5f*(1.f-sine),altitudeTexCoord);
	float4 insc1=skyTexture1.Sample(samplerState,texcoord);
	float4 insc2=skyTexture2.Sample(samplerState,texcoord);
	float4 insc=lerp(insc1,insc2,skyInterp);
	float cos0=dot(lightDir.xyz,view.xyz);
	float3 colour=InscatterFunction(insc,cos0);

	return float4(colour.rgb,1.f);
}

struct svertexInput
{
    float3 position			: POSITION;
    float2 tex				: TEXCOORD0;
};

struct svertexOutput
{
    float4 hPosition		: SV_POSITION;
    float2 tex				: TEXCOORD0;
};

svertexOutput VS_Sun(svertexInput IN) 
{
    svertexOutput OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));
    OUT.tex=IN.position.xy;
    return OUT;
}

float4 PS_Sun( svertexOutput IN): SV_TARGET
{
	float r=length(IN.tex);
	float brightness;
	if(r<0.99f)
		brightness=(0.99f-r)/0.4f;
	else
		brightness=0.f;
	brightness=saturate(brightness);
	float3 colour=brightness*sunlightColour;
	return float4(colour,1.f);
}

float4 PS_Flare( svertexOutput IN): SV_TARGET
{
	float3 colour=sunlightColour.rgb*flareTexture.Sample(flareSamplerState,float2(.5f,.5f)+0.5f*IN.tex).rgb;
	return float4(colour,1.f);
}

//------------------------------------
// technique10
//------------------------------------
DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
}; 
BlendState DontBlend
{
	BlendEnable[0] = FALSE;
};
BlendState DoBlend
{
	BlendEnable[0] = TRUE;
	SrcBlend = One;
	DestBlend = One;
};
RasterizerState RenderNoCull { CullMode = none; };

technique10 simul_sky
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
	//	SetBlendState(DoBlend,float4( 0.0f, 0.0f, 0.0f, 0.5f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Main()));
    }
}

technique10 simul_sun
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Sun()));
		SetPixelShader(CompileShader(ps_4_0,PS_Sun()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}


technique10 simul_flare
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Sun()));
		SetPixelShader(CompileShader(ps_4_0,PS_Flare()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}


technique10 simul_query
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Sun()));
		SetPixelShader(CompileShader(ps_4_0,PS_Sun()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}
