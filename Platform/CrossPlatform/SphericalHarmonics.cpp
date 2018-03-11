#include "SphericalHarmonics.h"
#include "Macros.h"
#include "GpuProfiler.h"
#include "RenderPlatform.h"
using namespace simul;
using namespace crossplatform;


SphericalHarmonics::SphericalHarmonics()
	:bands(4)
,sphericalHarmonicsEffect(nullptr)
,shSeed(0)
,lightProbesEffect(nullptr)
, renderPlatform(nullptr)
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
	sphericalSamples.InvalidateDeviceObjects();
	lightProbeConstants.RestoreDeviceObjects(renderPlatform);
	sphericalHarmonicsConstants.RestoreDeviceObjects(r);
	sphericalSamples.InvalidateDeviceObjects();
	static int seed = 0;
	seed = seed % 1001;
	shSeed=seed++;
	probeResultsRW.InvalidateDeviceObjects();
	probeResultsRW.RestoreDeviceObjects(renderPlatform,256,true);
}

void SphericalHarmonics::RecompileShaders()
{
	SAFE_DELETE(sphericalHarmonicsEffect);
	if (!renderPlatform)
		return;
	lightProbesEffect = renderPlatform->CreateEffect("light_probes");
	sphericalHarmonicsEffect = renderPlatform->CreateEffect("spherical_harmonics");
	sphericalHarmonicsConstants.LinkToEffect(sphericalHarmonicsEffect, "SphericalHarmonicsConstants");
	//lightProbeConstants.LinkToEffect(lightProbesEffect, "LightProbeConstants");
}

void SphericalHarmonics::InvalidateDeviceObjects()
{
	sphericalHarmonicsConstants.InvalidateDeviceObjects();
	sphericalHarmonics.InvalidateDeviceObjects();
	SAFE_DELETE(sphericalHarmonicsEffect);
	probeResultsRW.InvalidateDeviceObjects();
	SAFE_DELETE(lightProbesEffect);
}

float RoughnessFromMip(float mip, float numMips)
{
	static float  roughness_mip_scale = 1.2f;
	return exp2(( 3.0f+mip-numMips) / roughness_mip_scale);
}

void SphericalHarmonics::RenderMipsByRoughness(crossplatform::DeviceContext &deviceContext, crossplatform::Texture *target)
{
	if (!lightProbesEffect)
		RecompileShaders();
	lightProbesEffect->SetConstantBuffer(deviceContext, &lightProbeConstants);
	crossplatform::EffectTechnique *tech = lightProbesEffect->GetTechniqueByName("mip_from_roughness");
	lightProbeConstants.numMips = target->mips;
	for (int m = 0; m < target->mips - 1; m++)
	{
		lightProbeConstants.mipIndex = m+1;
		lightProbeConstants.roughness = RoughnessFromMip(float(lightProbeConstants.mipIndex), float(target->mips) );
		const char *passname = (lightProbeConstants.roughness < 0.01f) ? "smooth" : (lightProbeConstants.roughness < 0.99f ? "general" : "rough");
		for (int j = 0; j < 6; j++)
		{
			lightProbeConstants.cubeFace = j; 
			lightProbesEffect->SetConstantBuffer(deviceContext, &lightProbeConstants);
			// The source is the i'th mip of the faceIndex face of the cubemap texture.
			lightProbesEffect->SetTexture(deviceContext, "sourceCubemap", target, -1, m);
			// The target is the (i+1)'th mip of the faceIndex face.
			target->activateRenderTarget(deviceContext, j, m+1);
			lightProbesEffect->Apply(deviceContext, tech,passname);
			renderPlatform->DrawQuad(deviceContext);
			lightProbesEffect->UnbindTextures(deviceContext);
			lightProbesEffect->Unapply(deviceContext);
			target->deactivateRenderTarget(deviceContext);
		}
	}
}

void SphericalHarmonics::CopyMip(crossplatform::DeviceContext &deviceContext,Texture *tex,int face,int src_mip,float blend)
{
	if (!lightProbesEffect)
	{
		RecompileShaders();
	}
	lightProbeConstants.alpha = 1.0f-blend;
	lightProbeConstants.numMips = tex->mips;
	lightProbeConstants.cubeFace = face;
	lightProbeConstants.mipIndex = src_mip + 1;
	lightProbesEffect->UnbindTextures(deviceContext);
	lightProbeConstants.roughness = RoughnessFromMip(lightProbeConstants.mipIndex, tex->mips);
	lightProbesEffect->SetConstantBuffer(deviceContext, &lightProbeConstants);

	const bool doOldMipCopy = false;
	if(doOldMipCopy /*|| renderPlatform->GetName() == "DirectX 12" || renderPlatform->GetName() == "PS4"*/)
	{
	crossplatform::EffectTechnique *tech = lightProbesEffect->GetTechniqueByName((blend>0.0f) ? "blend_mip" : "overwrite_mip");
	// The source is the i'th mip of the faceIndex face of the cubemap texture.
	lightProbesEffect->SetTexture(deviceContext, "sourceTextureArray", tex, face, src_mip);
	// The target is the (i+1)'th mip of the faceIndex face.
	tex->activateRenderTarget(deviceContext, face, src_mip + 1);
	lightProbesEffect->Apply(deviceContext, tech, 0);
	renderPlatform->DrawQuad(deviceContext);
	lightProbesEffect->UnbindTextures(deviceContext);
	lightProbesEffect->Unapply(deviceContext);
	tex->deactivateRenderTarget(deviceContext);
}
	else
	{
		crossplatform::EffectTechniqueGroup *group	= lightProbesEffect->GetTechniqueGroupByName("mip_from_roughness");
		crossplatform::EffectTechnique *tech		= nullptr;
		if (blend > 0.0f)
		{
			tech = group->GetTechniqueByName("mip_from_roughness_blend");
		}
		else
		{
			tech = group->GetTechniqueByName("mip_from_roughness_no_blend");
		}
		const char *passname = (lightProbeConstants.roughness < 0.01f) ? "smooth" : (lightProbeConstants.roughness < 0.99f ? "general" : "rough");
		// The source is the i'th mip of the faceIndex face of the cubemap texture.
		lightProbesEffect->SetTexture(deviceContext, "sourceCubemap", tex, -1, src_mip);
		// The target is the (i+1)'th mip of the faceIndex face.
		tex->activateRenderTarget(deviceContext, face, src_mip + 1);
		lightProbesEffect->Apply(deviceContext, tech, passname);
		renderPlatform->DrawQuad(deviceContext);
		lightProbesEffect->UnbindTextures(deviceContext);
		lightProbesEffect->Unapply(deviceContext);
		tex->deactivateRenderTarget(deviceContext);
	}
}

bool SphericalHarmonics::Probe(crossplatform::DeviceContext &deviceContext
	,Texture *buffer_texture
	,int mip_size
	,int face_index
	,uint2 pos
	,uint2 size
	,vec4 *targetValuesFloat4)
{
	crossplatform::Effect *sphericalHarmonicsEffect = renderPlatform->GetEffect("spherical_harmonics");
	crossplatform::EffectTechnique *tech=sphericalHarmonicsEffect->GetTechniqueByName("probe_query");

	sphericalHarmonicsEffect->SetTexture(deviceContext,"cubemapAsTexture2DArray",buffer_texture);
	if(size.x*size.y>(unsigned)probeResultsRW.count)
	{
		probeResultsRW.RestoreDeviceObjects(renderPlatform,size.x*size.y*2,true);
	}
	probeResultsRW.ApplyAsUnorderedAccessView(deviceContext, sphericalHarmonicsEffect, "targetBuffer");

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
	static int sqrt_jitter_samples					=4;
	if(!sphericalHarmonics.count)
	{
		sphericalHarmonics.RestoreDeviceObjects(renderPlatform,num_coefficients,true);
		sphericalSamples.RestoreDeviceObjects(renderPlatform,sqrt_jitter_samples*sqrt_jitter_samples,true);
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
		crossplatform::EffectTechnique *jitter=sphericalHarmonicsEffect->GetTechniqueByName("jitter");
		sphericalSamples.ApplyAsUnorderedAccessView(deviceContext, sphericalHarmonicsEffect, "samplesBufferRW");
		sphericalHarmonicsEffect->SetTexture(deviceContext,"cubemapTexture"	,buffer_texture);
		sphericalHarmonicsEffect->SetConstantBuffer(deviceContext,&	sphericalHarmonicsConstants);
		sphericalHarmonicsEffect->Apply(deviceContext,jitter,0);
		int u = (sphericalHarmonicsConstants.numJitterSamples + BLOCK_SIZE - 1) / BLOCK_SIZE;
		renderPlatform->DispatchCompute(deviceContext, u, 1, 1);
		sphericalHarmonicsEffect->UnbindTextures(deviceContext);
		sphericalHarmonicsEffect->SetUnorderedAccessView(deviceContext,"samplesBufferRW",NULL);
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
		crossplatform::EffectTechnique *tech	=sphericalHarmonicsEffect->GetTechniqueByName("encode");
	sphericalHarmonicsEffect->SetTexture(deviceContext,"cubemapTexture"	,buffer_texture);
	sphericalSamples.Apply(deviceContext, sphericalHarmonicsEffect, "samplesBuffer");
	sphericalHarmonics.ApplyAsUnorderedAccessView(deviceContext, sphericalHarmonicsEffect, "targetBuffer");

	static bool sh_by_samples=false;
	sphericalHarmonicsEffect->SetConstantBuffer(deviceContext,&sphericalHarmonicsConstants);
	sphericalHarmonicsEffect->Apply(deviceContext,tech,0);
	int n = sh_by_samples ? sphericalHarmonicsConstants.numJitterSamples : num_coefficients;
	//static int ENCODE_BLOCK_SIZE=2;
	//int U = ((n) + ENCODE_BLOCK_SIZE - 1) / ENCODE_BLOCK_SIZE;
	renderPlatform->DispatchCompute(deviceContext, n, 1, 1);
	sphericalHarmonicsConstants.Unbind(deviceContext);
	sphericalHarmonicsEffect->Unapply(deviceContext);
	sphericalHarmonics.CopyToReadBuffer(deviceContext);
	sphericalHarmonicsEffect->UnbindTextures(deviceContext);
	SIMUL_COMBINED_PROFILE_END(deviceContext)
	SIMUL_COMBINED_PROFILE_END(deviceContext)
}


void SphericalHarmonics::RenderEnvmap(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *target_texture,int cubemapIndex,float blend)
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
	mat4 view;
	float cam_pos[] = { 0,0,0 };
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
		target_texture->activateRenderTarget(deviceContext,i,0);
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
			GetSphericalHarmonics().Apply(deviceContext, lightProbesEffect, "basisBuffer");
			lightProbesEffect->Apply(deviceContext, tech, 0);
			if(blend>0.0f)
				renderPlatform->SetStandardRenderState(deviceContext, crossplatform::StandardRenderState::STANDARD_ALPHA_BLENDING);
			else
				renderPlatform->SetStandardRenderState(deviceContext, crossplatform::StandardRenderState::STANDARD_OPAQUE_BLENDING);
			renderPlatform->DrawQuad(deviceContext);
			lightProbesEffect->SetTexture(deviceContext, "basisBuffer", NULL);
			lightProbesEffect->Unapply(deviceContext);
		}
		target_texture->deactivateRenderTarget(deviceContext);
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext)
}