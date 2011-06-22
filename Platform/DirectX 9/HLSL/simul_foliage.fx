float4x4 worldViewProj	: WorldViewProjection;

sampler3D cloudDensity:TEXUNIT0= sampler_state 
{
};

sampler3D nextCloudDensity:TEXUNIT1= sampler_state 
{
};

sampler2D noiseSampler:TEXUNIT2= sampler_state 
{
};

float4 eyePosition : EYEPOSITION_WORLDSPACE;
// Light response: primary,secondary,anisotropic,ambient
float4 lightResponse;
float3 lightDir : Direction;
float3 skylightColour;
float3 sunlightColour;
float4 fractalScale;
float2 interp={0,1};

struct vertexInput
{
    float3 position			: POSITION;
    float3 loss				: TEXCOORD0;
    float3 inscatter		: TEXCOORD1;
    float3 texCoords		: TEXCOORD2;
    float layerDensity		: TEXCOORD3;
    float2 texCoordsNoise	: TEXCOORD4;
};

struct vertexOutput
{
    float4 hPosition		: POSITION;
    float2 texCoordsNoise	: TEXCOORD0;
	float layerDensity		: TEXCOORD2;
    float3 texCoords		: TEXCOORD4;
    float3 loss				: TEXCOORD5;
	float3 wPosition		: TEXCOORD6;
    float3 inscatter		: TEXCOORD7;
};

vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
    OUT.hPosition = mul( worldViewProj, float4(IN.position.xyz , 1.0));
	
	OUT.texCoords=IN.texCoords;
	const float c=fractalScale.w;
	OUT.texCoordsNoise=IN.texCoordsNoise;
	OUT.loss=IN.loss;
	OUT.inscatter=IN.inscatter;
	OUT.wPosition=normalize(IN.position.xyz-eyePosition.xyz);
	OUT.layerDensity=IN.layerDensity;
    return OUT;
}

half4 PS_Main( vertexOutput IN): color
{
	float3 view=normalize(IN.wPosition);
	float cos0=saturate(dot(lightDir.xyz,view.xyz));
	float c3=cos0*cos0;
	float3 noiseval=tex2D(noiseSampler,IN.texCoordsNoise.xy).xyz;
	//c3*=c3;
	//float3 pos=IN.texCoords.xyz;
	float4 density=tex3D(cloudDensity,IN.texCoords.xyz);
	float4 density2=float4(0,0,0,0);
density2=tex3D(nextCloudDensity,IN.texCoords.xyz);

	//density*=interp.y;
	density+=density2;

	//if(density.x<=0)
	//	return half4(0,0,0,0.f);
	float Beta=lightResponse.x;//+c3*lightResponse.y;
	//float3 ambient=density.w*skylightColour.rgb;

	float opacity=density.x+noiseval.x-1.f+IN.layerDensity-1.f;
	float3 final=(density.z*Beta+lightResponse.w*density.y)*sunlightColour;//+ambient.rgb;
	
	//final*=IN.loss.xyz;
	//final+=IN.inscatter.xyz;

    return half4(noiseval.rgb*final.rgb,opacity);
}


technique simul_foliage
{
    pass p0 
    {
		zenable = true;
		DepthBias =0;
		SlopeScaleDepthBias =0;
		ZWriteEnable = true;
		alphablendenable = false;
        CullMode = None;
		AlphaTestEnable=true;
		FillMode = Solid;
        AlphaBlendEnable = false;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		
		// We would LIKE to do the following:
		//SeparateAlphaEnable = true;
		//SrcBlendAlpha = Zero;
		//DestBlendAlpha = InvSrcAlpha;
		// but it's not implemented!

		VertexShader = compile vs_3_0 VS_Main();
		PixelShader  = compile ps_3_0 PS_Main();
    }
}

