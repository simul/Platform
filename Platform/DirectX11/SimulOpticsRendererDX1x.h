#pragma once

#include "Simul/Camera/BaseOpticsRenderer.h"
#include "Simul/Camera/LensFlare.h"
#include "Simul/Sky/Float4.h"
#include <d3dx9.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <D3dx11effect.h>
#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include <vector>
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace dx11
	{
		SIMUL_DIRECTX11_EXPORT_CLASS SimulOpticsRendererDX1x:public simul::camera::BaseOpticsRenderer
		{
		public:
			SimulOpticsRendererDX1x();
			virtual ~SimulOpticsRendererDX1x();
			virtual void RestoreDeviceObjects(void *device);
			virtual void InvalidateDeviceObjects();
			virtual void RenderFlare(void *context,float exposure,const float *dir,const float *light);
			virtual void RecompileShaders();
			void SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p);
		protected:
			D3DXMATRIX								view,proj;
			ID3D1xDevice *							m_pd3dDevice;
			ID3D1xEffect*							m_pFlareEffect;
			ID3D1xEffectTechnique*					m_hTechniqueFlare;
			ID3D1xEffectMatrixVariable*				worldViewProj;
			ID3D1xEffectVectorVariable*				colour;
			ID3D1xEffectShaderResourceVariable*		flareTexture;
			ID3D1xShaderResourceView*				flare_texture;
			std::vector<ID3D1xShaderResourceView*>	halo_textures;
		protected:
			std::string								FlareTexture;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif

