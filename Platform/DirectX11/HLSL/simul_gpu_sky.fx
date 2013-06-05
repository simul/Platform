#define uniform
#define vec2 float2
#define vec3 float3
#define vec4 float4
#define sampler1D texture1D
#define sampler2D texture2D
#define sampler3D texture3D
#define texture(tex,texc) tex.Sample(samplerState,texc)
#define texture_clamp(tex,texc) tex.Sample(samplerStateClamp,texc)
#define texture_nearest(tex,texc) tex.Sample(samplerStateNearest,texc)
uniform sampler2D input_texture;
uniform sampler1D density_texture;
uniform sampler3D loss_texture;
uniform sampler3D insc_texture;

uniform sampler2D optical_depth_texture;

SamplerState samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
	AddressW = Clamp;
};
SamplerState samplerStateClamp 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
	AddressW = Clamp;
};
SamplerState samplerStateNearest 
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Clamp;
	AddressV = Clamp;
	AddressW = Clamp;
};


#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "CppHlsl.hlsl"
#include "../../CrossPlatform/simul_gpu_sky.sl"

struct vertexInput
{
    float3 position		: POSITION;
    float2 texc			: TEXCOORD0;
};

struct vertexOutput
{
    float4 hPosition	: SV_POSITION;
	float2 texc			: TEXCOORD0;		
};

vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
    OUT.hPosition = float4(IN.position.xy,1.0,1.0);
	OUT.texc=IN.texc;
    return OUT;
}

float4 PS_Loss(vertexOutput IN) : SV_TARGET
{
	vec4 previous_loss	=texture(input_texture,IN.texc.xy);
	float sin_e			=clamp(1.0-2.0*(IN.texc.y*texSize.y-texelOffset)/(texSize.y-1.0),-1.0,1.0);
	float cos_e			=sqrt(1.0-sin_e*sin_e);
	float altTexc		=(IN.texc.x*texSize.x-texelOffset)/(texSize.x-1.0);
	float viewAltKm		=altTexc*altTexc*maxOutputAltKm;
	float spaceDistKm	=getDistanceToSpace(sin_e,viewAltKm);
	float maxd			=min(spaceDistKm,distanceKm);
	float mind			=min(spaceDistKm,prevDistanceKm);
	float dist			=0.5*(mind+maxd);
	float stepLengthKm	=max(0.0,maxd-mind);
	float y				=planetRadiusKm+viewAltKm+dist*sin_e;
	float x				=dist*cos_e;
	float r				=sqrt(x*x+y*y);
	float alt_km		=r-planetRadiusKm;
	// lookups is: dens_factor,ozone_factor,haze_factor;
	float dens_texc		=(alt_km/maxDensityAltKm*(tableSize.x-1.0)+texelOffset)/tableSize.x;
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
    return			loss;
}


float4 PS_Insc(vertexOutput IN) : SV_TARGET
{
	float4 previous_insc	=texture_nearest(input_texture,IN.texc.xy);
	float3 previous_loss	=texture_nearest(loss_texture,float3(IN.texc.xy,pow(distanceKm/maxDistanceKm,0.5))).rgb;// should adjust texc - we want the PREVIOUS loss!
	float sin_e			=clamp(1.0-2.0*(IN.texc.y*texSize.y-texelOffset)/(texSize.y-1.0),-1.0,1.0);
	float cos_e			=sqrt(1.0-sin_e*sin_e);
	float altTexc		=(IN.texc.x*texSize.x-texelOffset)/(texSize.x-1.0);
	float viewAltKm		=altTexc*altTexc*maxOutputAltKm;
	float spaceDistKm	=getDistanceToSpace(sin_e,viewAltKm);
	float maxd			=min(spaceDistKm,distanceKm);
	float mind			=min(spaceDistKm,prevDistanceKm);
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
	float dens_texc		=(alt_km/maxDensityAltKm*(tableSize.x-1.0)+texelOffset)/tableSize.x;
	float4 lookups		=texture(density_texture,dens_texc);
	float dens_factor	=lookups.x;
	float ozone_factor	=lookups.y;
	float haze_factor	=getHazeFactorAtAltitude(alt_km);
	float4 light		=float4(sunIrradiance,1.0)*getSunlightFactor(alt_km,lightDir);
	float4 insc			=light;
	insc				*=1.0-getOvercastAtAltitudeRange(alt_1_km,alt_2_km);
	float3 extinction	=dens_factor*rayleigh+haze_factor*hazeMie;
	float3 total_ext	=extinction+ozone*ozone_factor;
	float3 loss			=exp(-extinction*stepLengthKm);
	insc.rgb			*=float3(1.0,1.0,1.0)-loss;
	float mie_factor	=exp(-insc.w*stepLengthKm*haze_factor*hazeMie.x);
	insc.w				=saturate((1.f-mie_factor)/(1.f-total_ext.x+0.0001f));
	
	insc.rgb			*=previous_loss.rgb;
	insc.rgb			+=previous_insc.rgb;
	float lossw=1.0;
	insc.w				=(lossw)*(1.0-previous_insc.w)*insc.w+previous_insc.w;

    return			insc;
}

// What spectral radiance is added on a light path towards the viewer, due to illumination of
// a volume of air by the surrounding sky?
// in cpp:
//	float cos0=dir_to_sun.z;
//	Skylight=GetAnisotropicInscatterFactor(true,hh,pif/2.f,0,1e5f,dir_to_sun,dir_to_moon,haze,false,1);
//	Skylight*=GetInscatterAngularMultiplier(cos0,Skylight.w,hh);
vec3 getSkylight(float alt_km)
{
// The inscatter factor, at this altitude looking straight up, is given by:
	vec4 insc		=texture(insc_texture,vec3(sqrt(alt_km/maxOutputAltKm),0.0,1.0));
	vec3 skylight	=InscatterFunction(insc,hazeEccentricity,0.0,mieRayleighRatio);
	return skylight;
}

float4 PS_Skyl(vertexOutput IN) : SV_TARGET
{
	vec4 previous_skyl	=texture(input_texture,IN.texc.xy);
	vec3 previous_loss	=texture(loss_texture,vec3(IN.texc.xy,pow(distanceKm/maxDistanceKm,0.5))).rgb;
	// should adjust texc - we want the PREVIOUS loss!
	float sin_e			=clamp(1.0-2.0*(IN.texc.y*texSize.y-texelOffset)/(texSize.y-1.0),-1.0,1.0);
	float cos_e			=sqrt(1.0-sin_e*sin_e);
	float altTexc		=(IN.texc.x*texSize.x-texelOffset)/(texSize.x-1.0);
	float viewAltKm		=altTexc*altTexc*maxOutputAltKm;
	float spaceDistKm	=getDistanceToSpace(sin_e,viewAltKm);
	float maxd			=min(spaceDistKm,distanceKm);
	float mind			=min(spaceDistKm,prevDistanceKm);
	float dist			=0.5*(mind+maxd);
	float stepLengthKm	=max(0.0,maxd-mind);
	float y				=planetRadiusKm+viewAltKm+dist*sin_e;
	float x				=dist*cos_e;
	float r				=sqrt(x*x+y*y);
	float alt_km		=r-planetRadiusKm;
	// lookups is: dens_factor,ozone_factor,haze_factor;
	float dens_texc		=(alt_km/maxDensityAltKm*(tableSize.x-1.0)+texelOffset)/tableSize.x;
	vec4 lookups		=texture(density_texture,dens_texc);
	float dens_factor	=lookups.x;
	float ozone_factor	=lookups.y;
	float haze_factor	=getHazeFactorAtAltitude(alt_km);
	vec4 light			=vec4(starlight+getSkylight(alt_km),0.0);
	vec4 skyl			=light;
	vec3 extinction		=dens_factor*rayleigh+haze_factor*hazeMie;
	vec3 total_ext		=extinction+ozone*ozone_factor;
	vec3 loss			=exp(-extinction*stepLengthKm);
	skyl.rgb			*=vec3(1.0,1.0,1.0)-loss;
	float mie_factor	=exp(-skyl.w*stepLengthKm*haze_factor*hazeMie.x);
	skyl.w				=saturate((1.f-mie_factor)/(1.f-total_ext.x+0.0001f));
	
	//skyl.w				=(loss.w)*(1.f-previous_skyl.w)*skyl.w+previous_skyl.w;
	skyl.rgb			*=previous_loss.rgb;
	skyl.rgb			+=previous_skyl.rgb;
	float lossw=1.0;
	skyl.w				=(lossw)*(1.0-previous_skyl.w)*skyl.w+previous_skyl.w;
    return skyl;
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