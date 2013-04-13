
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

static simul::math::Matrix4x4 MakeVertexMatrix(const int *grid,int start_texel,int texels)
{
	simul::math::Matrix4x4 mat;
	mat.Unit();
	int gridsize=grid[0]*grid[1]*grid[2];
	//start_texel-=grid[0]*grid[1];
	if(start_texel<0)
		start_texel=0;
	//texels+=grid[0]*grid[1];
	if(start_texel+texels>gridsize)
		texels=start_texel-gridsize;
	float y_start=2.f*(float)start_texel/(float)gridsize-1.f;
	float y_end=2.f*(float)(start_texel+texels)/(float)gridsize-1.f;
	mat(1,1)=(y_end-y_start)/2.f;
	mat(1,3)=(y_end+y_start)/2.f;
	return mat;
}

GpuCloudGenerator::GpuCloudGenerator()
			:m_pd3dDevice(NULL)
			,m_pImmediateContext(NULL)
			,effect(NULL)
			,densityTechnique(NULL)
			,lightingTechnique(NULL)
			,transformTechnique(NULL)
			,density_texture(NULL)
			,density_texture_srv(NULL)
{
}

GpuCloudGenerator::~GpuCloudGenerator()
{
	InvalidateDeviceObjects();
}

void GpuCloudGenerator::CreateVolumeNoiseTexture(int size,const float *src_ptr)
{
}

void GpuCloudGenerator::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(ID3D1xDevice*)dev;
	SAFE_RELEASE(m_pImmediateContext);
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
	RecompileShaders();
	fb[0].RestoreDeviceObjects(m_pd3dDevice);
	fb[1].RestoreDeviceObjects(m_pd3dDevice);
	dens_fb.RestoreDeviceObjects(m_pd3dDevice);
	world_fb.RestoreDeviceObjects(m_pd3dDevice);
}

void GpuCloudGenerator::InvalidateDeviceObjects()
{
	fb[0].InvalidateDeviceObjects();
	fb[1].InvalidateDeviceObjects();
	dens_fb.InvalidateDeviceObjects();
	world_fb.InvalidateDeviceObjects();
	SAFE_RELEASE(m_pImmediateContext);
	SAFE_RELEASE(effect);
	SAFE_RELEASE(density_texture_srv);
	SAFE_RELEASE(density_texture);
	m_pd3dDevice=NULL;
}

void GpuCloudGenerator::RecompileShaders()
{
	SAFE_RELEASE(effect);
	HRESULT hr=CreateEffect(m_pd3dDevice,&effect,L"simul_gpu_clouds.fx");
	if(effect)
	{
		densityTechnique	=effect->GetTechniqueByName("simul_gpu_density");
		lightingTechnique	=effect->GetTechniqueByName("simul_gpu_lighting");
		transformTechnique	=effect->GetTechniqueByName("simul_gpu_transform");
	}
}

int GpuCloudGenerator::GetDensityGridsize(const int *grid)
{
	int size=4;
	//if(iformat==DXGI_FORMAT_R32G32B32A32_FLOAT)
	//	size=4;
	return grid[0]*grid[1]*grid[2]*size;
}

void *GpuCloudGenerator::FillDensityGrid(const int *density_grid
									,int start_texel
									,int texels
									,float humidity
									,float baseLayer
									,float transition
									,float time
									,int noise_size,int octaves,float persistence
									,const float  *noise_src_ptr)
									//,float amount_complete)
{
try{
simul::base::Timer timer;
timer.StartTime();
std::cout<<"Gpu clouds: FillDensityGrid\n";

	int new_density_gridsize=GetDensityGridsize(density_grid);
	dens_fb.SetWidthAndHeight(density_grid[0],density_grid[1]*density_grid[2]);
	/*if(!dens_fb.SetTargetFormat(0,iformat,GL_FLOAT))
	{
		itype=GL_INTENSITY;
		if(!dens_fb.SetTargetFormat(0,iformat=GL_INTENSITY32F_ARB,GL_FLOAT))
		{
			itype=GL_RGBA;
			dens_fb.InitColor_Tex(0,iformat=GL_RGBA32F_ARB,GL_FLOAT);
		}
	}*/
	simul::math::Vector3 noise_scale(1.f,1.f,(float)density_grid[2]/(float)density_grid[0]);
	
	//using noise_size and noise_src_ptr, make a 3d texture:
	ID3D11Texture3D *volume_noise_tex=make3DTexture(m_pd3dDevice,m_pImmediateContext,noise_size,noise_size,noise_size,DXGI_FORMAT_R32_FLOAT,noise_src_ptr);
	ID3D11ShaderResourceView* volume_noise_tex_srv;
	m_pd3dDevice->CreateShaderResourceView(volume_noise_tex,NULL,&volume_noise_tex_srv);
	m_pImmediateContext->GenerateMips(volume_noise_tex_srv);
	
	simul::math::Matrix4x4 vertexMatrix=MakeVertexMatrix(density_grid,start_texel,texels);

	setMatrix(effect,"vertexMatrix"				,vertexMatrix);
	setParameter(effect,"volumeNoiseTexture"	,volume_noise_tex_srv);
	setParameter(effect,"octaves"				,octaves);
	setParameter(effect,"persistence"			,persistence);
	setParameter(effect,"humidity"				,humidity);
	setParameter(effect,"time"					,time);
	setParameter(effect,"zPixel"				,1.f/(float)density_grid[2]);
	setParameter(effect,"zSize"					,(float)density_grid[2]);
	setParameter(effect,"noiseScale"			,noise_scale);
	setParameter(effect,"baseLayer"				,baseLayer);
	setParameter(effect,"transition"			,transition);
	
	dens_fb.Activate();
		ApplyPass(densityTechnique->GetPassByIndex(0));
		dens_fb.DrawQuad();
	dens_fb.Deactivate();
	density_gridsize=new_density_gridsize;
std::cout<<"\tDraw "<<timer.UpdateTime()<<"ms"<<std::endl;
	Ensure3DTextureSizeAndFormat(m_pd3dDevice,m_pImmediateContext,density_texture,density_texture_srv,density_grid[0],density_grid[1],density_grid[2],DXGI_FORMAT_R32G32B32A32_FLOAT);
std::cout<<"\tmake 3DTexture "<<timer.UpdateTime()<<"ms"<<std::endl;
	if(start_texel+texels>=density_grid[0]*density_grid[1]*density_grid[2])
	{
		D3D1x_MAPPED_TEXTURE3D dens_texture_mapped;
		Map3D(density_texture,&dens_texture_mapped);
		simul::sky::float4 *tex_data=(simul::sky::float4 *)(dens_texture_mapped.pData);
		float *density=new float[new_density_gridsize];
		dens_fb.CopyToMemory(density);
		memcpy(tex_data,density,new_density_gridsize*sizeof(float));
		Unmap3D(density_texture);
		//density_texture	=make3DTexture(m_pd3dDevice,m_pImmediateContext,density_grid[0],density_grid[1],density_grid[2]	,DXGI_FORMAT_R32G32B32A32_FLOAT,density);
		delete [] density;
	}
	SAFE_RELEASE(volume_noise_tex);
	SAFE_RELEASE(volume_noise_tex_srv);
std::cout<<"\tfill 3DTexture "<<timer.UpdateTime()<<"ms"<<std::endl;

	}
	catch(...)
	{
	}
	return NULL;
}

void GpuCloudGenerator::PerformGPURelight(	float *target
											,const int *light_grid
											,int start_texel
											,int texels
											,const int *density_grid
											,const float *Matrix4x4LightToDensityTexcoords
											,const float *lightspace_extinctions_float3)
{
try{
	for(int i=0;i<2;i++)
	{
		fb[i].SetWidthAndHeight(light_grid[0],light_grid[1]);
		//fb[i].InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	}
	
	ID3D1xEffectShaderResourceVariable*	input_light_texture	=effect->GetVariableByName("input_light_texture")->AsShaderResource();
	ID3D1xEffectShaderResourceVariable*	densityTexture		=effect->GetVariableByName("density_texture")->AsShaderResource();
	ID3D1xEffectMatrixVariable*	lightToDensityMatrix		=effect->GetVariableByName("transformMatrix")->AsMatrix();
	ID3D1xEffectVectorVariable*	extinctions					=effect->GetVariableByName("extinctions")->AsVector();
	ID3D1xEffectScalarVariable *zPosition					=effect->GetVariableByName("zPosition")->AsScalar();
	
	simul::math::Matrix4x4 vertexMatrix;
	vertexMatrix.Unit();
	setMatrix(effect,"vertexMatrix"				,vertexMatrix);

	lightToDensityMatrix->SetMatrix(Matrix4x4LightToDensityTexcoords);
	extinctions->SetFloatVector(lightspace_extinctions_float3);
	// initialize the first input texture.
	FramebufferDX1x *F[2];
	F[0]=&fb[0];
	F[1]=&fb[1];
	densityTexture->SetResource(density_texture_srv);
	
	if(start_texel==0)
	{
		F[0]->Activate();
			input_light_texture->SetResource(F[1]->GetBufferResource());
			F[0]->Clear(1.f,1.f,1.f,0.f);
		F[0]->Deactivate();
		F[0]->CopyToMemory(target);
	}
	int i0=start_texel/(light_grid[0]*light_grid[1]);
	int i1=(start_texel+texels)/(light_grid[0]*light_grid[1]);
	target+=i0*light_grid[0]*light_grid[1]*4;
	if(i0%2)
		std::swap(F[0],F[1]);
	for(int i=i0;i<i1;i++)//light_grid[2]-1;i++)
	{
		float zPos=((float)i+0.5f)/(float)light_grid[2];
		zPosition->SetFloat(zPos);
		F[1]->Activate();
			input_light_texture->SetResource(F[0]->GetBufferResource());
		ApplyPass(lightingTechnique->GetPassByIndex(0));
			F[1]->DrawQuad();
		F[1]->Deactivate();
		// Copy F[1] contents to the target
		F[1]->CopyToMemory(target);
		std::swap(F[0],F[1]);
		target+=light_grid[0]*light_grid[1]*4;
	}
	}
	catch(...)
	{
	}
}

void GpuCloudGenerator::GPUTransferDataToTexture(unsigned char *target
											,const float *DensityToLightTransform
											,const float *light,const int *light_grid
											,const float *ambient,const int *density_grid
											,int start_texel
											,int texels)
{
try{
	// For each level in the z direction, we render out a 2D texture and copy it to the target.
	world_fb.SetWidthAndHeight(density_grid[0],density_grid[1]*density_grid[2]);
	world_fb.SetTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
	ID3D11Texture3D* light_texture1		=make3DTexture(m_pd3dDevice,m_pImmediateContext	,light_grid[0]	,light_grid[1]	,light_grid[2]	,DXGI_FORMAT_R32G32B32A32_FLOAT,light);
	ID3D11ShaderResourceView* light_texture;
	m_pd3dDevice->CreateShaderResourceView(light_texture1,NULL,&light_texture);
	m_pImmediateContext->GenerateMips(light_texture);
	ID3D11Texture3D *ambient_texture1	=make3DTexture(m_pd3dDevice,m_pImmediateContext	,density_grid[0],density_grid[1],density_grid[2]	,DXGI_FORMAT_R32G32B32A32_FLOAT,ambient);
	ID3D11ShaderResourceView* ambient_texture;
	m_pd3dDevice->CreateShaderResourceView(ambient_texture1,NULL,&ambient_texture);
	m_pImmediateContext->GenerateMips(ambient_texture);

	ID3D1xEffectShaderResourceVariable*	densityTexture	=effect->GetVariableByName("density_texture")->AsShaderResource();
	ID3D1xEffectShaderResourceVariable*	lightTexture	=effect->GetVariableByName("light_texture")->AsShaderResource();
	ID3D1xEffectShaderResourceVariable*	ambientTexture	=effect->GetVariableByName("ambient_texture")->AsShaderResource();
	ID3D1xEffectMatrixVariable *transformMatrix			=effect->GetVariableByName("transformMatrix")->AsMatrix();
	transformMatrix->SetMatrix(DensityToLightTransform);
	ID3D1xEffectScalarVariable *zSize					=effect->GetVariableByName("zSize")->AsScalar();
	ID3D1xEffectScalarVariable *zPixel					=effect->GetVariableByName("zPixel")->AsScalar();

	simul::math::Matrix4x4 vertexMatrix;
	vertexMatrix=MakeVertexMatrix(density_grid,start_texel,texels);
	setMatrix(effect,"vertexMatrix"				,vertexMatrix);

	zSize->SetFloat((float)density_grid[2]);
	zPixel->SetFloat(1.f/(float)density_grid[2]);

	densityTexture->SetResource(density_texture_srv);
	lightTexture->SetResource(light_texture);
	ambientTexture->SetResource(ambient_texture);
	// Instead of a loop, we do a single big render, by tiling the z layers in the y direction.

	world_fb.Activate();
		ApplyPass(transformTechnique->GetPassByIndex(0));
		world_fb.DrawQuad();
	world_fb.Deactivate();
	world_fb.CopyToMemory(target,start_texel,texels);

	SAFE_RELEASE(ambient_texture);
	SAFE_RELEASE(ambient_texture1);
	SAFE_RELEASE(light_texture);
	SAFE_RELEASE(light_texture1);
	}
	catch(...)
	{
	}
}