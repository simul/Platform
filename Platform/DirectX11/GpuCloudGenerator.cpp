#include "GpuCloudGenerator.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Base/Timer.h"
#include "CreateEffectDX1x.h"
#include <math.h>

using namespace simul;
using namespace dx11;

GpuCloudGenerator::GpuCloudGenerator()
			:m_pd3dDevice(NULL)
			,m_pImmediateContext(NULL)
			,effect(NULL)
			,densityComputeTechnique(NULL)
			,lightingComputeTechnique(NULL)
			,secondaryLightingComputeTechnique(NULL)
			,transformComputeTechnique(NULL)
			,volume_noise_tex(NULL)
			,volume_noise_tex_srv(NULL)
			,m_pWwcSamplerState(NULL)
			,m_pCwcSamplerState(NULL)
			,m_pWccSamplerState(NULL)
{
	for(int i=0;i<3;i++)
		finalTexture[i]=NULL;
}

GpuCloudGenerator::~GpuCloudGenerator()
{
	InvalidateDeviceObjects();
}

void GpuCloudGenerator::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(ID3D1xDevice*)dev;
	SAFE_RELEASE(m_pImmediateContext);
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
	// Mask must have depth as that's how it merges.
	mask_fb.SetDepthFormat(DXGI_FORMAT_R32_FLOAT);
	mask_fb.RestoreDeviceObjects(m_pd3dDevice);
	gpuCloudConstants.RestoreDeviceObjects(m_pd3dDevice);

	SAFE_RELEASE(m_pWwcSamplerState);
	SAFE_RELEASE(m_pCwcSamplerState);
	SAFE_RELEASE(m_pWccSamplerState);
	D3D11_SAMPLER_DESC samplerDesc;
	
    ZeroMemory( &samplerDesc, sizeof( D3D11_SAMPLER_DESC ) );
    samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC   ;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MaxAnisotropy = 16;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	
	m_pd3dDevice->CreateSamplerState(&samplerDesc,&m_pWwcSamplerState);
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	m_pd3dDevice->CreateSamplerState(&samplerDesc,&m_pCwcSamplerState);
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	m_pd3dDevice->CreateSamplerState(&samplerDesc,&m_pWccSamplerState);

	RecompileShaders();
}

void GpuCloudGenerator::InvalidateDeviceObjects()
{
	mask_fb.InvalidateDeviceObjects();
	SAFE_RELEASE(volume_noise_tex);
	SAFE_RELEASE(volume_noise_tex_srv);
	SAFE_RELEASE(m_pImmediateContext);
	SAFE_RELEASE(effect);
	density_texture.release();
	gpuCloudConstants.InvalidateDeviceObjects();
	for(int i=0;i<2;i++)
	{
		directLightTextures[i].release();
		indirectLightTextures[i].release();
	}
	SAFE_RELEASE(m_pWwcSamplerState);
	SAFE_RELEASE(m_pCwcSamplerState);
	SAFE_RELEASE(m_pWccSamplerState);
	m_pd3dDevice=NULL;
}

void GpuCloudGenerator::RecompileShaders()
{
	SAFE_RELEASE(effect);
	HRESULT hr=CreateEffect(m_pd3dDevice,&effect,"simul_gpu_clouds.fx");
	if(effect)
	{
		lightingComputeTechnique			=effect->GetTechniqueByName("gpu_lighting_compute");
		secondaryLightingComputeTechnique	=effect->GetTechniqueByName("gpu_secondary_compute");
		maskTechnique						=effect->GetTechniqueByName("density_mask");
		densityComputeTechnique				=effect->GetTechniqueByName("gpu_density_compute");
		transformComputeTechnique			=effect->GetTechniqueByName("gpu_transform_compute");
	}
	gpuCloudConstants.LinkToEffect(effect,"GpuCloudConstants");
}

int GpuCloudGenerator::GetDensityGridsize(const int *grid)
{
	//if(iformat==DXGI_FORMAT_R32G32B32A32_FLOAT)
	//	size=4;
	return grid[0]*grid[1]*grid[2];
}

void* GpuCloudGenerator::Make3DNoiseTexture(int noise_size,const float *noise_src_ptr)
{
	//using noise_size and noise_src_ptr, make a 3d texture:
	SAFE_RELEASE(volume_noise_tex);
	SAFE_RELEASE(volume_noise_tex_srv);
	volume_noise_tex=make3DTexture(m_pd3dDevice,noise_size,noise_size,noise_size,DXGI_FORMAT_R32_FLOAT,noise_src_ptr);
	m_pd3dDevice->CreateShaderResourceView(volume_noise_tex,NULL,&volume_noise_tex_srv);
	//m_pImmediateContext->GenerateMips(volume_noise_tex_srv);
	return volume_noise_tex_srv;
}

void GpuCloudGenerator::FillDensityGrid(int index
										,const clouds::GpuCloudsParameters &params
										,int start_texel
										,int texels
										,const simul::clouds::MaskMap &masks)
{
	for(int i=0;i<3;i++)
		finalTexture[i]->ensureTexture3DSizeAndFormat(m_pd3dDevice,params.density_grid[0],params.density_grid[1],params.density_grid[2],DXGI_FORMAT_R8G8B8A8_UNORM,true);
	int density_gridsize=params.density_grid[0]*params.density_grid[1]*params.density_grid[2];
	mask_fb.SetWidthAndHeight(params.density_grid[0],params.density_grid[1]);

	mask_fb.Activate(m_pImmediateContext);
	if(masks.size())
	{
		mask_fb.Clear(m_pImmediateContext,0.f,0.f,0.f,0.f,0.f);
		
		for(simul::clouds::MaskMap::const_iterator i=masks.begin();i!=masks.end();i++)
		{
	gpuCloudConstants.yRange		=vec4(0,1.f,0,0);
			gpuCloudConstants.maskCentre	=vec2(i->second.x,i->second.y);
			gpuCloudConstants.maskRadius	=i->second.radius;
			gpuCloudConstants.maskFeather	=0.1f;
			gpuCloudConstants.maskThickness	=i->second.thickness;
			gpuCloudConstants.Apply(m_pImmediateContext);
		ApplyPass(m_pImmediateContext,maskTechnique->GetPassByIndex(0));
			mask_fb.DrawQuad(m_pImmediateContext);
	}
	}
	else
	{
		mask_fb.Clear(m_pImmediateContext,1.f,1.f,1.f,1.f,1.f);
	}
	mask_fb.Deactivate(m_pImmediateContext);

	int z0	=start_texel/(params.density_grid[0]*params.density_grid[1]);
	int z1	=(start_texel+texels)/(params.density_grid[0]*params.density_grid[1]);
	int zmax=params.density_grid[2];
	float y_start					=(float)z0/(float)zmax;
	float y_range					=(float)z1/(float)zmax-y_start;
	SetGpuCloudConstants(gpuCloudConstants,params,y_start,y_range);

	simul::dx11::setParameter(effect,"volumeNoiseTexture"	,volume_noise_tex_srv);
	simul::dx11::setParameter(effect,"maskTexture"			,(ID3D11ShaderResourceView*)mask_fb.GetColorTex());
	
	density_texture.ensureTexture3DSizeAndFormat(m_pd3dDevice
		,params.density_grid[0],params.density_grid[1],params.density_grid[2]
		,DXGI_FORMAT_R32_FLOAT,true);

	simul::dx11::setUnorderedAccessView(effect,"targetTexture",density_texture.unorderedAccessView);

	// divide the grid into 8x8x8 blocks:
	static const int BLOCKWIDTH=8;
	static const int BLOCKSIZE=BLOCKWIDTH*BLOCKWIDTH*BLOCKWIDTH;
	uint3 subgrid;
	subgrid.x=(params.density_grid[0]+BLOCKWIDTH-1)/BLOCKWIDTH;
	subgrid.y=(params.density_grid[1]+BLOCKWIDTH-1)/BLOCKWIDTH;
	subgrid.z=(params.density_grid[2]+BLOCKWIDTH-1)/BLOCKWIDTH;
	int subgridsize=subgrid.x*subgrid.y*subgrid.z;
	// which blocks to execute?
	int x0	=start_texel/BLOCKSIZE/subgrid.y/subgrid.z;
	int x1	=(((start_texel+texels+BLOCKSIZE-1)/(BLOCKSIZE)+subgrid.y-1)/subgrid.y+subgrid.z-1)/subgrid.z;
	gpuCloudConstants.threadOffset=uint3(x0*BLOCKWIDTH,0,0);
	gpuCloudConstants.Apply(m_pImmediateContext);
	ApplyPass(m_pImmediateContext,densityComputeTechnique->GetPassByIndex(0));
	if(x1>x0)
		m_pImmediateContext->Dispatch(x1-x0,subgrid.y,subgrid.z);
	simul::dx11::setParameter			(effect,"volumeNoiseTexture",(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setParameter			(effect,"maskTexture"		,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setUnorderedAccessView	(effect,"targetTexture"		,(ID3D11UnorderedAccessView*)NULL);
	ApplyPass(m_pImmediateContext,densityComputeTechnique->GetPassByIndex(0));
}

void GpuCloudGenerator::PerformGPURelight	(int light_index
											,float *target
											,const int *light_grid_
											,int start_texel
											,int texels
											,const int *density_grid
											,const float *Matrix4x4LightToDensityTexcoords
											,const float *lightspace_extinctions_float3
											,bool wrap_light_tex)
{
	int light_grid[]={light_grid_[0],light_grid_[1],light_grid_[2]};//};
	start_texel*=2;
	texels*=2;
	int gridsize=light_grid[0]*light_grid[1]*light_grid[2];
	if(start_texel<0)
		start_texel=0;
	if(start_texel>gridsize)
		start_texel=gridsize;	
	if(start_texel+texels>gridsize)
		texels=gridsize-start_texel;
	directLightTextures[light_index].ensureTexture3DSizeAndFormat(m_pd3dDevice
				,light_grid[0],light_grid[1],light_grid[2]
				,DXGI_FORMAT_R32_FLOAT,true);
	indirectLightTextures[light_index].ensureTexture3DSizeAndFormat(m_pd3dDevice
				,light_grid[0],light_grid[1],light_grid[2]
				,DXGI_FORMAT_R32_FLOAT,true);

	ID3D1xEffectShaderResourceVariable*	densityTexture		=effect->GetVariableByName("densityTexture")->AsShaderResource();

	gpuCloudConstants.yRange			=vec4(0.0,1.0,0,0);
	gpuCloudConstants.transformMatrix	=Matrix4x4LightToDensityTexcoords;
	gpuCloudConstants.transformMatrix.transpose();
	gpuCloudConstants.extinctions		=lightspace_extinctions_float3;
	gpuCloudConstants.zPixelLightspace	=(1.f/(float)light_grid[2]);

	//transformMatrix * (0,0,1)
	simul::sky::float4 step	(gpuCloudConstants.transformMatrix._13,gpuCloudConstants.transformMatrix._23,gpuCloudConstants.transformMatrix._33,0);

	gpuCloudConstants.stepLength		=simul::sky::length(step); 
	if(wrap_light_tex)
		simul::dx11::setSamplerState(effect,"lightSamplerState",m_pWwcSamplerState);
	else if(light_grid[0]>light_grid[1])
		simul::dx11::setSamplerState(effect,"lightSamplerState",m_pWccSamplerState);
	else
		simul::dx11::setSamplerState(effect,"lightSamplerState",m_pCwcSamplerState);
	densityTexture->SetResource(density_texture.shaderResourceView);

	// divide the grid into 8x8x8 blocks:
	static const int BLOCKWIDTH=8;
	static const int BLOCKSIZE=BLOCKWIDTH*BLOCKWIDTH;
	uint3 subgrid;
	subgrid.x=(light_grid[0]+BLOCKWIDTH-1)/BLOCKWIDTH;
	subgrid.y=(light_grid[1]+BLOCKWIDTH-1)/BLOCKWIDTH;
	subgrid.z=1;
	int subgridsize=subgrid.x*subgrid.y*subgrid.z;

	// discard z dimension:
	int t0	=(start_texel)/light_grid[2];
	int t1	=(start_texel+texels+light_grid[2]-1)/light_grid[2];
	int t	=t1-t0;
	// which blocks to execute?
	int x0	=t0/BLOCKSIZE/subgrid.y;
	int x1	=((t0+t+BLOCKSIZE-1)/BLOCKSIZE+subgrid.y-1)/subgrid.y;

	gpuCloudConstants.threadOffset=uint3(x0*BLOCKWIDTH,0,0);
	gpuCloudConstants.Apply(m_pImmediateContext);
	if(x1>x0)
	{
		simul::dx11::setUnorderedAccessView(effect,"targetTexture1",directLightTextures[light_index].unorderedAccessView);
		ApplyPass(m_pImmediateContext,lightingComputeTechnique->GetPassByIndex(0));
		m_pImmediateContext->Dispatch(x1-x0,subgrid.y,1);
	}
	int z0	=start_texel/light_grid[1]/light_grid[0];
	int z1	=(start_texel+texels+light_grid[1]*light_grid[0]-1)/light_grid[1]/light_grid[0];
	if(z1>z0)
	{
		for(int z=z0;z<z1;z++)
		{
			gpuCloudConstants.threadOffset=uint3(0,0,z);
			gpuCloudConstants.Apply(m_pImmediateContext);
			setParameter(effect,"lightTexture1"				,directLightTextures[light_index].shaderResourceView);
			simul::dx11::setUnorderedAccessView(effect,"targetTexture1",indirectLightTextures[light_index].unorderedAccessView);
			ApplyPass(m_pImmediateContext,secondaryLightingComputeTechnique->GetPassByIndex(0));
			m_pImmediateContext->Dispatch(subgrid.x,subgrid.y,1);
		}
	}
	simul::dx11::setUnorderedAccessView(effect,"targetTexture1",(ID3D11UnorderedAccessView*)NULL);
	densityTexture->SetResource(NULL);
	// We have to do THIS, AFTER NULLing the textures. For god's sake.
	ApplyPass(m_pImmediateContext,secondaryLightingComputeTechnique->GetPassByIndex(0));

	// copy to CPU memory if required by CloudKeyframer.
	if(target)
	{
		directLightTextures[light_index].copyToMemory(m_pd3dDevice,m_pImmediateContext,target,start_texel,texels);
		target+=gridsize;
		indirectLightTextures[light_index].copyToMemory(m_pd3dDevice,m_pImmediateContext,target,start_texel,texels);
	}
}

void GpuCloudGenerator::GPUTransferDataToTexture(int cycled_index,unsigned char *target
												,const float *DensityToLightTransform
												,const float *light,const int *light_grid
												,const float *ambient,const int *density_grid
												,int start_texel
												,int texels
												,bool wrap_light_tex)
{

	int density_gridsize				=density_grid[0]*density_grid[1]*density_grid[2];

	int z0								=start_texel/(density_grid[0]*density_grid[1]);
	int z1								=(start_texel+texels)/(density_grid[0]*density_grid[1]);
	int zmax							=density_grid[2];

	float y_start						=(float)z0/(float)zmax;
	float y_range						=(float)z1/(float)zmax-y_start;
	gpuCloudConstants.yRange			=vec4(y_start,y_range,0,0);
	gpuCloudConstants.transformMatrix	=DensityToLightTransform;
	gpuCloudConstants.transformMatrix.transpose();

	gpuCloudConstants.zSize				=((float)density_grid[2]);
	gpuCloudConstants.zPixel			=(1.f/(float)density_grid[2]);
	gpuCloudConstants.zPixelLightspace	=(1.f/(float)light_grid[2]);

	setParameter(effect,"densityTexture"	,density_texture.shaderResourceView);
	setParameter(effect,"ambientTexture1"	,directLightTextures[0].shaderResourceView);
	setParameter(effect,"ambientTexture2"	,indirectLightTextures[0].shaderResourceView);
	setParameter(effect,"lightTexture1"		,directLightTextures[1].shaderResourceView);
	setParameter(effect,"lightTexture2"		,indirectLightTextures[1].shaderResourceView);
	// Instead of a loop, we do a single big render, by tiling the z layers in the y direction.
	gpuCloudConstants.Apply(m_pImmediateContext);
	for(int i=0;i<3;i++)
	{
		finalTexture[i]->ensureTexture3DSizeAndFormat(m_pd3dDevice,density_grid[0],density_grid[1],density_grid[2],DXGI_FORMAT_R8G8B8A8_UNORM,true);
	}
	// divide the grid into 8x8x8 blocks:
	static const int BLOCKWIDTH=8;
	static const int BLOCKSIZE=BLOCKWIDTH*BLOCKWIDTH*BLOCKWIDTH;
	uint3 subgrid;
	subgrid.x=(density_grid[0]+BLOCKWIDTH-1)/BLOCKWIDTH;
	subgrid.y=(density_grid[1]+BLOCKWIDTH-1)/BLOCKWIDTH;
	subgrid.z=(density_grid[2]+BLOCKWIDTH-1)/BLOCKWIDTH;
	int subgridsize=subgrid.x*subgrid.y*subgrid.z;
	// which blocks to execute?
	int x0	=start_texel/BLOCKSIZE/subgrid.y/subgrid.z;
	int x1	=(((start_texel+texels+BLOCKSIZE-1)/(BLOCKSIZE)+subgrid.y-1)/subgrid.y+subgrid.z-1)/subgrid.z;

	gpuCloudConstants.threadOffset=uint3(x0*BLOCKWIDTH,0,0);
	gpuCloudConstants.Apply(m_pImmediateContext);
	//simul::dx11::setParameter(effect,"targetTexture",density_texture.unorderedAccessView);
	simul::dx11::setUnorderedAccessView(effect,"targetTexture",finalTexture[cycled_index]->unorderedAccessView);
	ApplyPass(m_pImmediateContext,transformComputeTechnique->GetPassByIndex(0));
	if(x1>x0)
		m_pImmediateContext->Dispatch(x1-x0,subgrid.y,subgrid.z);
	simul::dx11::setUnorderedAccessView(effect,"targetTexture",(ID3D11UnorderedAccessView*)NULL);
	setParameter(effect,"densityTexture"	,(ID3D11ShaderResourceView*)NULL);
	setParameter(effect,"ambientTexture1"	,(ID3D11ShaderResourceView*)NULL);
	setParameter(effect,"ambientTexture2"	,(ID3D11ShaderResourceView*)NULL);
	setParameter(effect,"lightTexture1"		,(ID3D11ShaderResourceView*)NULL);
	setParameter(effect,"lightTexture2"		,(ID3D11ShaderResourceView*)NULL);
	ApplyPass(m_pImmediateContext,transformComputeTechnique->GetPassByIndex(0));
	// copy to CPU memory if required by CloudKeyframer.
	if(target)
	{
		finalTexture[cycled_index]->copyToMemory(m_pd3dDevice,m_pImmediateContext,target,start_texel,texels);
	}
}