#ifndef SIMUL_OCEAN_RENDERER_DX1X_H
#define SIMUL_OCEAN_RENDERER_DX1X_H


#include "OceanSimulator.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Terrain/BaseSeaRenderer.h"

#pragma warning(push)
#pragma warning(disable:4251)
namespace simul
{
	namespace dx11
	{
		SIMUL_DIRECTX11_EXPORT_CLASS OceanRenderer : public simul::terrain::BaseSeaRenderer
		{
		public:
			OceanRenderer(simul::terrain::SeaKeyframer *s);
			virtual ~OceanRenderer();
			// init & cleanup
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			// Rendering routines
			//! Call this once per frame to set the matrices.
			void SetMatrices(const float *v,const float *p);
			void Render(crossplatform::DeviceContext &context,float exposure);
			void RenderWireframe(crossplatform::DeviceContext &deviceContext);
			void Update(float dt);
			void SetCubemapTexture(crossplatform::Texture *c);
			void SetLossAndInscatterTextures(crossplatform::Texture *l,crossplatform::Texture *i,crossplatform::Texture *s);
			void RenderTextures(crossplatform::DeviceContext &deviceContext,int width,int depth);
		protected:
			OceanSimulator						*oceanSimulator;
			ID3D11Device						*m_pd3dDevice;
			// HLSL shaders
			crossplatform::Effect				*effect;
			// State blocks
			ID3D11Buffer* g_pPerCallCB;
			ID3D11Buffer* g_pPerFrameCB;
			ID3D11Buffer* g_pShadingCB;

			// D3D11 buffers and layout
			ID3D11Buffer* g_pMeshVB;
			ID3D11Buffer* g_pMeshIB;
			ID3D11InputLayout* g_pMeshLayout;

			// Color look up 1D texture
			ID3D11Texture1D* g_pFresnelMap;
			ID3D11ShaderResourceView* g_pSRV_Fresnel;

			// Distant perlin wave
			ID3D11ShaderResourceView* g_pSRV_Perlin;

			// Environment maps
			crossplatform::Texture *cubemapTexture;
			// Atmospheric scattering
			ID3D11ShaderResourceView* skyLossTexture_SRV;
			ID3D11ShaderResourceView* skyInscatterTexture_SRV;
			ID3D11ShaderResourceView* skylightTexture_SRV;
			crossplatform::ConstantBuffer<cbShading>		shadingConstants;
			crossplatform::ConstantBuffer<cbChangePerCall>	changePerCallConstants;
	
			// create a triangle strip mesh for ocean surface.
			void createSurfaceMesh();
			// create color/fresnel lookup table.
			void createFresnelMap();
			// create perlin noise texture for far-sight rendering
			void loadTextures();
			void EnumeratePatterns(unsigned long* index_array);
		};
	}
}
#pragma warning(pop)
#endif