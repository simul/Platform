// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.
#pragma once
#include <d3dx9.h>
#ifdef DX10
	#include <d3d10.h>
	#include <d3dx10.h>
#else
	#include <d3d11.h>
	#include <d3dx11.h>
	#include <d3dx11effect.h>
#endif
typedef long HRESULT;
#include "Simul/Base/Referenced.h"
#include "Simul/Terrain/BaseTerrainRenderer.h"
#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Base/Referenced.h"
#include "Simul/Base/PropertyMacros.h"

SIMUL_DIRECTX11_EXPORT_CLASS SimulTerrainRendererDX1x : public simul::terrain::BaseTerrainRenderer
{
public:
	SimulTerrainRendererDX1x();
	~SimulTerrainRendererDX1x();
	void RecompileShaders();
	void RestoreDeviceObjects(void*);
	void InvalidateDeviceObjects();
	void Render(void *context);
	//! Call this once per frame to set the matrices.
	void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);
private:
	ID3D1xDevice*						m_pd3dDevice;
	ID3D1xBuffer*						m_pVertexBuffer;
	ID3D1xInputLayout*					m_pVtxDecl;
	ID3D1xEffect*						m_pTerrainEffect;
	ID3D1xEffectTechnique*				m_pTechnique;
	ID3D1xEffectMatrixVariable*			worldViewProj;
	D3DXMATRIX							view,proj;
};
