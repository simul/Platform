#define uniform
#define vec2 float2
#define vec3 float3
#define vec4 float4
#define sampler1D texture1D
#define sampler2D texture2D
#define sampler3D texture3D

#include "CppHLSL.hlsl"
uniform sampler2D inputTexture;
uniform sampler3D densityTexture;
uniform sampler2D maskTexture;
uniform sampler3D lightTexture;
uniform sampler3D ambientTexture;
uniform sampler3D volumeNoiseTexture;
#include "../../CrossPlatform/states.sl"
#include "../../CrossPlatform/simul_gpu_clouds.sl"

struct vertexOutput
{
    float4 hPosition	: SV_POSITION;
	float2 texCoords	: TEXCOORD0;		
};

vertexOutput VS_Main(idOnly IN)
{
    vertexOutput OUT;
	float2 poss[4]=
	{
		{ 1.0, 0.0},
		{ 1.0, 1.0},
		{ 0.0, 0.0},
		{ 0.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	pos.y			=yRange.x+pos.y*yRange.y;
	float4 vert_pos	=float4(float2(-1.0,1.0)+2.0*vec2(pos.x,-pos.y),1.0,1.0);
    OUT.hPosition	=vert_pos;
    OUT.texCoords	=pos;
    return OUT;
}

float4 PS_DensityMask(vertexOutput IN) : SV_TARGET
{
	float dr					=0.1;
	vec2 pos					=2.0*IN.texCoords.xy-vec2(1.0,1.0);
	float r						=length(pos);
	float dens					=saturate((1.0-r)/dr);
    return float4(dens,dens,dens,1.0);
}
float4 PS_Density(vertexOutput IN) : SV_TARGET
{
	vec3 densityspace_texcoord	=assemble3dTexcoord(IN.texCoords.xy);
	vec3 noisespace_texcoord	=densityspace_texcoord*noiseScale+vec3(1.0,1.0,0);
	float noise_val				=NoiseFunction(noisespace_texcoord,octaves,persistence,time);
	float hm					=humidity*GetHumidityMultiplier(densityspace_texcoord.z)*maskTexture.Sample(clampSamplerState,densityspace_texcoord.xy).x;
	float diffusivity			=0.02;
	float dens					=saturate((noise_val+hm-1.0)/diffusivity);
	dens						*=saturate(densityspace_texcoord.z/zPixel-0.5)*saturate((1.0-0.5*zPixel-densityspace_texcoord.z)/zPixel);
    return vec4(dens,0,0,1.0);
}
static const float glow=0.1;
float4 PS_Lighting(vertexOutput IN) : SV_TARGET
{
	vec2 texcoord				=IN.texCoords.xy;//+texCoordOffset;
	vec2 previous_light			=texture_wrap(inputTexture,texcoord.xy);
	vec3 lightspace_texcoord	=vec3(texcoord.xy,zPosition);
	vec3 densityspace_texcoord	=mul(transformMatrix,vec4(lightspace_texcoord,1.0)).xyz;
	float density				=texture_wwc(densityTexture,densityspace_texcoord).x;
	vec2 unity					=vec2(1.0,1.0);
	if(density==0)
		previous_light			=unity-exp(-.1*zPixel)*(unity-previous_light);
	float direct_light			=previous_light.x*exp(-extinctions.x*density);
	float indirect_light		=previous_light.y*exp(-extinctions.y*density);
	//indirect_light				+=(direct_light+indirect_light)*glow*density;
    return						vec4(direct_light,indirect_light,0,0);
}

float4 PS_Transform(vertexOutput IN) : SV_TARGET
{
	vec3 densityspace_texcoord	=assemble3dTexcoord(IN.texCoords.xy);
	vec3 ambient_texcoord		=vec3(densityspace_texcoord.xy,1.0-zPixel/2.0-densityspace_texcoord.z);
	vec3 lightspace_texcoord	=mul(transformMatrix,vec4(densityspace_texcoord,1.0)).xyz;
	lightspace_texcoord.z		-=zPixel;
	vec2 light_lookup			=saturate(texture_wwc(lightTexture,lightspace_texcoord).xy);
	vec2 amb_texel				=texture_wwc(ambientTexture,ambient_texcoord).xy;
	float ambient_lookup		=saturate(0.5*(amb_texel.x+amb_texel.y));
	float density				=saturate(texture_wwc(densityTexture,densityspace_texcoord).x);
	return						vec4(light_lookup.y,light_lookup.x,density,ambient_lookup);
}

//------------------------------------
// Technique
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
RasterizerState RenderNoCull { CullMode = none; };

technique11 density_mask
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_DensityMask()));
    }
}
technique11 simul_gpu_density
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Density()));
    }
}

technique11 simul_gpu_lighting
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Lighting()));
    }
}

technique11 simul_gpu_transform
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Transform()));
    }
}