#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/SL/spherical_harmonics_constants.sl"
#include "../../CrossPlatform/SL/noise.sl"
#include "../../CrossPlatform/SL/spherical_harmonics.sl"
// The cubemap input we are creating coefficients for.
TextureCube cubemapTexture;
// A texture (l_max+1)^2 of coefficients.
RWStructuredBuffer<float4> targetBuffer;
// A buffer of nxn random sample positions. The higher res, the more accurate.
RWStructuredBuffer<SphericalHarmonicsSample> samplesBufferRW;
StructuredBuffer<SphericalHarmonicsSample> samplesBuffer;

[numthreads(8,8,1)]
void CS_Jitter(uint3 sub_pos: SV_DispatchThreadID )
{
	SH_setup_spherical_samples(samplesBufferRW,sub_pos.xy,sqrtJitterSamples,MAX_SH_BANDS);
}

[numthreads(1,1,1)]
void CS_Clear(uint3 sub_pos: SV_DispatchThreadID )
{
	targetBuffer[sub_pos.x]				=vec4(0,0,0,0); 
}

[numthreads(1,1,1)]
void CS_Encode(uint3 sub_pos: SV_DispatchThreadID )
{
	// The sub_pos gives the co-ordinate in the table of sam
	const float weight				=4.0*PI; 
	// divide the result by weight and number of samples 
	float factor						=weight*invNumJitterSamples; 
#if 0
	SphericalHarmonicsSample sample		=samplesBuffer[sub_pos.x];
	vec4 colour							=cubemapTexture.SampleLevel(wrapSamplerState,-sample.dir,0);
	for(int n=0;n<num_bands;n++)
	{ 
		targetBuffer[n]					+=colour;//*factor*sample.coeff[n]; 
	} 
#else
	//if(sub_pos.x<num_bands)
	for(int n=0;n<numJitterSamples;n++)
	{ 
		SphericalHarmonicsSample sample	=samplesBuffer[n];
		vec4 colour						=cubemapTexture.SampleLevel(wrapSamplerState,-sample.dir,0);
		targetBuffer[sub_pos.x]			+=colour*factor*sample.coeff[sub_pos.x]; 
	}
#endif
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

