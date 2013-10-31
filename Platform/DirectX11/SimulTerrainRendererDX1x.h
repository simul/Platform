// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.
#pragma once
#include <d3dx9.h>
	#include <d3d11.h>
	#include <d3dx11.h>
	#include <d3dx11effect.h>
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
#include "Simul/Platform/CrossPlatform/simul_terrain_constants.sl"
#include "Simul/Platform/CrossPlatform/simul_cloud_constants.sl"

namespace simul
{
	namespace dx11
	{
		SIMUL_DIRECTX11_EXPORT_CLASS SimulTerrainRendererDX1x : public simul::terrain::BaseTerrainRenderer
		{
		public:
			SimulTerrainRendererDX1x(simul::base::MemoryInterface *m);
			~SimulTerrainRendererDX1x();
			void ReloadTextures();
			void RecompileShaders();
			void RestoreDeviceObjects(void*);
			void InvalidateDeviceObjects();
			void Render(void *context,float exposure);
			//! Call this once per frame to set the matrices.
			void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);
		private:
			ID3D11Device*						m_pd3dDevice;
			ID3D11Buffer*						m_pVertexBuffer;
			ID3D11InputLayout*					m_pVtxDecl;
			ID3DX11Effect*						m_pTerrainEffect;
			ID3DX11EffectTechnique*				m_pTechnique;
			// ID3D11Texture2D	Accesses data in a 2D texture or a 2D texture array
			simul::dx11::ArrayTexture			arrayTexture;
			D3DXMATRIX							view,proj;

			ConstantBuffer<TerrainConstants>	terrainConstants;
		};
	}
}