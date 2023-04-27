#include "SphericalHarmonics.h"
#include "Macros.h"
#include "GpuProfiler.h"
#include "RenderPlatform.h"
using namespace simul;
using namespace crossplatform;


SphericalHarmonics::SphericalHarmonics()
	:bands(4)
	,renderPlatform(nullptr)
	,sphericalHarmonicsEffect(nullptr)
	,shSeed(0)
	,lightProbesEffect(nullptr)
{
}

SphericalHarmonics::~SphericalHarmonics()
{
	InvalidateDeviceObjects();
}

void SphericalHarmonics::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform = r;
	// The table of coefficients.
	int s = (bands + 1);
	if (s<4)
		s = 4;
	sphericalHarmonics.InvalidateDeviceObjects();
	lightProbeConstants.RestoreDeviceObjects(renderPlatform);

	sphericalHarmonicsConstants.RestoreDeviceObjects(r);
	sphericalSamples.InvalidateDeviceObjects();
	static int seed = 0;
	seed = seed % 1001;
	shSeed=seed++;
	probeResultsRW.InvalidateDeviceObjects();
	probeResultsRW.RestoreDeviceObjects(renderPlatform,256,true,false,nullptr,"probeResultsRW");
}

void SphericalHarmonics::RecompileShaders()
{
	if (!renderPlatform)
		return;
	SAFE_DESTROY(renderPlatform, sphericalHarmonicsEffect);
	SAFE_DESTROY(renderPlatform, lightProbesEffect);
	sphericalHarmonicsEffect	=renderPlatform->CreateEffect("spherical_harmonics");
	sphericalHarmonicsConstants.LinkToEffect(sphericalHarmonicsEffect, "SphericalHarmonicsConstants");
	jitter							=sphericalHarmonicsEffect->GetTechniqueByName("jitter");
	encode							=sphericalHarmonicsEffect->GetTechniqueByName("encode");
	_samplesBuffer					=sphericalHarmonicsEffect->GetShaderResource("samplesBuffer");
	_targetBuffer					=sphericalHarmonicsEffect->GetShaderResource("targetBuffer");
	_samplesBufferRW				=sphericalHarmonicsEffect->GetShaderResource("samplesBufferRW");
	_cubemapTexture					=sphericalHarmonicsEffect->GetShaderResource("cubemapTexture");

	lightProbesEffect				=renderPlatform->CreateEffect("light_probes");
	lightProbeConstants.LinkToEffect(lightProbesEffect, "LightProbeConstants");
	auto group						=lightProbesEffect->GetTechniqueGroupByName("mip_from_roughness");
	mip_from_roughness_blend		=group->GetTechniqueByName("mip_from_roughness_blend");
	mip_from_roughness_no_blend		=group->GetTechniqueByName("mip_from_roughness_no_blend");	
	_basisBuffer					=lightProbesEffect->GetShaderResource("basisBuffer");
}

void SphericalHarmonics::InvalidateDeviceObjects()
{
	ResetBuffers();
	lightProbeConstants.InvalidateDeviceObjects();
	sphericalHarmonicsConstants.InvalidateDeviceObjects();
	SAFE_DESTROY(renderPlatform,sphericalHarmonicsEffect);
	SAFE_DESTROY(renderPlatform, lightProbesEffect);
}

void SphericalHarmonics::ResetBuffers()
{
	sphericalSamples.InvalidateDeviceObjects();
	sphericalHarmonics.InvalidateDeviceObjects();
	probeResultsRW.InvalidateDeviceObjects();
}

void SphericalHarmonics::ClearBuffers(GraphicsDeviceContext &deviceContext)
{
}

float RoughnessFromMip(float mip, float numMips)
{
	static float  roughness_mip_scale = 1.2f;
	return exp2(( 3.0f+mip-numMips) / roughness_mip_scale);
}

void SphericalHarmonics::RenderMipsByRoughness(GraphicsDeviceContext &deviceContext, crossplatform::Texture *target)
{
	for (int j = 0; j < 6; j++)
	{
		for (int m = 0; m < target->mips - 1; m++)
		{
			CopyMip(deviceContext,target,j,m,0.0f);
		}
	}
}

void SphericalHarmonics::CopyMip(GraphicsDeviceContext &deviceContext,Texture *tex,int face,int src_mip,float blend)
{
	if (!lightProbesEffect)
	{
		RecompileShaders();
	}
	renderPlatform->WaitForFencedResources(deviceContext);
	lightProbeConstants.alpha = 1.0f-blend;
	lightProbeConstants.numMips = tex->mips;
	lightProbeConstants.cubeFace = face;
	lightProbeConstants.mipIndex = src_mip + 1;
	lightProbesEffect->UnbindTextures(deviceContext);
	const float maxMips = 8;
	lightProbeConstants.roughness = RoughnessFromMip((float)lightProbeConstants.mipIndex, maxMips);
	renderPlatform->SetConstantBuffer(deviceContext, &lightProbeConstants);

	crossplatform::EffectTechnique *tech		= nullptr;
	if (blend > 0.0f)
	{
		tech = mip_from_roughness_blend;
	}
	else
	{
		tech = mip_from_roughness_no_blend;
	}
	const char *passname = (lightProbeConstants.roughness < 0.01f) ? "smooth" : (lightProbeConstants.roughness < 0.99f ? "general" : "rough");
	// The source is the i'th mip of the faceIndex face of the cubemap texture.
	lightProbesEffect->SetTexture(deviceContext, "sourceCubemap", tex, {0, 1, 0, -1});
	// The target is the (i+1)'th mip of the faceIndex face.
	tex->activateRenderTarget(deviceContext, { tex->GetShaderResourceTypeForRTVAndDSV(), { src_mip + 1, 1, face, 1}});
	lightProbesEffect->Apply(deviceContext, tech, passname); 
	renderPlatform->DrawQuad(deviceContext);
	lightProbesEffect->UnbindTextures(deviceContext);
	lightProbesEffect->Unapply(deviceContext);
	tex->deactivateRenderTarget(deviceContext);
	
}

bool SphericalHarmonics::Probe(crossplatform::DeviceContext &deviceContext
	,Texture *buffer_texture
	,int mip_size
	,int face_index
	,uint2 pos
	,uint2 size
	,vec4 *targetValuesFloat4)
{
	crossplatform::EffectTechnique *tech=sphericalHarmonicsEffect->GetTechniqueByName("probe_query");

	sphericalHarmonicsEffect->SetTexture(deviceContext,"cubemapAsTexture2DArray",buffer_texture);
	if(size.x*size.y>(unsigned)probeResultsRW.count)
	{
		probeResultsRW.RestoreDeviceObjects(renderPlatform,size.x*size.y*2,true,false,nullptr,"probeResultsRW");
	}
	probeResultsRW.ApplyAsUnorderedAccessView(deviceContext, sphericalHarmonicsEffect,sphericalHarmonicsEffect->GetShaderResource("targetBuffer"));

	sphericalHarmonicsConstants.lookupOffset=uint3(pos.x,pos.y,face_index);
	sphericalHarmonicsConstants.lookupSize=size;

	sphericalHarmonicsEffect->Apply(deviceContext,tech,0);
	renderPlatform->DispatchCompute(deviceContext,size.x,size.y,1);
	sphericalHarmonicsEffect->Unapply(deviceContext);
	probeResultsRW.CopyToReadBuffer(deviceContext);
	const vec4 *res=probeResultsRW.OpenReadBuffer(deviceContext);
	if(res)
	{
		unsigned int sz=size.x*size.y;
		for(unsigned int i=0;i<sz;i++)
		{
			if (isnan(res[i].x))
			{
				return false;
			}
		}
		memcpy(targetValuesFloat4,res,sizeof(vec4)*sz);
	}
	probeResultsRW.CloseReadBuffer(deviceContext);
	return res!=0;
	//return false;

}

void SphericalHarmonics::CalcSphericalHarmonics(crossplatform::DeviceContext &deviceContext,Texture *buffer_texture)
{
	if (!bands)
		return;
	SIMUL_COMBINED_PROFILE_START(deviceContext,"Calc Spherical Harmonics")
	int num_coefficients=bands*bands;
	static int BLOCK_SIZE=16;
	static int sqrt_jitter_samples=4;
	if(!sphericalHarmonics.count)
	{
		sphericalHarmonics.RestoreDeviceObjects(renderPlatform,num_coefficients,true,true,nullptr,"spherical Harmonics");
		sphericalSamples.RestoreDeviceObjects(renderPlatform,sqrt_jitter_samples*sqrt_jitter_samples,true,true,nullptr,"sphericalSamples");
	}
	if(!sphericalHarmonicsEffect)
		RecompileShaders();
	SIMUL_COMBINED_PROFILE_START(deviceContext,"clear")
	sphericalHarmonicsConstants.num_bands			=bands;
	sphericalHarmonicsConstants.numCoefficients		=num_coefficients;
	sphericalHarmonicsConstants.sqrtJitterSamples	=sqrt_jitter_samples;
	sphericalHarmonicsConstants.numJitterSamples	=sqrt_jitter_samples*sqrt_jitter_samples;
	sphericalHarmonicsConstants.invNumJitterSamples	=1.0f/(float)sphericalHarmonicsConstants.numJitterSamples;
	sphericalHarmonicsConstants.randomSeed			=shSeed;

	SIMUL_COMBINED_PROFILE_END(deviceContext)
	SIMUL_COMBINED_PROFILE_START(deviceContext,"jitter")

	{
		// The table of 3D directional sample positions. sqrt_jitter_samples x sqrt_jitter_samples
		// We just fill this buffer_texture with random 3d directions.
		sphericalSamples.ApplyAsUnorderedAccessView(deviceContext, sphericalHarmonicsEffect, _samplesBufferRW);
		sphericalHarmonicsEffect->SetTexture(deviceContext,_cubemapTexture	,buffer_texture);
		sphericalHarmonicsEffect->SetConstantBuffer(deviceContext,&	sphericalHarmonicsConstants);
		sphericalHarmonicsEffect->Apply(deviceContext,jitter,0);
		int u = (sphericalHarmonicsConstants.numJitterSamples + BLOCK_SIZE - 1) / BLOCK_SIZE;
		// 1,1,1
		renderPlatform->DispatchCompute(deviceContext, u, 1, 1);
		sphericalHarmonicsEffect->UnbindTextures(deviceContext);
		sphericalHarmonicsEffect->SetUnorderedAccessView(deviceContext,_samplesBufferRW,NULL);
		sphericalHarmonicsEffect->Unapply(deviceContext);
	}
	static bool test=false;
	if(test)
	{
		// testing:
		sphericalSamples.CopyToReadBuffer(deviceContext);
		const SphericalHarmonicsSample* sam=(const SphericalHarmonicsSample* )sphericalSamples.OpenReadBuffer(deviceContext);
		if(sam)
		{
			for(int i=0;i<sphericalSamples.count;i++)
			{
				std::cout<<i<<": ("<<sam[i].dir.x<<","<<sam[i].dir.y<<","<<sam[i].dir.z<<") coefficients: ";
				for(int j=0;j<9;j++)
				{
					if(j)
						std::cout<<", ";
					std::cout<<sam[i].coeff[j];
				}
				std::cout<<std::endl;
			}
			std::cout<<std::endl;
		}
		sphericalSamples.CloseReadBuffer(deviceContext);
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext)
	SIMUL_COMBINED_PROFILE_START(deviceContext,"encode")
	{
		sphericalHarmonicsEffect->SetTexture(deviceContext,_cubemapTexture,buffer_texture);
		sphericalSamples.Apply(deviceContext, sphericalHarmonicsEffect, _samplesBuffer);
		sphericalHarmonics.ApplyAsUnorderedAccessView(deviceContext,sphericalHarmonicsEffect,_targetBuffer);

		static bool sh_by_samples=false;
		sphericalHarmonicsEffect->SetConstantBuffer(deviceContext,&sphericalHarmonicsConstants);
		sphericalHarmonicsEffect->Apply(deviceContext,encode,0);
		int n = sh_by_samples?sphericalHarmonicsConstants.numJitterSamples:num_coefficients;
		//9,1,1
		renderPlatform->DispatchCompute(deviceContext, n, 1, 1);

		sphericalHarmonicsConstants.Unbind(deviceContext);
		sphericalHarmonicsEffect->UnbindTextures(deviceContext);
		sphericalHarmonicsEffect->Unapply(deviceContext);
		sphericalHarmonics.CopyToReadBuffer(deviceContext);
	}
	static bool test2=false;
	if(test2)
	{
		sphericalHarmonics.CopyToReadBuffer(deviceContext);
		const vec4* h=(const vec4* )sphericalHarmonics.OpenReadBuffer(deviceContext);
		if(h)
		{
			for(int i=0;i<num_coefficients;i++)
			{
				std::cout<<i<<", RGB: ("<<h[i].x<<","<<h[i].y<<","<<h[i].z<<std::endl;
			}
			std::cout<<std::endl;
		}
		sphericalSamples.CloseReadBuffer(deviceContext);
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext)
	SIMUL_COMBINED_PROFILE_END(deviceContext)
}


void SphericalHarmonics::RenderEnvmap(GraphicsDeviceContext &deviceContext,crossplatform::Texture *target_texture,int cubemapIndex,float blend)
{
	if (!target_texture)
		return;
	if (!lightProbesEffect)
	{
		RecompileShaders();
	}
	if (!lightProbesEffect)
		return;
	math::Matrix4x4 invViewProj;
	//mat4 view;
	//float cam_pos[] = { 0,0,0 };
	crossplatform::EffectTechnique *tech = lightProbesEffect->GetTechniqueByName("irradiance_map");
		// For each face, 
		SIMUL_COMBINED_PROFILE_START(deviceContext, "RenderEnvmap draw")
		int cube_start=0,cube_end=6;
	if(cubemapIndex>=0&&cubemapIndex<6)
	{
		cube_start=cubemapIndex;
		cube_end=cube_start+1;
	}
	for(int i=cube_start;i<cube_end;i++)
	{
		target_texture->activateRenderTarget(deviceContext, { target_texture->GetShaderResourceTypeForRTVAndDSV(), {0, 1, i, 1}});
		//math::Matrix4x4 cube_proj = simul::crossplatform::Camera::MakeDepthReversedProjectionMatrix(SIMUL_PI_F / 2.f, SIMUL_PI_F / 2.f, 0.2f, 200000.f);
		{
			//static bool rev = true;
			//simul::crossplatform::GetCubeMatrixAtPosition((float *)&view, i, cam_pos, false, rev);
			//crossplatform::MakeInvViewProjMatrix(invViewProj, view, cube_proj);
			simul::crossplatform::GetCubeInvViewProjMatrix(invViewProj,i,false,true);
			lightProbeConstants.invViewProj = invViewProj;
			lightProbeConstants.numSHBands = bands;
			
			lightProbeConstants.alpha = 1.0f-blend;

			lightProbesEffect->SetConstantBuffer(deviceContext, &lightProbeConstants);
			GetSphericalHarmonics().Apply(deviceContext, lightProbesEffect, _basisBuffer);
			lightProbesEffect->Apply(deviceContext, tech, 0);
			if(blend>0.0f)
				renderPlatform->SetStandardRenderState(deviceContext, crossplatform::StandardRenderState::STANDARD_ALPHA_BLENDING);
			else
				renderPlatform->SetStandardRenderState(deviceContext, crossplatform::StandardRenderState::STANDARD_OPAQUE_BLENDING);
			renderPlatform->DrawQuad(deviceContext);
			lightProbesEffect->SetTexture(deviceContext, _basisBuffer, NULL);
			lightProbesEffect->Unapply(deviceContext);
		}
		target_texture->deactivateRenderTarget(deviceContext);
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext)
}