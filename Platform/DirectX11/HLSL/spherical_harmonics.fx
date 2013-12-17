#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/spherical_harmonics_constants.sl"
#include "../../CrossPlatform/spherical_harmonics.sl"
// The cubemap input we are creating coefficients for.
TextureCube cubemapTexture;
// A texture (l+1)^2 of coefficients.
RWStructuredBuffer<float4> targetBuffer;
// A buffer of nxn random sample positions. The higher res, the more accurate.
RWStructuredBuffer<SphericalHarmonicsSample> samplesBufferRW;
StructuredBuffer<SphericalHarmonicsSample> samplesBuffer;

[numthreads(8,8,1)]
void CS_Jitter(uint3 sub_pos: SV_DispatchThreadID )
{
//	samplesBufferRW[sub_pos.y*16+sub_pos.x].dir=vec3(0,0,.7);
//	samplesBufferRW[sub_pos.y*16+sub_pos.x].theta=0;
	SH_setup_spherical_samples(samplesBufferRW,sub_pos.xy,16,3);
}

[numthreads(8,1,1)]
void CS_Clear(uint3 sub_pos: SV_DispatchThreadID )
{
	targetBuffer[sub_pos.x]				=vec4(0,0,0,0); 
}

[numthreads(1,1,1)]
void CS_Encode(uint3 sub_pos: SV_DispatchThreadID )
{
	SphericalHarmonicsSample sample	=samplesBuffer[sub_pos.x];
	// The sub_pos gives the co-ordinate in the table of samples.
	vec4 colour						=cubemapTexture.SampleLevel(wrapSamplerState,sample.dir,0);
	const double weight				=4.0*PI; 
	// for each sample
	double theta					=sample.theta; 
	double phi						=sample.phi; 
	// divide the result by weight and number of samples 
	double factor					=weight/1024.0; 
	for(int n=0; n<16; ++n)
	{ 
		targetBuffer[n]				+=colour*factor;//sample.coeff[n]*factor; 
	} 
}

technique11 jitter
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_Jitter()));
    }
}

technique11 clear
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_Clear()));
    }
}

technique11 encode
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_Encode()));
    }
}

