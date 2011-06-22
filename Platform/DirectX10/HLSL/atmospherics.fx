float4x4 invViewProj;

Texture2D depthTexture;
SamplerState depthSamplerState 
{
	AddressU = Clamp;
	AddressV = Clamp;
};

Texture2D imageTexture;
SamplerState imageSamplerState 
{
	AddressU = Clamp;
	AddressV = Clamp;
};

Texture3D lossTexture1;
Texture3D inscatterTexture1;
Texture2D lossTexture2;
Texture2D inscatterTexture2;
SamplerState fadeSamplerState 
{
	AddressU = Clamp;
	AddressV = Mirror;
	AddressW = Clamp;
};


float4 lightDir;
float4 MieRayleighRatio;
float HazeEccentricity;
float fadeInterp;
float altitudeTexCoord;
#define pi (3.1415926536f)

struct atmosVertexInput
{
    float2 position			: POSITION;
    float3 texCoords		: TEXCOORD0;
};

struct atmosVertexOutput
{
    float4 position			: POSITION;
    float4 hpos_duplicate		: TEXCOORD0;
    float3 texCoords		: TEXCOORD1;
};

atmosVertexOutput VS_Atmos(atmosVertexInput IN)
{
	atmosVertexOutput OUT;
	OUT.position=float4(IN.position.xy,0,1);
	OUT.hpos_duplicate=float4(IN.position.xy,0,1);
	OUT.texCoords.xyz = IN.texCoords.yzx;
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
	float BetaMie=HenyeyGreenstein(HazeEccentricity,cos0);		// Mie's phase function
	float3 BetaTotal=(BetaRayleigh+BetaMie*inscatter_factor.a*MieRayleighRatio.xyz)
		/(float3(1,1,1)+inscatter_factor.a*MieRayleighRatio.xyz);
	float3 colour=BetaTotal*inscatter_factor.rgb;
	return colour;
}

float4 PS_Atmos(atmosVertexOutput IN): SV_TARGET
{
	float4 pos=float4(-1.f,1.f,1.f,1.f);
	pos.x+=2.f*IN.texCoords.x;
	pos.y-=2.f*IN.texCoords.y;
	float3 view=mul(invViewProj,pos).xyz;
	view=normalize(view);
	float4 lookup=imageTexture.Sample(imageSamplerState,IN.texCoords.xy);
	float3 colour=lookup.rgb;
	float depth=lookup.a;
	if(depth>=.99f)
		discard;
	float sine=view.y;
	float3 texc=float3(depth,0.5f*(1.f-sine),altitudeTexCoord);//depth,0.5f*(1.f-sine),altitudeTexCoord);
	float3 loss1=lossTexture1.Sample(fadeSamplerState,texc.xyz).rgb;
	float3 loss2=lossTexture2.Sample(fadeSamplerState,texc.xyz).rgb;
	float3 loss=lerp(loss1,loss2,fadeInterp);
	colour*=loss;
	float4 insc1=inscatterTexture1.Sample(fadeSamplerState,texc.xyz);
	float4 insc2=inscatterTexture2.Sample(fadeSamplerState,texc.xyz);
	float4 inscatter_factor=lerp(insc1,insc2,fadeInterp);
	float cos0=dot(view,lightDir.xyz);
	colour+=InscatterFunction(inscatter_factor,cos0);
    return float4(colour,1.f);
}

DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
}; 

RasterizerState RenderNoCull
{
	CullMode = none;
};

technique10 simul_atmospherics
{
    pass
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState(RenderNoCull);

        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_Atmos()));
    }
}
