// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.
#pragma once
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif

typedef long HRESULT;
#include "Simul/Base/Referenced.h"
#include "Simul/Terrain/BaseTerrainRenderer.h"
#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Base/Referenced.h"
#include "Simul/Base/PropertyMacros.h"
#include "Simul/Platform/DirectX11/HLSL/CppHLSL.hlsl"
#include "Simul/Platform/CrossPlatform/SL/simul_terrain_constants.sl"
#include "Simul/Platform/CrossPlatform/SL/simul_cloud_constants.sl"

namespace simul
{
	namespace dx11
	{
		SIMUL_DIRECTX11_EXPORT_CLASS TerrainRenderer : public simul::terrain::BaseTerrainRenderer
		{
		public:
			TerrainRenderer(simul::base::MemoryInterface *m);
			~TerrainRenderer();
			void ReloadTextures();
			void RecompileShaders();
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			void InvalidateDeviceObjects();
			void Render(crossplatform::DeviceContext &deviceContext,float exposure);
			/// Test render function for alpha-to-coverage
			void Test(crossplatform::DeviceContext &deviceContext);
		private:
			void MakeVertexBuffer();
			ID3D11Device*						m_pd3dDevice;
			ID3D11Buffer*						m_pVertexBuffer;
			ID3D11InputLayout*					m_pVtxDecl;
			crossplatform::EffectTechnique*		m_pTechnique;
			// ID3D11Texture2D	Accesses data in a 2D texture or a 2D texture array
			simul::dx11::ArrayTexture			arrayTexture;
			crossplatform::ConstantBuffer<TerrainConstants>	terrainConstants;
			int numVertices;
		};
	}
}