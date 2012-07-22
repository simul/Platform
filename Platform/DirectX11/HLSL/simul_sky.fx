cbuffer cbPerObject : register(b0)
{
	matrix worldViewProj : packoffset(c0);
	matrix proj : packoffset(c32);
	matrix cubemapViews[6] : packoffset(c48);
};

Texture2D skyTexture1;
Texture2D skyTexture2;
SamplerState samplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Mirror;
};

Texture2D flareTexture;
SamplerState flareSamplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

Texture3D fadeTexture1;
Texture3D fadeTexture2;
SamplerState fadeSamplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
	AddressW = Clamp;
};

TextureCube cubeTexture;

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
float3 colour;
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

struct geomOutput
{
    float4 hPosition		: SV_POSITION;
    float3 wDirection		: TEXCOORD0;
    uint RTIndex			: SV_RenderTargetArrayIndex;
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

vertexOutput VS_Cubemap(vertexInput IN) 
{
    vertexOutput OUT;
	// World matrix would be identity.
    OUT.hPosition=float4(IN.position.xyz,1.0);
    OUT.wDirection=normalize(IN.position.xyz);
    return OUT;
}

[maxvertexcount(3)]
void GS_Main( triangle vertexOutput input[3], inout TriangleStream<geomOutput> CubeMapStream, uint PrimitiveId : SV_PrimitiveID )
{
	geomOutput output;
	output.RTIndex = 0;
	for( int v = 0; v < 3; v++ )
	{
		output.hPosition = input[v].hPosition;
		output.wDirection = input[v].wDirection;
		CubeMapStream.Append( output );
	}
	CubeMapStream.RestartStrip();
}

[maxvertexcount(18)]
void GS_Cubemap( triangle vertexOutput input[3], inout TriangleStream<geomOutput> CubeMapStream, uint PrimitiveId : SV_PrimitiveID )
{
	for( int f = 0; f < 6; ++f )
	{
		// Compute screen coordinates
		geomOutput output;
	 
		output.RTIndex = f;
	 
		for( int v = 0; v < 3; v++ )
		{
			output.hPosition = mul(input[v].hPosition,cubemapViews[f]);
			output.hPosition = mul(output.hPosition,proj);
			output.wDirection = input[v].wDirection;
	 
			CubeMapStream.Append( output );
		}
	 
		CubeMapStream.RestartStrip();
	}
}

float4 PS_Test( geomOutput IN): SV_TARGET
{
	return float4(1.f,0,0,1.f);
}

float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.f+g2-2.f*g*cos0;
	return 0.5*0.079577+0.5*(1.f-g2)/(4.f*pi*sqrt(u*u*u));
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

float4 PS_Cubemap( vertexOutput IN): SV_TARGET
{
	float3 view=(IN.wDirection.xyz);
	float4 colour=cubeTexture.Sample(samplerState,view);
	return float4(colour.rgb,1.f);
}

float4 PS_Mainc( geomOutput IN): SV_TARGET
{
	float3 view=(IN.wDirection.xyz);
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

struct vertexInput3Dto2D
{
    float4 position			: POSITION;
    float2 texCoords		: TEXCOORD0;
};

struct vertexOutput3Dto2D
{
    float4 hPosition		: SV_POSITION;
    float2 texCoords		: TEXCOORD0;
};

vertexOutput3Dto2D VS_Fade3DTo2D(vertexInput3Dto2D IN) 
{
    vertexOutput3Dto2D OUT;
    OUT.hPosition=float4(IN.position.xy,1.0,1.0);
    OUT.texCoords=IN.texCoords;
    return OUT;
}

float4 PS_Fade3DTo2D(vertexOutput3Dto2D IN): SV_TARGET
{
	float3 texc=float3(IN.texCoords.xy,altitudeTexCoord);
	float4 colour1=fadeTexture1.Sample(fadeSamplerState,texc);
	float4 colour2=fadeTexture2.Sample(fadeSamplerState,texc);
	float4 colour=lerp(colour1,colour2,skyInterp);
    return colour;
}

float4 PS_ShowSkyTexture(vertexOutput3Dto2D IN): SV_TARGET
{
#ifdef USE_ALTITUDE_INTERPOLATION
	float4 colour=skyTexture1.Sample(fadeSamplerState,float2(IN.texCoords.y,altitudeTexCoord));
#else
	float4 colour=skyTexture1.Sample(fadeSamplerState,float2(IN.texCoords.y,0));
#endif
    return float4(colour.rgb,1);
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
	float3 output=brightness*colour;
	return float4(output,1.f);
}

float4 PS_Flare( svertexOutput IN): SV_TARGET
{
	float3 output=colour.rgb*flareTexture.Sample(flareSamplerState,float2(.5f,.5f)+0.5f*IN.tex).rgb;
	return float4(output,1.f);
}

float4 PS_Planet(svertexOutput IN): SV_TARGET
{
	float4 output=flareTexture.Sample(flareSamplerState,float2(.5f,.5f)-0.5f*IN.tex);
	// IN.tex is +- 1.
	float3 normal;
	normal.x=-IN.tex.x;
	normal.y=IN.tex.y;
	float l=length(IN.tex);
	normal.z=sqrt(1.f-l*l);
	float light=saturate(dot(normal.xyz,lightDir.xyz));
	//output.rgb*=colour.rgb;
	output.rgb*=light;
	return output;
}


//------------------------------------
// Technique
//------------------------------------
DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
}; 
DepthStencilState EnableDepth
{
	DepthEnable = TRUE;
	DepthWriteMask = ALL;
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
BlendState PartBlend
{
	BlendEnable[0] = TRUE;
	SrcBlend = SRC_ALPHA;
	DestBlend = INV_SRC_ALPHA;
};
RasterizerState RenderNoCull { CullMode = none; };

technique11 simul_sky
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

technique11 simul_sky_CUBEMAP
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
	//	SetBlendState(DoBlend,float4( 0.0f, 0.0f, 0.0f, 0.5f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Cubemap()));
        SetGeometryShader(CompileShader(gs_4_0,GS_Cubemap()));
		SetPixelShader(CompileShader(ps_4_0,PS_Mainc()));
    }
}

technique11 simul_show_sky_texture
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		//SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Fade3DTo2D()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Fade3DTo2D()));
    }
}

technique11 simul_fade_3d_to_2d
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		//SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Fade3DTo2D()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Fade3DTo2D()));
    }
}

technique11 simul_sun
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


technique11 simul_flare
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


technique11 simul_query
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

technique11 simul_planet
{
    pass p0 
    {		
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Sun()));
		SetPixelShader(CompileShader(ps_4_0,PS_Planet()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}
technique11 draw_cubemap
{
    pass p0 
    {		
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
		SetPixelShader(CompileShader(ps_4_0,PS_Cubemap()));
		SetDepthStencilState( EnableDepth, 0 );
		//SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}