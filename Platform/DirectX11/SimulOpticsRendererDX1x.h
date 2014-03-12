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
#include "Simul/Platform/DirectX11/Utilities.h"
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
			SimulOpticsRendererDX1x(simul::base::MemoryInterface *m);
			virtual ~SimulOpticsRendererDX1x();
			virtual void RestoreDeviceObjects(void *device);
			virtual void InvalidateDeviceObjects();
			virtual void RenderFlare(void *context,float exposure,void *moistureTexture,const float *v,const float *p,const float *dir,const float *light);
			virtual void RenderRainbowAndCorona(void *context,float exposure,void *depthTexture,const float *v,const float *p,const float *dir_to_sun,const float *light);
			virtual void RecompileShaders();
		protected:
			ID3D11Device *							m_pd3dDevice;
			ID3DX11Effect*							effect;
			ID3DX11EffectTechnique*					m_hTechniqueFlare;
			ID3DX11EffectTechnique*					techniqueRainbowCorona;
			ID3D1xShaderResourceView*				flare_texture;
			std::vector<ID3D1xShaderResourceView*>	halo_textures;
			ID3D1xShaderResourceView*				rainbowLookupTexture;
			ID3D1xShaderResourceView*				coronaLookupTexture;
			//ID3D1xShaderResourceView*				moistureTexture;
		protected:
			std::string								FlareTexture;
			ConstantBuffer<OpticsConstants>			opticsConstants;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif

