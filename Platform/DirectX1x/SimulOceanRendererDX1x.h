#ifndef SIMUL_OCEAN_RENDERER_DX1X_H
#define SIMUL_OCEAN_RENDERER_DX1X_H

#include "ocean_simulator.h"
#include "Simul/Platform/DirectX1x/MacrosDX1x.h"
#include "Simul/Platform/DirectX1x/Export.h"
#include "Simul/Terrain/BaseSeaRenderer.h"

#pragma warning(push)
#pragma warning(disable:4251)

SIMUL_DIRECTX1x_EXPORT_CLASS SimulOceanRendererDX1x : public simul::terrain::BaseSeaRenderer
{
public:
	SimulOceanRendererDX1x();
	virtual ~SimulOceanRendererDX1x();
	// init & cleanup
	void RestoreDeviceObjects(ID3D11Device* pd3dDevice);
	void InvalidateDeviceObjects();
	void RecompileShaders();
	// Rendering routines
	//! Call this once per frame to set the matrices.
	void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);
	void Render();
	void RenderWireframe(float time);
	void Update(float dt);
	void SetCubemap(ID3D1xShaderResourceView *c);
protected:
	D3DXMATRIX view,proj;
	ID3D11Device*						m_pd3dDevice;
	ID3D1xDeviceContext*				m_pImmediateContext;
	// HLSL shaders
	ID3D11VertexShader* g_pOceanSurfVS;
	ID3D11PixelShader* g_pOceanSurfPS;
	ID3D11PixelShader* g_pWireframePS;
	// State blocks
	ID3D11RasterizerState* g_pRSState_Solid;
	ID3D11RasterizerState* g_pRSState_Wireframe;
	ID3D11DepthStencilState* g_pDSState_Disable;
	ID3D11DepthStencilState* g_pDSState_Enable;
	ID3D11BlendState* g_pBState_Transparent;
	ID3D11BlendState* g_pBState_Solid;
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
	ID3D11ShaderResourceView* g_pSRV_ReflectCube;

	// Samplers
	ID3D11SamplerState* g_pHeightSampler;
	ID3D11SamplerState* g_pGradientSampler;
	ID3D11SamplerState* g_pFresnelSampler;
	ID3D11SamplerState* g_pPerlinSampler;
	ID3D11SamplerState* g_pCubeSampler;
	// create a triangle strip mesh for ocean surface.
	void createSurfaceMesh();
	// create color/fresnel lookup table.
	void createFresnelMap();
	// create perlin noise texture for far-sight rendering
	void loadTextures();
	void EnumeratePatterns(unsigned long* index_array);
};
#pragma warning(pop)
#endif