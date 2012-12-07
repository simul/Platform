#pragma once
#include "Simul/Clouds/BaseGpuCloudGenerator.h"
#include "Simul/Platform/DirectX9/Export.h"
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
						,float humidity
						,float time_val
						,int noise_size,int octaves,float persistence
						,const float  *noise_src_ptr);
	void PerformFullGPURelight(float *target,const int *light_gridsizes,const int *density_grid,const float *Matrix4x4LightToDensityTexcoords,const float *lightspace_extinctions_float3);
	void GPUTransferDataToTexture(	unsigned char *target
											,const float *DensityToLightTransform
											,const float *light,const int *light_grid
											,const float *ambient,const int *density_grid);
protected:
	// Things to store for GPU-based lighting
	LPD3DXEFFECT					m_pGPULightingEffect;
	IDirect3DSurface9				*gpuLighting2DSurface;
	LPDIRECT3DDEVICE9				m_pd3dDevice;
	LPDIRECT3DVOLUMETEXTURE9		volume_noise_texture;
	LPDIRECT3DTEXTURE9				flattened_target_texture;
	LPDIRECT3DSURFACE9				pFlattenedRenderTarget;
	LPDIRECT3DVOLUMETEXTURE9		ambient_texture;
	LPDIRECT3DVOLUMETEXTURE9		direct_texture;


	void MakeShader();
	SIMUL_DIRECTX9_EXPORT_CLASS Target
	{
	public:
		Target()
		{
			target_textures[0]=target_textures[1]=NULL;
			pLightRenderTarget[0]=pLightRenderTarget[1]=NULL;
			pBuf=NULL;
		}
		void Release();
		void Swap()
		{
			std::swap(target_textures[0],target_textures[1]);
			std::swap(pLightRenderTarget[0],pLightRenderTarget[1]);
		}
		LPDIRECT3DTEXTURE9			target_textures[2];
		LPDIRECT3DSURFACE9			pLightRenderTarget[2];
		IDirect3DSurface9			*pBuf;
	};
	Target				xy_target;
	Target				yz_target;
	Target				zx_target;
};
}
}