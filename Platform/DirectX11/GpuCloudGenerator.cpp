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
			,densityTechnique(NULL)
			,densityComputeTechnique(NULL)
			,lightingTechnique(NULL)
			,lightingComputeTechnique(NULL)
			,secondaryLightingComputeTechnique(NULL)
			,transformTechnique(NULL)
			,transformComputeTechnique(NULL)
			,volume_noise_tex(NULL)
			,volume_noise_tex_srv(NULL)
			,m_pWwcSamplerState(NULL)
			,m_pCccSamplerState(NULL)
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
	fb[0].RestoreDeviceObjects(m_pd3dDevice);
	fb[1].RestoreDeviceObjects(m_pd3dDevice);
	dens_fb.SetDepthFormat(0);
	dens_fb.SetFormat(DXGI_FORMAT_R32_FLOAT);
	dens_fb.RestoreDeviceObjects(m_pd3dDevice);
	mask_fb.SetDepthFormat(0);
	mask_fb.RestoreDeviceObjects(m_pd3dDevice);
	world_fb.RestoreDeviceObjects(m_pd3dDevice);
	gpuCloudConstants.RestoreDeviceObjects(m_pd3dDevice);

	SAFE_RELEASE(m_pWwcSamplerState);
	SAFE_RELEASE(m_pCccSamplerState);
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
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	m_pd3dDevice->CreateSamplerState(&samplerDesc,&m_pCccSamplerState);

	RecompileShaders();
}

void GpuCloudGenerator::InvalidateDeviceObjects()
{
	fb[0].InvalidateDeviceObjects();
	fb[1].InvalidateDeviceObjects();
	dens_fb.InvalidateDeviceObjects();
	mask_fb.InvalidateDeviceObjects();
	world_fb.InvalidateDeviceObjects();
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
	SAFE_RELEASE(m_pCccSamplerState);
	m_pd3dDevice=NULL;
}

void GpuCloudGenerator::RecompileShaders()
{
	SAFE_RELEASE(effect);
	HRESULT hr=CreateEffect(m_pd3dDevice,&effect,"simul_gpu_clouds.fx");
	if(effect)
	{
		densityTechnique		=effect->GetTechniqueByName("simul_gpu_density");
		lightingTechnique		=effect->GetTechniqueByName("simul_gpu_lighting");
		lightingComputeTechnique=effect->GetTechniqueByName("gpu_lighting_compute");
		secondaryLightingComputeTechnique=effect->GetTechniqueByName("gpu_secondary_compute");
		transformTechnique		=effect->GetTechniqueByName("simul_gpu_transform");
		maskTechnique			=effect->GetTechniqueByName("density_mask");
		densityComputeTechnique	=effect->GetTechniqueByName("gpu_density_compute");
		transformComputeTechnique=effect->GetTechniqueByName("gpu_transform_compute");
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
	m_pImmediateContext->GenerateMips(volume_noise_tex_srv);
	return volume_noise_tex_srv;
}

void GpuCloudGenerator::FillDensityGrid(int cycled_index
										,const int *density_grid
										,int start_texel
										,int texels
										,float humidity
										,float baseLayer
										,float transition
										,float upperDensity
										,float diffusivity
										,float time
										,void* noise_tex
										,int octaves
										,float persistence
										,bool mask)
{
simul::base::Timer timer;
timer.StartTime();
std::cout<<"Gpu clouds: FillDensityGrid\n";
	for(int i=0;i<3;i++)
		finalTexture[i]->ensureTexture3DSizeAndFormat(m_pd3dDevice,density_grid[0],density_grid[1],density_grid[2],DXGI_FORMAT_R8G8B8A8_UNORM,true);
	int density_gridsize=density_grid[0]*density_grid[1]*density_grid[2];
	dens_fb.SetWidthAndHeight(density_grid[0],density_grid[1]*density_grid[2]);
	mask_fb.SetWidthAndHeight(density_grid[0],density_grid[1]);

	mask_fb.Activate(m_pImmediateContext);
	if(mask)
	{
		mask_fb.Clear(m_pImmediateContext,1.f,1.f,1.f,1.f,1.f);
		ApplyPass(m_pImmediateContext,maskTechnique->GetPassByIndex(0));
		dens_fb.DrawQuad(m_pImmediateContext);
	}
	else
	{
		mask_fb.Clear(m_pImmediateContext,1.f,1.f,1.f,1.f,1.f);
	}
	mask_fb.Deactivate(m_pImmediateContext);

	simul::math::Vector3 noise_scale(1.f,1.f,(float)density_grid[2]/(float)density_grid[0]);

	int z0	=start_texel/(density_grid[0]*density_grid[1]);
	int z1	=(start_texel+texels)/(density_grid[0]*density_grid[1]);
	int zmax=density_grid[2];
	float y_start					=(float)z0/(float)zmax;
	float y_range					=(float)z1/(float)zmax-y_start;
	gpuCloudConstants.yRange		=vec4(y_start,y_range,0,0);

	gpuCloudConstants.octaves		=octaves;
	gpuCloudConstants.persistence	=persistence;
	gpuCloudConstants.humidity		=humidity;
	gpuCloudConstants.time			=time;
	gpuCloudConstants.noiseScale	=noise_scale;

	gpuCloudConstants.zPixel		=1.f/(float)density_grid[2];
	gpuCloudConstants.zSize			=(float)density_grid[2];
	gpuCloudConstants.baseLayer		=baseLayer;
	gpuCloudConstants.transition	=transition;
	gpuCloudConstants.upperDensity	=upperDensity;
	gpuCloudConstants.diffusivity	=diffusivity;

	float fractalSum=0.0;
	float mult=0.5;
	for(int i=0;i<octaves;i++)
	{
		fractalSum+=mult;
		mult*=persistence;
	}
	gpuCloudConstants.invFractalSum=1.f/fractalSum;

//gpuCloudConstants.densityGrid	=uint3(density_grid);
	simul::dx11::setParameter(effect,"volumeNoiseTexture"	,volume_noise_tex_srv);
	simul::dx11::setParameter(effect,"maskTexture"			,(ID3D11ShaderResourceView*)mask_fb.GetColorTex());

	//DXGI_FORMAT_R32G32B32A32_FLOAT
	density_texture.ensureTexture3DSizeAndFormat(m_pd3dDevice
		,density_grid[0],density_grid[1],density_grid[2]
		,DXGI_FORMAT_R32_FLOAT,true);
//	Ensure3DTextureSizeAndFormat(m_pd3dDevice,density_texture,density_texture_srv,density_grid[0],density_grid[1],density_grid[2],DXGI_FORMAT_R32G32B32A32_FLOAT);
std::cout<<"\tmake 3DTexture "<<timer.UpdateTime()<<"ms"<<std::endl;

	simul::dx11::setParameter(effect,"targetTexture",density_texture.unorderedAccessView);
	HRESULT hr;
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
	ApplyPass(m_pImmediateContext,densityComputeTechnique->GetPassByIndex(0));
	if(x1>x0)
		m_pImmediateContext->Dispatch(x1-x0,subgrid.y,subgrid.z);
std::cout<<"\tfill 3DTexture "<<timer.UpdateTime()<<"ms"<<std::endl;
	simul::dx11::setParameter(effect,"targetTexture",(ID3D11UnorderedAccessView*)NULL);
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
simul::base::Timer timer;
timer.StartTime();
std::cout<<"Gpu clouds: PerformGPURelight\n";
int light_grid[]={light_grid_[0],light_grid_[1],light_grid_[2]};//*2};
	start_texel*=2;
	texels*=2;
	int gridsize=light_grid[0]*light_grid[1]*light_grid[2];
	if(start_texel<0)
		start_texel=0;
	if(start_texel>gridsize)
		start_texel=gridsize;	
	if(start_texel+texels>gridsize)
		texels=gridsize-start_texel; 
	for(int i=0;i<2;i++)
	{
		fb[i].SetWidthAndHeight(light_grid[0],light_grid[1]);
	}
	directLightTextures[light_index].ensureTexture3DSizeAndFormat(m_pd3dDevice,light_grid[0],light_grid[1],light_grid[2]
				,DXGI_FORMAT_R32_FLOAT,true);
	indirectLightTextures[light_index].ensureTexture3DSizeAndFormat(m_pd3dDevice,light_grid[0],light_grid[1],light_grid[2]
				,DXGI_FORMAT_R32_FLOAT,true);

	ID3D1xEffectShaderResourceVariable*	input_light_texture	=effect->GetVariableByName("inputTexture")->AsShaderResource();
	ID3D1xEffectShaderResourceVariable*	densityTexture		=effect->GetVariableByName("densityTexture")->AsShaderResource();

	gpuCloudConstants.yRange			=vec4(0.0,1.0,0,0);
	gpuCloudConstants.transformMatrix	=Matrix4x4LightToDensityTexcoords;
	gpuCloudConstants.transformMatrix.transpose();
	gpuCloudConstants.extinctions		=lightspace_extinctions_float3;

	//transformMatrix * (0,0,1)
	simul::sky::float4 step	(gpuCloudConstants.transformMatrix._13,gpuCloudConstants.transformMatrix._23,gpuCloudConstants.transformMatrix._33,0);
		//	Vector3 step=fabs(scales(axis))*light_vec/(float)grid[2];
	gpuCloudConstants.stepLength		=simul::sky::length(step); 
	if(wrap_light_tex)
		simul::dx11::setSamplerState(effect,"lightSamplerState",m_pWwcSamplerState);
	else
		simul::dx11::setSamplerState(effect,"lightSamplerState",m_pCccSamplerState);
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
		simul::dx11::setParameter(effect,"targetTexture1",directLightTextures[light_index].unorderedAccessView);
		ApplyPass(m_pImmediateContext,lightingComputeTechnique->GetPassByIndex(0));
		m_pImmediateContext->Dispatch(x1-x0,subgrid.y,1);
	}
	int z0	=start_texel/light_grid[1]/light_grid[0];
	int z1	=(start_texel+texels+light_grid[1]*light_grid[0]-1)/light_grid[1]/light_grid[0];
	if(z1>z0)
	{
		gpuCloudConstants.threadOffset=uint3(0,0,z0);
		gpuCloudConstants.Apply(m_pImmediateContext);
		simul::dx11::setParameter(effect,"targetTexture1",indirectLightTextures[light_index].unorderedAccessView);
		ApplyPass(m_pImmediateContext,secondaryLightingComputeTechnique->GetPassByIndex(0));
		m_pImmediateContext->Dispatch(subgrid.x,subgrid.y,z1-z0);
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
simul::base::Timer timer;
timer.StartTime();
std::cout<<"Gpu clouds: GPUTransferDataToTexture\n";
	// For each level in the z direction, we render out a 2D texture and copy it to the target.
	world_fb.SetWidthAndHeight(density_grid[0],density_grid[1]*density_grid[2]);
	world_fb.SetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
std::cout<<"\tInit "<<timer.UpdateTime()<<"ms"<<std::endl;

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

	setParameter(effect,"densityTexture",density_texture.shaderResourceView);
	setParameter(effect,"ambientTexture1",directLightTextures[0].shaderResourceView);
	setParameter(effect,"ambientTexture2",indirectLightTextures[0].shaderResourceView);
	setParameter(effect,"lightTexture1"	,directLightTextures[1].shaderResourceView);
	setParameter(effect,"lightTexture2"	,indirectLightTextures[1].shaderResourceView);
	// Instead of a loop, we do a single big render, by tiling the z layers in the y direction.
	gpuCloudConstants.Apply(m_pImmediateContext);
#if 1
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
	simul::dx11::setParameter(effect,"targetTexture",finalTexture[cycled_index]->unorderedAccessView);
	ApplyPass(m_pImmediateContext,transformComputeTechnique->GetPassByIndex(0));
	if(x1>x0)
		m_pImmediateContext->Dispatch(x1-x0,subgrid.y,subgrid.z);
#endif
}