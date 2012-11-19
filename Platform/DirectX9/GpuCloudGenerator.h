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
	void SetGPULightingParameters(const float *Matrix4x4LightToDensityTexcoords
									,const unsigned *light_grid
									,const float *lightspace_extinctions_float3
									,int num_octaves
									,float persistence_val
									,float humidity_val
									,float time_val,int w,int l,int d
									,int size,const float *src_ptr);
	void PerformFullGPURelight(float *target_direct_grid);
	void GPUTransferDataToTexture(	unsigned char *target_texture
									,const float *light_grid
									,const float *ambient_grid);
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