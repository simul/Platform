// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Clouds/BaseLightningRenderer.h"
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
#include "Simul/Platform/DirectX9/Export.h"
typedef long HRESULT;
class SimulLightningRenderer: public simul::clouds::BaseLightningRenderer
{
public:
	SimulLightningRenderer(simul::clouds::LightningRenderInterface *lri);
	~SimulLightningRenderer();
	bool RestoreDeviceObjects(void *pd3dDevice);
	bool Render();
	bool InvalidateDeviceObjects();
	void SetYVertical(bool y)
	{
		y_vertical=y;
	}
	bool IsYVertical() const{return y_vertical;}
	void RecompileShaders();
protected:
	bool y_vertical;
	struct float2
	{
		float x,y;
		void operator=(const float*f)
		{
			x=f[0];
			y=f[1];
		}
	};
	struct float3
	{
		float x,y,z;
		void operator=(const float*f)
		{
			x=f[0];
			y=f[1];
			z=f[2];
		}
	};
	struct PosTexVert_t
	{
		float3 position;	
		float2 texCoords;
	};
	PosTexVert_t *lightning_vertices;

	LPDIRECT3DDEVICE9				m_pd3dDevice;
	LPDIRECT3DVERTEXDECLARATION9	m_pLightningVtxDecl;
	LPD3DXEFFECT					m_pLightningEffect;
	D3DXHANDLE						m_hTechniqueLightningLines;
	D3DXHANDLE						m_hTechniqueLightningQuads;
	D3DXHANDLE						l_worldViewProj;
	D3DXHANDLE						lightningColour;
	LPDIRECT3DTEXTURE9				lightning_texture;
	D3DXMATRIX						world,view,proj;

	HRESULT CreateLightningTexture();
};