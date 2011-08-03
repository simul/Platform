#pragma once

#include "Simul/Camera/BaseOpticsRenderer.h"
#include "Simul/Camera/LensFlare.h"
#include "Simul/Sky/Float4.h"
#include <d3dx9.h>
#ifdef DX10
#include <d3d10.h>
#include <d3dx10.h>
#include <d3d10effect.h>
#else
#include <d3d11.h>
#include <d3dx11.h>
#include <D3dx11effect.h>
#endif
#include "Simul/Platform/DirectX1x/MacrosDx1x.h"
#include "Simul/Platform/DirectX1x/Export.h"
#include <vector>
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif


SIMUL_DIRECTX1x_EXPORT_CLASS SimulOpticsRendererDX1x:public simul::camera::BaseOpticsRenderer
{
public:
	SimulOpticsRendererDX1x();
	virtual ~SimulOpticsRendererDX1x();
	virtual void RestoreDeviceObjects(void *device);
	virtual void InvalidateDeviceObjects();
	virtual void RenderFlare(float exposure,const float *dir,const float *light);
	virtual void ReloadShaders();
	void SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p);
protected:
	D3DXMATRIX								view,proj;
	ID3D1xDevice *							m_pd3dDevice;
	ID3D1xDeviceContext *					m_pImmediateContext;
	ID3D1xEffect*							m_pFlareEffect;
	ID3D1xEffectTechnique*					m_hTechniqueFlare;
	ID3D1xEffectMatrixVariable*				worldViewProj;
	ID3D1xEffectVectorVariable*				colour;
	ID3D1xEffectShaderResourceVariable*		flareTexture;
	ID3D1xShaderResourceView*				flare_texture;
	std::vector<ID3D1xShaderResourceView*>	halo_textures;
protected:
	tstring									FlareTexture;
};
#ifdef _MSC_VER
	#pragma warning(pop)
#endif

