#version 330
#include "simul_inscatter_fns.hlsl"
#include "simul_gpu_sky.hlsl"

uniform texture2D input_insc_texture;
uniform texture1D density_texture;
uniform texture3D loss_texture;

uniform float maxDistanceKm;
uniform float3 sunIrradiance;
uniform float3 lightDir;

struct vertexInput
{
    float3 position		: POSITION;
    float4 texCoords	: TEXCOORD0;
};

struct vertexOutput
{
    float4 hPosition	: SV_POSITION;
	float2 texc			: TEXCOORD0;		

float4 PS_Loss(void)
{
	vec4 previous_loss	=texture(input_loss_texture,texc.xy);
	float sin_e			=1.0-2.0*(texc.y*texSize.y-0.5)/(texSize.y-1.0);
	float cos_e			=sqrt(1.0-sin_e*sin_e);
	float altTexc		=(texc.x*texSize.x-0.5)/(texSize.x-1.0);
	float viewAltKm		=altTexc*altTexc*maxOutputAltKm;
	float spaceDistKm	=getDistanceToSpace(sin_e,viewAltKm);
	float maxd			=min(spaceDistKm,distKm);
	float mind			=min(spaceDistKm,prevDistKm);
	float dist			=0.5*(mind+maxd);
	float stepLengthKm	=max(0.0,maxd-mind);
	float y				=planetRadiusKm+viewAltKm+dist*sin_e;
	float x				=dist*cos_e;
	float r				=sqrt(x*x+y*y);
	float alt_km		=r-planetRadiusKm;
	// lookups is: dens_factor,ozone_factor,haze_factor;
	float dens_texc		=(alt_km/maxDensityAltKm*(tableSize.x-1.0)+0.5)/tableSize.x;
	vec4 lookups		=texture(density_texture,dens_texc);
	float dens_factor	=lookups.x;
	float ozone_factor	=lookups.y;
	float haze_factor	=getHazeFactorAtAltitude(alt_km);
	vec3 extinction		=dens_factor*rayleigh+haze_factor*hazeMie+ozone*ozone_factor;
	vec4 loss;
	loss.rgb			=exp(-extinction*stepLengthKm);
	loss.a				=(loss.r+loss.g+loss.b)/3.0;
//loss.rgb	*=0.5;//=vec3(alt_km/maxDensityAltKm,stepLengthKm/512.0,stepLengthKm/512.0);
	loss				*=previous_loss;
//outColor.r=1.0;
//outColor.a=1.0;
    outColor			=loss;
}


void PS_Insc(void)
{
	float4 previous_insc	=texture(input_insc_texture,texc.xy);
	float3 previous_loss	=texture(loss_texture,float3(texc.xy,distKm/maxDistanceKm)).rgb;// should adjust texc - we want the PREVIOUS loss!
	float sin_e			=1.0-2.0*(texc.y*texSize.y-0.5)/(texSize.y-1.0);
	float cos_e			=sqrt(1.0-sin_e*sin_e);
	float altTexc		=(texc.x*texSize.x-0.5)/(texSize.x-1.0);
	float viewAltKm		=altTexc*altTexc*maxOutputAltKm;
	float spaceDistKm	=getDistanceToSpace(sin_e,viewAltKm);
	float maxd			=min(spaceDistKm,distKm);
	float mind			=min(spaceDistKm,prevDistKm);
	float dist			=0.5*(mind+maxd);
	float stepLengthKm	=max(0.0,maxd-mind);
	float y				=planetRadiusKm+viewAltKm+dist*sin_e;
	float x				=dist*cos_e;
	float r				=sqrt(x*x+y*y);
	float alt_km		=r-planetRadiusKm;
	
	float x1			=mind*cos_e;
	float r1			=sqrt(x1*x1+y*y);
	float alt_1_km		=r1-planetRadiusKm;
	
	float x2			=maxd*cos_e;
	float r2			=sqrt(x2*x2+y*y);
	float alt_2_km		=r2-planetRadiusKm;
	
	// lookups is: dens_factor,ozone_factor,haze_factor;
	float dens_texc		=(alt_km/maxDensityAltKm*(tableSize.x-1.0)+0.5)/tableSize.x;
	float4 lookups		=texture(density_texture,dens_texc);
	float dens_factor	=lookups.x;
	float ozone_factor	=lookups.y;
	float haze_factor	=getHazeFactorAtAltitude(alt_km);
	float4 light			=getSunlightFactor(alt_km,lightDir)*float4(sunIrradiance,1.0);
	float4 insc			=light;
#ifdef OVERCAST
	//insc*=1.0-getOvercastAtAltitudeRange(alt_1_km,alt_2_km);
#endif
	float3 extinction		=dens_factor*rayleigh+haze_factor*hazeMie;
	float3 total_ext		=extinction+ozone*ozone_factor;
	float3 loss			=exp(-extinction*stepLengthKm);
	insc.rgb			*=float3(1.0,1.0,1.0)-loss;
	float mie_factor	=exp(-insc.w*stepLengthKm*haze_factor*hazeMie.x);
	insc.w				=saturate((1.f-mie_factor)/(1.f-total_ext.x+0.0001f));
	
	//insc.w				=(loss.w)*(1.f-previous_insc.w)*insc.w+previous_insc.w;
	
	insc.rgb			*=previous_loss.rgb;
	insc.rgb			+=previous_insc.rgb;
	float lossw=1.0;
	insc.w				=(lossw)*(1.0-previous_insc.w)*insc.w+previous_insc.w;

    outColor			=insc;
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

technique11 simul_gpu_loss
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Loss()));
    }
}

technique11 simul_gpu_insc
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Insc()));
    }
}

technique11 simul_gpu_skyl
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Skyl()));
    }
}