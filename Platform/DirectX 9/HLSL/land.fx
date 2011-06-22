string description = "Helper techniques for rainbow effects. render fog factor to dest alpha etc.";

float Script : STANDARDSGLOBAL <
    string UIWidget = "none";
    string ScriptClass = "object";
    string ScriptOrder = "standard";
    string ScriptOutput = "color";
    string Script = "Technique=Technique?texturedWithFogFactor:blackWithFogFactor:skyBoxMoisture;";
> = 0.8;

//------------------------------------
float4x4 worldViewProj : WorldViewProjection;
float4x4 world   : World;
float4x4 worldInverseTranspose : WorldInverseTranspose;
float4x4 viewInverse : ViewInverse;

texture diffuseTexture : DiffuseMap
<
	string Name = "default_color.dds";
>;

float4 lightDir : Direction
<
	string UIObject = "DirectionalLight";
    string Space = "World";
> = {1.0f, -1.0f, 1.0f, 0.0f};

float4 lightColor : Diffuse
<
    string UIName = "Diffuse Light Color";
    string UIObject = "DirectionalLight";
> = {1.0f, 1.0f, 1.0f, 1.0f};

float4 lightAmbient : MaterialAmbient
<
    string UIType = "Ambient Light Color";
> = {0.0f, 0.0f, 0.0f, 1.0f};

float4 materialDiffuse : MaterialDiffuse
<
    string UIType = "Surface Color";
> = {1.0f, 1.0f, 1.0f, 1.0f};

float4 materialSpecular : MaterialSpecular
<
	string UIType = "Surface Specular";
> = {0.0f, 0.0f, 0.0f, 0.0f};

float shininess : Power
<
    string UIType = "slider";
    float UIMin = 1.0;
    float UIMax = 128.0;
    float UIStep = 1.0;
    string UIName = "specular power";
> = 30.0;

float fogNear 
<
    string UIType = "slider";
    float UIMin = 0.0;
    float UIMax = 1000.0;
    float UIStep = 1.0;
    string UIName = "fog near";
> = 1.0;


float fogFar 
<
    string UIType = "slider";
    float UIMin = 0.0;
    float UIMax = 1000.0;
    float UIStep = 1.0;
    string UIName = "fog far";
> = 20.0;

//------------------------------------
struct vertexInput {
    float3 position				: POSITION;
    float3 normal				: NORMAL;
    float4 texCoordDiffuse		: TEXCOORD0;
};

struct vertexOutput {
    float4 hPosition		: POSITION;
    float4 texCoordDiffuse	: TEXCOORD0;
    float4 fogFactor		: TEXCOORD1;
    float4 diffAmbColor		: COLOR0;
    float4 specCol			: COLOR1;
};


//------------------------------------
vertexOutput VS_TransformAndTexture(vertexInput IN) 
{
    vertexOutput OUT;
	IN.position.y*=.1;
    OUT.hPosition = mul( float4(IN.position.xyz , 1.0) , worldViewProj);
    OUT.texCoordDiffuse = IN.texCoordDiffuse;

	//calculate our vectors N, E, L, and H
	float3 worldEyePos = viewInverse[3].xyz;
    float3 worldVertPos = mul(IN.position, world).xyz;
	float4 N = mul(IN.normal, worldInverseTranspose); //normal vector
    float3 E = normalize(worldEyePos - worldVertPos); //eye vector
    float3 L = normalize( -lightDir.xyz); //light vector
    float3 H = normalize(E + L); //half angle vector

	//calculate the diffuse and specular contributions
    float  diff = max(0 , dot(N,L));
    float  spec = pow( max(0 , dot(N,H) ) , shininess );
    if( diff <= 0 )
    {
        spec = 0;
    }

	//output diffuse
    float4 ambColor = materialDiffuse * lightAmbient;
    float4 diffColor = materialDiffuse * diff * lightColor ;
    OUT.diffAmbColor = diffColor + ambColor;

	//output specular
    float4 specColor = materialSpecular * lightColor * spec;
    OUT.specCol = specColor;
    
    OUT.fogFactor = (OUT.hPosition.z  - fogNear)/ (fogFar - fogNear);

    return OUT;
}


//------------------------------------
vertexOutput VS_TransformAndFogFactor(vertexInput IN) 
{
    vertexOutput OUT;
    OUT.hPosition = mul( float4(IN.position.xyz , 1.0) , worldViewProj);
    OUT.texCoordDiffuse = IN.texCoordDiffuse;
    OUT.diffAmbColor = float4(0,0,0,0);
    OUT.specCol = float4(0,0,0,0);
    
    OUT.fogFactor =  (OUT.hPosition.z  - fogNear)/ (fogFar - fogNear) ;

    return OUT;
}

//------------------------------------
sampler TextureSampler = sampler_state 
{
    texture = <diffuseTexture>;
    AddressU  = CLAMP;        
    AddressV  = CLAMP;
    AddressW  = CLAMP;
    MIPFILTER = LINEAR;
    MINFILTER = LINEAR;
    MAGFILTER = LINEAR;
};

sampler TextureSamplerGround = sampler_state 
{
    texture = <diffuseTexture>;
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = CLAMP;
    MIPFILTER = LINEAR;
    MINFILTER = LINEAR;
    MAGFILTER = LINEAR;
};


//-----------------------------------
float4 PS_Textured( vertexOutput IN): COLOR
{
  float4 diffuseTexture = tex2D( TextureSamplerGround, IN.texCoordDiffuse );
  float4 finalColor = IN.diffAmbColor * diffuseTexture + IN.specCol;
  finalColor.a = IN.fogFactor;
  return finalColor;
}

float4 PS_BlackWithFog( vertexOutput IN): COLOR
{
  float4 finalColor = float4(0,0,0,IN.fogFactor.x);
  return finalColor;
}

float4 PS_AlphaOut( vertexOutput IN): COLOR
{
  return tex2D( TextureSampler, IN.texCoordDiffuse ).aaaa;
}

//-----------------------------------
technique texturedWithFogFactor <
	string Script = "Pass=p0;";
> {
    pass p0  <
		string Script = "Draw=geometry;";
    > {		
		VertexShader = compile vs_2_0 VS_TransformAndTexture();
		PixelShader  = compile ps_2_0 PS_Textured();
    }
}


technique blackWithFogFactor <
	string Script = "Pass=p0;";
> {
    pass p0  <
		string Script = "Draw=geometry;";
    > {		
		VertexShader = compile vs_2_0 VS_TransformAndFogFactor();
		PixelShader  = compile ps_2_0 PS_BlackWithFog();
    }
}
technique skyBoxMoisture <
	string Script = "Pass=p0;";
> {
    pass p0  <
		string Script = "Draw=geometry;";
    > {		
		VertexShader = compile vs_2_0 VS_TransformAndTexture();
		PixelShader  = compile ps_2_0 PS_AlphaOut();
    }
}

