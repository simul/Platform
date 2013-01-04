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
#include <d3d10effect.h>
#else
#include <d3d11.h>
#include <d3dx11.h>
#include <D3dx11effect.h>
#endif
typedef long HRESULT;
#include <vector>
#include "Simul/Platform/DirectX1x/MacrosDX1x.h"
#include "Simul/Math/float3.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Platform/DirectX1x/Export.h"
namespace simul
{
	namespace sky
	{
		class SkyInterface;
	}
	namespace terrain
	{
		class HeightMapInterface;
		struct Cutout;
	}
}
class SimulTerrainRendererDX1x
{
public:
	SimulTerrainRendererDX1x();
	//standard d3d object interface functions
	HRESULT Create(ID3D1xDevice* pd3dDevice);
	HRESULT RestoreDeviceObjects( ID3D1xDevice* pd3dDevice);
	HRESULT InvalidateDeviceObjects();
	HRESULT Destroy();

	virtual ~SimulTerrainRendererDX1x();
	HRESULT Render();
	void SetMatrices(const D3DXMATRIX &w,const D3DXMATRIX &v,const D3DXMATRIX &p);

protected:
};