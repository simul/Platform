#pragma once
#include "Simul/Clouds/BaseGpuCloudGenerator.h"
#include "Simul/Platform/DirectX9/Export.h"
#include "Simul/Platform/DirectX9/Framebuffer.h"
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
//! A DX9 class to generate clouds on the GPU
namespace simul
{
namespace dx9
{
SIMUL_DIRECTX9_EXPORT_CLASS GpuCloudGenerator:public simul::clouds::BaseGpuCloudGenerator
{
public:
	GpuCloudGenerator();
	~GpuCloudGenerator();
	void CreateVolumeNoiseTexture(int size,const float *src_ptr);
	void RestoreDeviceObjects(void *dev);
	void InvalidateDeviceObjects();
	void RecompileShaders();
	// implementing GpuLightingCallback:
	bool CanPerformGPULighting() const;			
	int GetDensityGridsize(const int *grid);
	void* FillDensityGrid(const int *grid
							,int start_texel
							,int texels
							,float humidity
									,float baseLayer
									,float transition
							,float time_val
							,int noise_size,int octaves,float persistence
							,const float  *noise_src_ptr);
	
	void PerformGPURelight(float *target
							,const int *light_gridsizes
							,int start_texel
							,int texels
							,const int *density_grid
							,const float *Matrix4x4LightToDensityTexcoords
							,const float *lightspace_extinctions_float3);
	void GPUTransferDataToTexture(	unsigned char *target
									,const float *DensityToLightTransform
									,const float *light,const int *light_grid
									,const float *ambient,const int *density_grid
									,int start_texel
									,int texels);
protected:
	Framebuffer	fb[2];
	Framebuffer	world_fb;
	Framebuffer	dens_fb;
	D3DFORMAT iformat;
	// Things to store for GPU-based lighting
	LPD3DXEFFECT					m_pGPULightingEffect;
	IDirect3DSurface9				*gpuLighting2DSurface;
	LPDIRECT3DDEVICE9				m_pd3dDevice;
	LPDIRECT3DVOLUMETEXTURE9		volume_noise_texture;
	LPDIRECT3DVOLUMETEXTURE9		ambient_texture;
	LPDIRECT3DVOLUMETEXTURE9		direct_texture;
	LPDIRECT3DVOLUMETEXTURE9	density_texture;

	void MakeShader();
	void EnsureVolumeTexture(LPDIRECT3DVOLUMETEXTURE9 &tex,const int *grid,const float *src);
	SIMUL_DIRECTX9_EXPORT_CLASS Target
	{
	public:
		Target()
			:bytes_per_texel(0)
			,width(0)
			,height(0)
		{
			target_textures[0]=target_textures[1]=NULL;
			pLightRenderTarget[0]=pLightRenderTarget[1]=NULL;
			pBuf=NULL;
		}
		void Release();
		void Init(LPDIRECT3DDEVICE9 m_pd3dDevice,int w,int h);
		void Swap()
		{
			std::swap(target_textures[0],target_textures[1]);
			std::swap(pLightRenderTarget[0],pLightRenderTarget[1]);
		}
		// return the number of bytes copied.
		int Copy(int index,unsigned char *destination);
		LPDIRECT3DTEXTURE9			target_textures[2];
		LPDIRECT3DSURFACE9			pLightRenderTarget[2];
		IDirect3DSurface9			*pBuf;
		LPDIRECT3DDEVICE9	m_pd3dDevice;
		int					bytes_per_texel;
		int					width,height;
	};
	Target				xy_target;
	Target				yz_target;
	Target				zx_target;
};
}
}