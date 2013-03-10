
#include "GpuCloudGenerator.h"
#include "Simul/Platform/DirectX9/Macros.h"
#include "Simul/Platform/DirectX9/CreateDX9Effect.h"
#include "Simul/Base/Timer.h"
<<<<<<< HEAD

using namespace simul;
using namespace dx9;
#ifdef XBOX
	static D3DPOOL d3d_memory_pool=D3DUSAGE_CPU_CACHED_MEMORY;
#else
	static D3DPOOL d3d_memory_pool=D3DPOOL_MANAGED;
#endif

=======
>>>>>>> master
void GpuCloudGenerator::Target::Release()
{
	SAFE_RELEASE(pLightRenderTarget[0]);
	SAFE_RELEASE(pLightRenderTarget[1]);
	SAFE_RELEASE(target_textures[0]);
	SAFE_RELEASE(target_textures[1]);
	SAFE_RELEASE(pBuf);
}

<<<<<<< HEAD
void GpuCloudGenerator::Target::Init(LPDIRECT3DDEVICE9 dev,int w,int h)
{
	Release();
	m_pd3dDevice=dev;
	for(int i=0;i<2;i++)
	{
		m_pd3dDevice->CreateTexture(	w,
										h,
										1,
										D3DUSAGE_RENDERTARGET,
										D3DFMT_G32R32F,
										D3DPOOL_DEFAULT,
										&target_textures[i],
										NULL);
		pLightRenderTarget[i]=MakeRenderTarget(target_textures[i]);
	}
	width=w;
	height=h;
	bytes_per_texel=2*sizeof(float);
}

int GpuCloudGenerator::Target::Copy(int index,unsigned char *dest)
{
	// Copy the texture to an offscreen surface:
	m_pd3dDevice->GetRenderTargetData(pLightRenderTarget[index],pBuf);
	D3DLOCKED_RECT rect;
	pBuf->LockRect(&rect,NULL,0);
	unsigned char *source=(unsigned char*)rect.pBits;
	// Copy the light data to the target 3D grid:
	int size=bytes_per_texel*width*height;
	memcpy(dest,source,size);
	pBuf->UnlockRect();
	return size;
}
		
=======
>>>>>>> master
GpuCloudGenerator::GpuCloudGenerator():
	m_pGPULightingEffect(NULL)
	,gpuLighting2DSurface(NULL)
	,volume_noise_texture(NULL)
	,m_pd3dDevice(NULL)
<<<<<<< HEAD
	,ambient_texture(NULL)
	,direct_texture(NULL)
	,density_texture(NULL)
	,iformat(D3DFMT_R32F)
=======
	,flattened_target_texture(NULL)
	,pFlattenedRenderTarget(NULL)
	,ambient_texture(NULL)
	,direct_texture(NULL)
>>>>>>> master
{
}

GpuCloudGenerator::~GpuCloudGenerator()
{
	InvalidateDeviceObjects();
}

void GpuCloudGenerator::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
<<<<<<< HEAD
	fb[0].RestoreDeviceObjects(dev);
	fb[1].RestoreDeviceObjects(dev);
	world_fb.RestoreDeviceObjects(dev);
	dens_fb.RestoreDeviceObjects(dev);
=======
>>>>>>> master
}

void GpuCloudGenerator::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(m_pGPULightingEffect)
        hr=m_pGPULightingEffect->OnLostDevice();
	xy_target.Release();
	yz_target.Release();
	zx_target.Release();
	SAFE_RELEASE(m_pGPULightingEffect);
	SAFE_RELEASE(gpuLighting2DSurface);
<<<<<<< HEAD
	m_pd3dDevice=NULL;
	SAFE_RELEASE(volume_noise_texture);
	SAFE_RELEASE(direct_texture);
	SAFE_RELEASE(ambient_texture);
	fb[0].InvalidateDeviceObjects();
	fb[1].InvalidateDeviceObjects();
	world_fb.InvalidateDeviceObjects();
	dens_fb.InvalidateDeviceObjects();
=======
	SAFE_RELEASE(gpuLighting2DSurface);
	m_pd3dDevice=NULL;
	SAFE_RELEASE(volume_noise_texture);
	SAFE_RELEASE(flattened_target_texture);
	SAFE_RELEASE(pFlattenedRenderTarget);
	SAFE_RELEASE(direct_texture);
	SAFE_RELEASE(ambient_texture);
>>>>>>> master
}

void GpuCloudGenerator::RecompileShaders()
{
	SAFE_RELEASE(m_pGPULightingEffect);
}

void GpuCloudGenerator::CreateVolumeNoiseTexture(int size,const float *src_ptr)
{
	SAFE_RELEASE(volume_noise_texture);
	HRESULT hr=S_OK;
	// Otherwise create it:
#ifndef XBOX
	D3DFORMAT tex_format=D3DFMT_R32F;
#else
	D3DFORMAT tex_format=D3DFMT_LIN_R32F;
#endif
	if(FAILED(hr=D3DXCreateVolumeTexture(m_pd3dDevice,size,size,size,1,0,tex_format,D3DPOOL_MANAGED,&volume_noise_texture)))
		return;
	D3DLOCKED_BOX lockedBox={0};
	if(FAILED(hr=volume_noise_texture->LockBox(0,&lockedBox,NULL,NULL)))
		return;
		// Copy the array of floats into the texture.
	float *float_ptr=(float *)(lockedBox.pBits);
	for(int i=0;i<size*size*size;i++)
		*float_ptr++=(*src_ptr++);
	hr=volume_noise_texture->UnlockBox(0);
	volume_noise_texture->GenerateMipSubLevels();
}


bool GpuCloudGenerator::CanPerformGPULighting() const
{
	return true;
}

<<<<<<< HEAD
int GpuCloudGenerator::GetDensityGridsize(const int *grid)
{
	int size=1;
	//size=4;
	return grid[0]*grid[1]*grid[2]*size;
=======
void GpuCloudGenerator::SetGPULightingParameters(const float *Matrix4x4LightToDensityTexcoords,const unsigned *light_grid,const float *lightspace_extinctions_float3
	,int num_octaves,float persistence_val,float humidity_val
	,float time_val,int w,int l,int d
	,int size,const float *src_ptr)
{
	if(w!=cloud_tex_width_x||l!=cloud_tex_length_y||d!=cloud_tex_depth_z)
	{
		// This surface must be invalidated because it will be the wrong size.
		SAFE_RELEASE(gpuLighting2DSurface);
		SAFE_RELEASE(flattened_target_texture);
		SAFE_RELEASE(pFlattenedRenderTarget);
		SAFE_RELEASE(ambient_texture);
		xy_target.Release();
		yz_target.Release();
		zx_target.Release();
	}
		HRESULT hr;
	if(!volume_noise_texture)
		CreateVolumeNoiseTexture(size,src_ptr);
	BaseGpuCloudGenerator::SetGPULightingParameters(Matrix4x4LightToDensityTexcoords,light_grid,lightspace_extinctions_float3,num_octaves,persistence_val,humidity_val,time_val,w,l,d,size,src_ptr);
	for(int i=0;i<2;i++)
	{
	// Make a floating point texture.
		if(!xy_target.target_textures[i])
		{
			if(FAILED(hr=m_pd3dDevice->CreateTexture(cloud_tex_width_x,
				cloud_tex_length_y,0,D3DUSAGE_RENDERTARGET|D3DUSAGE_AUTOGENMIPMAP,
				D3DFMT_G32R32F,D3DPOOL_DEFAULT,&(xy_target.target_textures[i]),NULL)))
				throw std::runtime_error("GpuCloudGenerator::SetGPULightingParameters: Can't make target textures");
			SAFE_RELEASE(xy_target.pLightRenderTarget[i]);
			xy_target.target_textures[i]->GetSurfaceLevel(0,&xy_target.pLightRenderTarget[i]);
		}
		if(!yz_target.target_textures[i])		
		{
			if(FAILED(hr=m_pd3dDevice->CreateTexture(cloud_tex_length_y,
				cloud_tex_depth_z,0,D3DUSAGE_RENDERTARGET|D3DUSAGE_AUTOGENMIPMAP,
				D3DFMT_G32R32F,D3DPOOL_DEFAULT,&(yz_target.target_textures[i]),NULL)))
				throw std::runtime_error("GpuCloudGenerator::SetGPULightingParameters: Can't make target textures");
			SAFE_RELEASE(yz_target.pLightRenderTarget[i]);
			yz_target.target_textures[i]->GetSurfaceLevel(0,&yz_target.pLightRenderTarget[i]);
		}
		if(!zx_target.target_textures[i])	
		{
			if(FAILED(hr=m_pd3dDevice->CreateTexture(cloud_tex_depth_z,
				cloud_tex_width_x,0,D3DUSAGE_RENDERTARGET|D3DUSAGE_AUTOGENMIPMAP,
				D3DFMT_G32R32F,D3DPOOL_DEFAULT,&(zx_target.target_textures[i]),NULL)))
				throw std::runtime_error("GpuCloudGenerator::SetGPULightingParameters: Can't make target textures");
			SAFE_RELEASE(zx_target.pLightRenderTarget[i]);
			zx_target.target_textures[i]->GetSurfaceLevel(0,&zx_target.pLightRenderTarget[i]);
		}
	}
	if(!xy_target.pBuf)
		hr=m_pd3dDevice->CreateOffscreenPlainSurface(cloud_tex_width_x,cloud_tex_length_y,D3DFMT_G32R32F,D3DPOOL_SYSTEMMEM,&xy_target.pBuf,NULL);
	if(!yz_target.pBuf)
		hr=m_pd3dDevice->CreateOffscreenPlainSurface(cloud_tex_length_y,cloud_tex_depth_z,D3DFMT_G32R32F,D3DPOOL_SYSTEMMEM,&yz_target.pBuf,NULL);
	if(!zx_target.pBuf)
		hr=m_pd3dDevice->CreateOffscreenPlainSurface(cloud_tex_depth_z,cloud_tex_width_x,D3DFMT_G32R32F,D3DPOOL_SYSTEMMEM,&zx_target.pBuf,NULL);
	// Make a 32-bit target texture.
	if(!flattened_target_texture)
	{
		V_CHECK(m_pd3dDevice->CreateTexture(cloud_tex_width_x,cloud_tex_length_y*cloud_tex_depth_z
												,0,D3DUSAGE_RENDERTARGET|D3DUSAGE_AUTOGENMIPMAP,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&flattened_target_texture,NULL));
		flattened_target_texture->GetSurfaceLevel(0,&pFlattenedRenderTarget);
	}
	if(!ambient_texture)
		V_CHECK(D3DXCreateVolumeTexture(m_pd3dDevice,
			cloud_tex_width_x,cloud_tex_length_y,cloud_tex_depth_z
			,1,0,D3DFMT_G32R32F,D3DPOOL_MANAGED,&ambient_texture));
>>>>>>> master
}

void GpuCloudGenerator::MakeShader()
{
	bool wrap=true;
	bool y_vertical=false;
	std::map<std::string,std::string> defines=MakeDefinesList(simul::clouds::BaseCloudRenderer::FRAGMENT,wrap,y_vertical);
	V_CHECK(CreateDX9Effect(m_pd3dDevice,m_pGPULightingEffect,"simul_gpulighting.fx",defines));

	inputLightTexture			=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"inputLightTexture");
	cloudDensityTexture			=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"cloudDensityTexture");
	volumeNoiseTexture			=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"volumeNoiseTexture");
	extinctions					=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"extinctions");
	lightToDensityMatrix		=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"transformMatrix");
	texCoordOffset				=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"texCoordOffset");
	octaves						=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"octaves");
	persistence					=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"persistence");
	humidity					=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"humidity");
	time						=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"time");
	zPixel						=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"zPixel");
	zPosition					=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"zPosition");
							
	noiseScale					=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"noiseScale");

	inputDirectLightTexture		=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"inputDirectLightTexture");
	inputAmbientLightTexture	=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"inputAmbientLightTexture");
	densityToLightMatrix		=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"transformMatrix");
	cloudDensityTexture			=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"cloudDensityTexture");
							
	zSize						=(void*)m_pGPULightingEffect->GetParameterByName(NULL,"zSize");
}

<<<<<<< HEAD
	
void* GpuCloudGenerator::FillDensityGrid(const int *density_grid
							,int start_texel
							,int texels
						,float hum
						,float time_val
						,int noise_size,int oct,float pers
						,const float *noise_src_ptr)
{
	if(!m_pGPULightingEffect)
		MakeShader();
	CreateVolumeNoiseTexture(noise_size,noise_src_ptr);
	// We render out a 2D texture with each XY layer laid end-to-end, and copy it to the target.
	dens_fb.SetWidthAndHeight(density_grid[0],density_grid[1]*density_grid[2]);
	if(!dens_fb.SetFormat(iformat))
	{
		dens_fb.SetFormat(iformat=D3DFMT_A32B32G32R32F);
	}
	D3DXVECTOR4 noise_scale(1.f,1.f,(float)density_grid[2]/(float)density_grid[0],0);
	m_pGPULightingEffect->SetTexture((D3DXHANDLE)volumeNoiseTexture,volume_noise_texture);
	
	D3DXHANDLE techniqueDensity	=m_pGPULightingEffect->GetTechniqueByName("cloud_density");
	m_pGPULightingEffect->SetTechnique(techniqueDensity);

	m_pGPULightingEffect->SetInt((D3DXHANDLE)octaves		,oct);
	m_pGPULightingEffect->SetFloat((D3DXHANDLE)persistence	,pers);
	m_pGPULightingEffect->SetFloat((D3DXHANDLE)humidity		,hum);
	m_pGPULightingEffect->SetFloat((D3DXHANDLE)time			,time_val);
	m_pGPULightingEffect->SetFloat((D3DXHANDLE)zPixel		,1.f/(float)density_grid[2]);
	m_pGPULightingEffect->SetFloat((D3DXHANDLE)zSize		,(float)density_grid[2]);
	m_pGPULightingEffect->SetVector((D3DXHANDLE)noiseScale	,&noise_scale);
	m_pd3dDevice->BeginScene();
// Set up the rendertarget:
	dens_fb.Activate();
		DrawFullScreenQuad(m_pd3dDevice,m_pGPULightingEffect);
	dens_fb.Deactivate();
	m_pd3dDevice->EndScene();
	
	// Now put the generated data into a 3D texture.
	// Create the texture.
	SAFE_RELEASE(density_texture);
	D3DXCreateVolumeTexture(m_pd3dDevice,density_grid[0],density_grid[1],density_grid[2],1,0,iformat,d3d_memory_pool,&density_texture);
	// First copy the texture to an offscreen surface:
	IDirect3DSurface9 *offscreenSurface=NULL;
	m_pd3dDevice->CreateOffscreenPlainSurface(density_grid[0],density_grid[1]*density_grid[2],iformat,D3DPOOL_SYSTEMMEM,&offscreenSurface,NULL);
	m_pd3dDevice->GetRenderTargetData(dens_fb.m_pHDRRenderTarget,offscreenSurface);
	
	// The 2D source:
	D3DLOCKED_RECT rect;
	offscreenSurface->LockRect(&rect,NULL,0);
	
	// The 3D target:
	D3DLOCKED_BOX box;
	density_texture->LockBox(0,&box,NULL,NULL);

	// Copy the light data to the target 3D grid:
	int size=sizeof(float)*density_grid[0]*density_grid[1]*density_grid[2];
	memcpy(box.pBits,rect.pBits,size);
	density_texture->UnlockBox(0);
	offscreenSurface->UnlockRect();
	
	return dens_fb.GetColorTex();
}

void GpuCloudGenerator::PerformGPURelight(float *dest,const int *light_gridsizes
							,int start_texel,int texels
							,const int *density_grid
							,const float *Matrix4x4LightToDensityTexcoords
							,const float *lightspace_extinctions_float3)
=======
void GpuCloudGenerator::PerformFullGPURelight(float *target_direct_grid)
>>>>>>> master
{
	if(!m_pGPULightingEffect)
		MakeShader();
	// To use the GPU to relight clouds:

	// 1. Create two 2D target rendertextures with X and Y sizes given by the light_gridsizes[0] and [1].
	// 2. Set the transform matrix to LightToDensityTransform.
	// 3. Initialize the first target to 1.0
	// 4. Use the first target and the density volume texture as an input to rendering the second target.
	// 5. Swap targets and repeat for all the positions in the Z grid.

	// 1. Create two 2D target rendertextures:
	HRESULT hr=S_OK;
	if(!m_pd3dDevice)
<<<<<<< HEAD
		return ;
	LPDIRECT3DSURFACE9				pOldRenderTarget	=NULL;
=======
		return;
simul::base::Timer timer;
timer.StartTime();
std::cout<<"\t\nPerformFullGPURelight:"<<std::endl;
	LPDIRECT3DSURFACE9				pOldRenderTarget	=NULL;

>>>>>>> master
	D3DXHANDLE m_hTechniqueGpuLighting	=m_pGPULightingEffect->GetTechniqueByName("simul_gpulighting");
	m_pGPULightingEffect->SetTechnique(m_hTechniqueGpuLighting);
	// store the current rendertarget for later:
	hr=m_pd3dDevice->GetRenderTarget(0,&pOldRenderTarget);
	// Make the input texture:
	Target *target=NULL;
<<<<<<< HEAD
	if(light_gridsizes[0]==density_grid[0]
		&&light_gridsizes[1]==density_grid[1])
	{
		target=&xy_target;
	}
	else if(light_gridsizes[0]==density_grid[1]
		&&light_gridsizes[1]==density_grid[2])
	{
		target=&yz_target;
	}
	else if(light_gridsizes[0]==density_grid[2]
		&&light_gridsizes[1]==density_grid[0])
=======
	if(light_gridsizes[0]==cloud_tex_width_x
		&&light_gridsizes[1]==cloud_tex_length_y)
	{
		target=&xy_target;
	}
	else if(light_gridsizes[0]==cloud_tex_length_y
		&&light_gridsizes[1]==cloud_tex_depth_z)
	{
		target=&yz_target;
	}
	else if(light_gridsizes[0]==cloud_tex_depth_z
		&&light_gridsizes[1]==cloud_tex_width_x)
>>>>>>> master
	{
		target=&zx_target;
	}
	else
<<<<<<< HEAD
		throw std::runtime_error("GpuCloudGenerator::PerformGPURelight: Can't find appropriate target textures");
	target->Init(m_pd3dDevice,light_gridsizes[0],light_gridsizes[1]);
	// Two floats per texture.
	SAFE_RELEASE(target->pBuf);
	m_pd3dDevice->CreateOffscreenPlainSurface(light_gridsizes[0],light_gridsizes[1],D3DFMT_G32R32F,D3DPOOL_SYSTEMMEM,&target->pBuf,NULL);
=======
		throw std::runtime_error("GpuCloudGenerator::PerformFullGPURelight: Can't find appropriate target textures");
>>>>>>> master

	static unsigned colr=0x00FFFF00;
	
	// Make a floating point texture.
	hr=m_pd3dDevice->SetRenderTarget(0,target->pLightRenderTarget[0]);
	hr=m_pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,colr,1.f,0L);
<<<<<<< HEAD
	m_pd3dDevice->SetRenderTarget(0,pOldRenderTarget);
	unsigned char *d=(unsigned char *)dest;
	d+=target->Copy(0,d);

	m_pGPULightingEffect->SetVector((D3DXHANDLE)extinctions,(D3DXVECTOR4*)(lightspace_extinctions_float3));
	float offset[]={0.5f/(float)light_gridsizes[0],0.5f/(float)light_gridsizes[1],0,0};
	m_pGPULightingEffect->SetVector((D3DXHANDLE)texCoordOffset,(D3DXVECTOR4*)(offset));
	m_pGPULightingEffect->SetMatrix((D3DXHANDLE)lightToDensityMatrix,(D3DXMATRIX*)(Matrix4x4LightToDensityTexcoords));
	m_pGPULightingEffect->SetFloat((D3DXHANDLE)zPixel,1.f/(float)density_grid[2]);

	// 4. Use the first target and the density volume texture as an input to rendering the second target.
	// 5. Swap targets and repeat for all the positions in the Z grid.#m_pd3dDevice->BeginScene();
	// For each position along the light direction
	//double rt_time=0.0,mem_time=0.0;
	for(int i=0;i<(int)light_gridsizes[2]-1;i++)
=======
	//	m_pd3dDevice->SetRenderTarget(0,pOldRenderTarget);

std::cout<<"\tCreate RTs "<<timer.UpdateTime()<<std::endl;
	m_pGPULightingEffect->SetInt((D3DXHANDLE)octaves,num_octaves);
	m_pGPULightingEffect->SetFloat((D3DXHANDLE)persistence,persistence_val);
	m_pGPULightingEffect->SetFloat((D3DXHANDLE)humidity,humidity_val);
	m_pGPULightingEffect->SetFloat((D3DXHANDLE)time,time_val);
	m_pGPULightingEffect->SetVector((D3DXHANDLE)extinctions,(D3DXVECTOR4*)(light_extinctions));
	float offset[]={0.5f/(float)light_gridsizes[0],0.5f/(float)light_gridsizes[1],0,0};
	m_pGPULightingEffect->SetVector((D3DXHANDLE)texCoordOffset,(D3DXVECTOR4*)(offset));
	m_pGPULightingEffect->SetMatrix((D3DXHANDLE)lightToDensityMatrix,(D3DXMATRIX*)(LightToDensityTransform));
	m_pGPULightingEffect->SetTexture((D3DXHANDLE)volumeNoiseTexture,volume_noise_texture);
	m_pGPULightingEffect->SetFloat((D3DXHANDLE)zPixel,1.f/(float)cloud_tex_depth_z);

	D3DXVECTOR4 noise_scale(1.f,1.f,(float)cloud_tex_depth_z/(float)cloud_tex_width_x,0);
	m_pGPULightingEffect->SetVector((D3DXHANDLE)noiseScale,&noise_scale);
	// 4. Use the first target and the density volume texture as an input to rendering the second target.
	// 5. Swap targets and repeat for all the positions in the Z grid.#m_pd3dDevice->BeginScene();
	m_pd3dDevice->BeginScene();
std::cout<<"\tCreateOffscreenPlainSurface "<<timer.UpdateTime()<<std::endl;
	// For each position along the light direction
	unsigned char *dest=(unsigned char*)target_direct_grid;
	//double rt_time=0.0,mem_time=0.0;
	for(int i=0;i<(int)light_gridsizes[2];i++)
>>>>>>> master
	{
		static float cc=-0.5f;
		float zPos=((float)i+cc)/(float)light_gridsizes[2];
		static float zz=0;
		if(zz>0)
			zPos=zz;
		m_pGPULightingEffect->SetFloat((D3DXHANDLE)zPosition,zPos);
// Set up the rendertarget:
		hr=m_pd3dDevice->SetRenderTarget(0,target->pLightRenderTarget[1]);
		m_pGPULightingEffect->SetTexture((D3DXHANDLE)inputLightTexture,target->target_textures[0]);
<<<<<<< HEAD
		m_pGPULightingEffect->SetTexture((D3DXHANDLE)cloudDensityTexture,density_texture);
	m_pd3dDevice->BeginScene();
		DrawFullScreenQuad(m_pd3dDevice,m_pGPULightingEffect);
	m_pd3dDevice->EndScene();
		target->Swap();
		hr=m_pd3dDevice->SetRenderTarget(0,pOldRenderTarget);
		d+=target->Copy(0,d);
	}
	m_pd3dDevice->SetRenderTarget(0,pOldRenderTarget);
	SAFE_RELEASE(pOldRenderTarget);
}

void GpuCloudGenerator::EnsureVolumeTexture(LPDIRECT3DVOLUMETEXTURE9 &tex,const int *grid,const float *src)
{
	D3DVOLUME_DESC vd={D3DFMT_G32R32F,D3DRTYPE_VOLUME,0,D3DPOOL_MANAGED,0,0,0};
	if(tex)
		tex->GetLevelDesc(0, &vd);
	D3DLOCKED_BOX lockedBox;
	unsigned size3d=sizeof(unsigned char)*grid[0]*grid[1]*grid[2];
	if(!tex||vd.Width!=grid[0]||vd.Height!=grid[1]||vd.Depth!=grid[2])
	{
		SAFE_RELEASE(tex);
		V_CHECK(D3DXCreateVolumeTexture(m_pd3dDevice,
			grid[0],grid[1],grid[2],
			1,0,D3DFMT_G32R32F,D3DPOOL_MANAGED,&tex));
	}
	// Copy the light data to the target 3D grid:
	tex->LockBox(0,&lockedBox,NULL,NULL);
	unsigned char *target=(unsigned char*)lockedBox.pBits;
	memcpy(target,src,sizeof(float)*2*size3d);
	tex->UnlockBox(0);
}

// Use the GPU to transfer the oblique light grid data to the main grid
void GpuCloudGenerator::GPUTransferDataToTexture(	unsigned char *target
											,const float *DensityToLightTransform
											,const float *light,const int *light_gridsizes
											,const float *ambient,const int *density_grid
											,int texels
											,int start_texel)
=======
//std::cout<<"\t\t\tSetRenderTarget "<<timer.UpdateTime()<<std::endl;
		//RenderTexture(m_pd3dDevice,0,0,light_gridsizes[0],light_gridsizes[1],
		//			  target->target_textures[0],m_pGPULightingEffect,NULL);
		DrawFullScreenQuad(m_pd3dDevice,m_pGPULightingEffect);
//std::cout<<"\t\t\tRenderTexture "<<timer.UpdateTime()<<std::endl;
		//rt_time+=timer.UpdateTime();
		target->Swap();
		// Copy the texture to an offscreen surface:
		hr=m_pd3dDevice->GetRenderTargetData(target->pLightRenderTarget[0],target->pBuf);
//std::cout<<"\t\t\tGetRenderTargetData "<<timer.UpdateTime()<<std::endl;
		D3DLOCKED_RECT rect;
		target->pBuf->LockRect(&rect,NULL,0);
		unsigned char *source=(unsigned char*)rect.pBits;
		// Copy the light data to the target 3D grid:
		int size=2*sizeof(float)*light_gridsizes[0]*light_gridsizes[1];
		memcpy(dest,source,size);
//std::cout<<"\t\t\tmemcpy "<<timer.UpdateTime()<<std::endl;
		dest+=size;
		target->pBuf->UnlockRect();
		//mem_time+=timer.UpdateTime();
	}
std::cout<<"\tRenderTextures "<<timer.UpdateTime()<<std::endl;
//std::cout<<"\t\tmemcpy "<<mem_time<<std::endl;
std::cout<<"\tpBuf "<<timer.UpdateTime()<<std::endl;
	m_pd3dDevice->EndScene();
	m_pd3dDevice->SetRenderTarget(0,pOldRenderTarget);
std::cout<<"\tSetRenderTarget "<<timer.UpdateTime()<<std::endl;

	SAFE_RELEASE(pOldRenderTarget);
std::cout<<"\tSAFE_RELEASE(pOldRenderTarget "<<timer.UpdateTime()<<std::endl;
std::cout<<"\tEnd "<<timer.UpdateTime()<<std::endl;
double t1=timer.UpdateTime();
double t2=timer.UpdateTime();
std::cout<<"\tEnd "<<t2<<std::endl;
}
// Use the GPU to transfer the oblique light grid data to the main grid
void GpuCloudGenerator::GPUTransferDataToTexture(	unsigned char *target_grid
													,const float *direct_grid
													,const float *ambient_grid)
>>>>>>> master
{
	HRESULT hr=S_OK;
	if(!m_pd3dDevice)
		return;
	std::cout<<"\t\nGPUTransferDataToTexture:"<<std::endl;
	simul::base::Timer timer;
	timer.StartTime();
<<<<<<< HEAD
	D3DXHANDLE m_hTechniqueGpuTransformLightgrid	=m_pGPULightingEffect->GetTechniqueByName("simul_transform_lightgrid");
	m_pGPULightingEffect->SetTechnique(m_hTechniqueGpuTransformLightgrid);
=======
	LPDIRECT3DSURFACE9			pOldRenderTarget			=NULL;
	D3DXHANDLE m_hTechniqueGpuTransformLightgrid	=m_pGPULightingEffect->GetTechniqueByName("simul_transform_lightgrid");
	m_pGPULightingEffect->SetTechnique(m_hTechniqueGpuTransformLightgrid);
	// store the current rendertarget for later:
	hr=m_pd3dDevice->GetRenderTarget(0,&pOldRenderTarget);
>>>>>>> master
	// Make the input textures:
	static unsigned colr=0x00FFFF00;
	
std::cout<<"\tCreateTextures "<<timer.UpdateTime()<<std::endl;
	
	D3DLOCKED_RECT rect;
<<<<<<< HEAD
	// the light textures:
	direct_texture=(LPDIRECT3DVOLUMETEXTURE9)light;
	ambient_texture=(LPDIRECT3DVOLUMETEXTURE9)ambient;
	//EnsureVolumeTexture(direct_texture,light_gridsizes,light);
	//EnsureVolumeTexture(ambient_texture,density_grid,ambient);
	float offset[]={0.5f/(float)density_grid[0],0.5f/(float)(density_grid[1]*density_grid[2]),0,0};
	m_pGPULightingEffect->SetVector((D3DXHANDLE)texCoordOffset,(D3DXVECTOR4*)(offset));
	m_pGPULightingEffect->SetMatrix((D3DXHANDLE)densityToLightMatrix,(D3DXMATRIX*)(DensityToLightTransform));
	m_pGPULightingEffect->SetFloat((D3DXHANDLE)zPixel,1.f/(float)density_grid[2]);
	m_pGPULightingEffect->SetTexture((D3DXHANDLE)inputDirectLightTexture,direct_texture);
	m_pGPULightingEffect->SetTexture((D3DXHANDLE)inputAmbientLightTexture,ambient_texture);
	m_pGPULightingEffect->SetTexture((D3DXHANDLE)cloudDensityTexture,density_texture);
	// 4. Use the first target and the density volume texture as an input to rendering the second target.
	// 5. Swap targets and repeat for all the positions in the Z grid.#m_pd3dDevice->BeginScene();
	// For each xy surface
	// Set up the rendertarget:
	world_fb.SetFormat(D3DFMT_A8R8G8B8);
	world_fb.SetWidthAndHeight(density_grid[0],density_grid[1]*density_grid[2]);
	{
		world_fb.Activate();
		m_pd3dDevice->BeginScene();
		m_pGPULightingEffect->SetInt((D3DXHANDLE)zSize,density_grid[2]);
		//RenderTexture(m_pd3dDevice,0,0,density_grid[0],density_grid[1]*density_grid[2],NULL,m_pGPULightingEffect,m_hTechniqueGpuTransformLightgrid);
		DrawFullScreenQuad(m_pd3dDevice,m_pGPULightingEffect);
		m_pd3dDevice->EndScene();
		world_fb.Deactivate();

		// Copy the texture to an offscreen surface:
		if(!gpuLighting2DSurface)
			hr=m_pd3dDevice->CreateOffscreenPlainSurface(density_grid[0],density_grid[1]*density_grid[2],D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&gpuLighting2DSurface,NULL);

		hr=m_pd3dDevice->GetRenderTargetData(world_fb.m_pHDRRenderTarget,gpuLighting2DSurface);

		gpuLighting2DSurface->LockRect(&rect,NULL,0);
		unsigned char *source=(unsigned char*)rect.pBits;
		// Copy the light data to the target 3D grid:
		unsigned size2d=4*sizeof(unsigned char)*density_grid[0]*density_grid[1]*density_grid[2];
		memcpy(target,source,size2d);
		//target_grid+=size2d;
		gpuLighting2DSurface->UnlockRect();

	}

=======
	D3DLOCKED_BOX lockedBox;
	unsigned size3d=sizeof(unsigned char)*light_gridsizes[0]*light_gridsizes[1]*light_gridsizes[2];
	// the light textures:
	if(direct_grid)
	{
		D3DVOLUME_DESC vd={D3DFMT_G32R32F,D3DRTYPE_VOLUME,0,D3DPOOL_MANAGED,0,0,0};
		if(direct_texture)
			direct_texture->GetLevelDesc(0, &vd);
		if(!direct_texture||vd.Width!=light_gridsizes[0]||vd.Height!=light_gridsizes[1]||vd.Depth!=light_gridsizes[2])
		{
			SAFE_RELEASE(direct_texture);
			V_CHECK(D3DXCreateVolumeTexture(m_pd3dDevice,
				light_gridsizes[0],light_gridsizes[1],light_gridsizes[2],
				1,0,D3DFMT_G32R32F,D3DPOOL_MANAGED,&direct_texture));
		}
		// Copy the light data to the target 3D grid:
		direct_texture->LockBox(0,&lockedBox,NULL,NULL);
		unsigned char *target=(unsigned char*)lockedBox.pBits;
		memcpy(target,direct_grid,sizeof(float)*2*size3d);
		direct_texture->UnlockBox(0);
	}
std::cout<<"\t\tD3DXCreateVolumeTexture "<<timer.UpdateTime()<<std::endl;
	if(ambient_grid)
	{
		ambient_texture->LockBox(0,&lockedBox,NULL,NULL);
		unsigned char *target=(unsigned char*)lockedBox.pBits;
		memcpy(target,ambient_grid,sizeof(float)*2*size3d);
		ambient_texture->UnlockBox(0);
	}
std::cout<<"\t\tambient_grid "<<timer.UpdateTime()<<std::endl;
	float offset[]={0.5f/(float)cloud_tex_width_x,0.5f/(float)(cloud_tex_length_y*cloud_tex_depth_z),0,0};
	m_pGPULightingEffect->SetVector((D3DXHANDLE)texCoordOffset,(D3DXVECTOR4*)(offset));
	m_pGPULightingEffect->SetMatrix((D3DXHANDLE)densityToLightMatrix,(D3DXMATRIX*)(DensityToLightTransform));
	m_pGPULightingEffect->SetInt((D3DXHANDLE)octaves,num_octaves);
	m_pGPULightingEffect->SetFloat((D3DXHANDLE)persistence,persistence_val);
	m_pGPULightingEffect->SetFloat((D3DXHANDLE)humidity,humidity_val);
	m_pGPULightingEffect->SetFloat((D3DXHANDLE)time,time_val);
	m_pGPULightingEffect->SetFloat((D3DXHANDLE)zPixel,1.f/(float)cloud_tex_depth_z);

	D3DXVECTOR4 noise_scale(1.f,1.f,(float)cloud_tex_depth_z/(float)cloud_tex_width_x,0);
	m_pGPULightingEffect->SetVector((D3DXHANDLE)noiseScale,&noise_scale);

	m_pGPULightingEffect->SetTexture((D3DXHANDLE)inputDirectLightTexture,direct_texture);
	m_pGPULightingEffect->SetTexture((D3DXHANDLE)volumeNoiseTexture,volume_noise_texture);
	m_pGPULightingEffect->SetTexture((D3DXHANDLE)inputAmbientLightTexture,ambient_texture);
	// 4. Use the first target and the density volume texture as an input to rendering the second target.
	// 5. Swap targets and repeat for all the positions in the Z grid.#m_pd3dDevice->BeginScene();
	m_pd3dDevice->BeginScene();
	// For each xy surface
	// Set up the rendertarget:
	hr=m_pd3dDevice->SetRenderTarget(0,pFlattenedRenderTarget);
std::cout<<"\t\tSetFlattenedRenderTarget "<<timer.UpdateTime()<<std::endl;
	{
		m_pGPULightingEffect->SetInt((D3DXHANDLE)zSize,cloud_tex_depth_z);
		//RenderTexture(m_pd3dDevice,0,0,cloud_tex_width_x,cloud_tex_length_y*cloud_tex_depth_z,NULL,m_pGPULightingEffect,m_hTechniqueGpuTransformLightgrid);
		DrawFullScreenQuad(m_pd3dDevice,m_pGPULightingEffect);
std::cout<<"\t\tRenderTexture "<<timer.UpdateTime()<<std::endl;
		// Copy the texture to an offscreen surface:
		if(!gpuLighting2DSurface)
			hr=m_pd3dDevice->CreateOffscreenPlainSurface(cloud_tex_width_x,cloud_tex_length_y*cloud_tex_depth_z,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&gpuLighting2DSurface,NULL);
		std::cout<<"\t\tCreateOffscreenPlainSurface "<<timer.UpdateTime()<<std::endl;
		hr=m_pd3dDevice->GetRenderTargetData(pFlattenedRenderTarget,gpuLighting2DSurface);
		std::cout<<"\t\tGetRenderTargetData "<<timer.UpdateTime()<<std::endl;
		gpuLighting2DSurface->LockRect(&rect,NULL,0);
		unsigned char *source=(unsigned char*)rect.pBits;
		// Copy the light data to the target 3D grid:
		unsigned size2d=4*sizeof(unsigned char)*cloud_tex_width_x*cloud_tex_length_y*cloud_tex_depth_z;
		memcpy(target_grid,source,size2d);
		//target_grid+=size2d;
		gpuLighting2DSurface->UnlockRect();
std::cout<<"\t\tmemcpy "<<timer.UpdateTime()<<std::endl;
	}
	m_pd3dDevice->SetRenderTarget(0,pOldRenderTarget);
	m_pd3dDevice->EndScene();

	SAFE_RELEASE(pOldRenderTarget);
std::cout<<"\t\tend "<<timer.UpdateTime()<<std::endl;
>>>>>>> master
}