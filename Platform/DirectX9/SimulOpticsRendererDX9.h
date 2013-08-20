#pragma once

#include "Simul/Camera/BaseOpticsRenderer.h"
#include "Simul/Camera/LensFlare.h"
#include "Simul/Sky/Float4.h"
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
#include "Simul/Platform/DirectX9/Export.h"
#include <vector>
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
SIMUL_DIRECTX9_EXPORT_CLASS SimulOpticsRendererDX9:public simul::camera::BaseOpticsRenderer
{
public:
	SimulOpticsRendererDX9();
	virtual ~SimulOpticsRendererDX9();
	virtual void RestoreDeviceObjects(void *device);
	virtual void InvalidateDeviceObjects();
	virtual void RenderFlare(void *context,float exposure,const float *dir,const float *light);
	virtual void RecompileShaders();
	void SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p);
	void SetFlare(LPDIRECT3DTEXTURE9 tex,float rad);
protected:
	D3DXMATRIX						view,proj;
	LPDIRECT3DDEVICE9				m_pd3dDevice;
	LPD3DXEFFECT					m_pFlareEffect;
	D3DXHANDLE						m_hTechniqueFlare;
	D3DXHANDLE						worldViewProj;
	D3DXHANDLE						colour;
	D3DXHANDLE						flareTexture;
	bool							external_flare_texture;
	LPDIRECT3DTEXTURE9				flare_texture;
	std::vector<LPDIRECT3DTEXTURE9> halo_textures;
	std::string						FlareTexture;
};

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
