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
			,lightingTechnique(NULL)
			,transformTechnique(NULL)
			,volume_noise_tex(NULL)
			,volume_noise_tex_srv(NULL)
			,density_texture(NULL)
			,density_texture_srv(NULL)
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
	SAFE_RELEASE(density_texture_srv);
	SAFE_RELEASE(density_texture);
	gpuCloudConstants.InvalidateDeviceObjects();
	for(int i=0;i<2;i++)
		lightTextures[i].release();
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
		densityTechnique	=effect->GetTechniqueByName("simul_gpu_density");
		lightingTechnique	=effect->GetTechniqueByName("simul_gpu_lighting");
		transformTechnique	=effect->GetTechniqueByName("simul_gpu_transform");
		maskTechnique		=effect->GetTechniqueByName("density_mask");
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

void GpuCloudGenerator::FillDensityGrid(int index
										,const int *density_grid
										,int start_texel
										,int texels
										,float humidity
										,float baseLayer
										,float transition
										,float upperDensity
										,float time
										,void* noise_tex
										,int octaves
										,float persistence
										,bool mask)
{
simul::base::Timer timer;
timer.StartTime();
std::cout<<"Gpu clouds: FillDensityGrid\n";
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

	float y_start					=(float)start_texel/(float)density_gridsize;
	float y_range					=(float)(texels)/(float)density_gridsize;
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

	simul::dx11::setParameter(effect,"volumeNoiseTexture"	,volume_noise_tex_srv);
	simul::dx11::setParameter(effect,"maskTexture"			,(ID3D11ShaderResourceView*)mask_fb.GetColorTex());

	gpuCloudConstants.Apply(m_pImmediateContext);


	dens_fb.Activate(m_pImmediateContext);
std::cout<<"\tInit "<<timer.UpdateTime()<<"ms"<<std::endl;
		ApplyPass(m_pImmediateContext,densityTechnique->GetPassByIndex(0));
		dens_fb.DrawQuad(m_pImmediateContext);
	dens_fb.Deactivate(m_pImmediateContext);
std::cout<<"\tDraw "<<timer.UpdateTime()<<"ms"<<std::endl;
	Ensure3DTextureSizeAndFormat(m_pd3dDevice,density_texture,density_texture_srv,density_grid[0],density_grid[1],density_grid[2],DXGI_FORMAT_R32G32B32A32_FLOAT);
std::cout<<"\tmake 3DTexture "<<timer.UpdateTime()<<"ms"<<std::endl;
	//if(start_texel+texels>=density_gridsize)
	{
		// Copy all the layers from the 2D dens_fb texture to the 3D density texture.
		D3D11_BOX sourceRegion;
		sourceRegion.left	=0;
		sourceRegion.right	=density_grid[0];
		sourceRegion.front	=0;
		sourceRegion.back	=1;

		int z0=start_texel/(density_grid[0]*density_grid[1]);
		int z1=(start_texel+texels)/(density_grid[0]*density_grid[1]);

		for(int Z=z0;Z<z1;Z++)
		{
			sourceRegion.top	=density_grid[1]*Z;
			sourceRegion.bottom	=density_grid[1]*(Z+1);
			m_pImmediateContext->CopySubresourceRegion(density_texture,0,0,0,Z,dens_fb.GetColorTexture(),0,&sourceRegion);
		}
	}
std::cout<<"\tfill 3DTexture "<<timer.UpdateTime()<<"ms"<<std::endl;
}

void GpuCloudGenerator::PerformGPURelight	(int light_index
											,float *target
											,const int *light_grid
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
		//fb[i].InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	}
	lightTextures[light_index].ensureTexture3DSizeAndFormat(m_pd3dDevice,light_grid[0],light_grid[1],light_grid[2],DXGI_FORMAT_R32G32B32A32_FLOAT);
	ID3D1xEffectShaderResourceVariable*	input_light_texture	=effect->GetVariableByName("inputTexture")->AsShaderResource();
	ID3D1xEffectShaderResourceVariable*	densityTexture		=effect->GetVariableByName("densityTexture")->AsShaderResource();

	gpuCloudConstants.yRange			=vec4(0.0,1.0,0,0);
	gpuCloudConstants.transformMatrix	=Matrix4x4LightToDensityTexcoords;
	gpuCloudConstants.transformMatrix.transpose();
	gpuCloudConstants.extinctions		=lightspace_extinctions_float3;
	// initialize the first input texture.
	simul::dx11::Framebuffer *F[2];
	F[0]=&fb[0];
	F[1]=&fb[1];
	densityTexture->SetResource(density_texture_srv);
	D3D11_BOX sourceRegion;
	sourceRegion.left	=0;
	sourceRegion.right	=light_grid[0];
	sourceRegion.top	=0;
	sourceRegion.bottom	=light_grid[1];
	sourceRegion.front	=0;
	sourceRegion.back	=1;
	if(wrap_light_tex)
		simul::dx11::setSamplerState(effect,"lightSamplerState",m_pWwcSamplerState);
	else
		simul::dx11::setSamplerState(effect,"lightSamplerState",m_pCccSamplerState);
std::cout<<"\tInit "<<timer.UpdateTime()<<"ms"<<std::endl;
	if(start_texel==0)
	{
		F[0]->Activate(m_pImmediateContext);
			input_light_texture->SetResource(F[1]->GetBufferResource());
			F[0]->Clear(m_pImmediateContext,1.f,1.f,1.f,1.f,1.f);
		F[0]->Deactivate(m_pImmediateContext);
std::cout<<"\tDraw0 "<<timer.UpdateTime()<<"ms"<<std::endl;
		if(target)
			F[0]->CopyToMemory(m_pImmediateContext,target);
std::cout<<"\tCopy0 "<<timer.UpdateTime()<<"ms"<<std::endl;
		m_pImmediateContext->CopySubresourceRegion(lightTextures[light_index].texture,0,0,0,0,F[0]->GetColorTexture(),0,&sourceRegion);
	}
	int i0	=start_texel/(light_grid[0]*light_grid[1]);
	int i1	=(start_texel+texels)/(light_grid[0]*light_grid[1]);
	if(target)
		target	+=i0*light_grid[0]*light_grid[1]*4;
	if(i0%2)
		std::swap(F[0],F[1]);
	float drawtime=0.f,copytime=0.f;
	for(int i=i0;i<i1;i++)
	{
		float zPos=((float)i+0.5f)/(float)light_grid[2];
		gpuCloudConstants.zPosition=(zPos);
		gpuCloudConstants.Apply(m_pImmediateContext);
		F[1]->Activate(m_pImmediateContext);
			input_light_texture->SetResource(F[0]->GetBufferResource());
			ApplyPass(m_pImmediateContext,lightingTechnique->GetPassByIndex(0));
			F[1]->DrawQuad(m_pImmediateContext);
		F[1]->Deactivate(m_pImmediateContext);
drawtime+=timer.UpdateTime();
		// Copy F[1] contents to the target
		if(target)
			F[1]->CopyToMemory(m_pImmediateContext,target);

		// Copy this layer to the volume light texture:
		m_pImmediateContext->CopySubresourceRegion(lightTextures[light_index].texture,0,0,0,i,F[1]->GetColorTexture(),0,&sourceRegion);

		std::swap(F[0],F[1]);
		if(target)
			target+=light_grid[0]*light_grid[1]*4;
copytime+=timer.UpdateTime();
	}

	std::cout<<"\tDraw "<<drawtime<<"ms"<<std::endl;
	std::cout<<"\tCopy "<<copytime<<"ms"<<std::endl;

	std::cout<<"\tTotal "<<timer.TimeSum<<"ms"<<std::endl;
}

void GpuCloudGenerator::GPUTransferDataToTexture(int index,unsigned char *target
												,const float *DensityToLightTransform
												,const float *light,const int *light_grid
												,const float *ambient,const int *density_grid
												,int start_texel
												,int texels)
{
simul::base::Timer timer;
timer.StartTime();
std::cout<<"Gpu clouds: GPUTransferDataToTexture\n";
	// For each level in the z direction, we render out a 2D texture and copy it to the target.
	world_fb.SetWidthAndHeight(density_grid[0],density_grid[1]*density_grid[2]);
	world_fb.SetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
std::cout<<"\tInit "<<timer.UpdateTime()<<"ms"<<std::endl;

	int density_gridsize				=density_grid[0]*density_grid[1]*density_grid[2];
	float y_start						=(float)start_texel/(float)density_gridsize;
	float y_range						=(float)(texels)/(float)density_gridsize;
	gpuCloudConstants.yRange			=vec4(y_start,y_range,0,0);
	gpuCloudConstants.transformMatrix	=DensityToLightTransform;
	gpuCloudConstants.transformMatrix.transpose();

	gpuCloudConstants.zSize=((float)density_grid[2]);
	gpuCloudConstants.zPixel=(1.f/(float)density_grid[2]);

	setParameter(effect,"densityTexture",density_texture_srv);
	setParameter(effect,"ambientTexture",lightTextures[0].shaderResourceView);
	setParameter(effect,"lightTexture",lightTextures[1].shaderResourceView);
	// Instead of a loop, we do a single big render, by tiling the z layers in the y direction.
	gpuCloudConstants.Apply(m_pImmediateContext);
	world_fb.Activate(m_pImmediateContext);
		ApplyPass(m_pImmediateContext,transformTechnique->GetPassByIndex(0));
		world_fb.DrawQuad(m_pImmediateContext);
	world_fb.Deactivate(m_pImmediateContext);
std::cout<<"\tDraw "<<timer.UpdateTime()<<"ms"<<std::endl;

	int z0=start_texel/(density_grid[0]*density_grid[1]);
	int z1=(start_texel+texels)/(density_grid[0]*density_grid[1]);

	// We will only copy WHOLE Z LAYERS at a time.
	if(z1>z0)
	{
		int t0=z0*density_grid[0]*density_grid[1];
		int t=z1*density_grid[0]*density_grid[1]-t0;
std::cout<<"\tMemCopy "<<timer.UpdateTime()<<"ms"<<std::endl;
		// Copy all the layers from the 2D dens_fb texture to the 3D texture.
		if(finalTexture[index])
		{
			finalTexture[index]->ensureTexture3DSizeAndFormat(m_pd3dDevice,density_grid[0],density_grid[1],density_grid[2],DXGI_FORMAT_R8G8B8A8_UNORM);
			D3D11_BOX sourceRegion;
			sourceRegion.left	=0;
			sourceRegion.right	=density_grid[0];
			sourceRegion.front	=0;
			sourceRegion.back	=1;
			for(int Z=z0;Z<z1;Z++)
			{
				sourceRegion.top	=density_grid[1]*Z;
				sourceRegion.bottom	=density_grid[1]*(Z+1);
				m_pImmediateContext->CopySubresourceRegion(finalTexture[index]->texture,0,0,0,Z,world_fb.GetColorTexture(),0,&sourceRegion);
			}
		}
		if(target)
			world_fb.CopyToMemory(m_pImmediateContext,target,t0,t);
	}
std::cout<<"\tGpuCopy "<<timer.UpdateTime()<<"ms"<<std::endl;
std::cout<<"\tRelease "<<timer.UpdateTime()<<"ms"<<std::endl;
}