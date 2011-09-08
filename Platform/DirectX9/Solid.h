#pragma once
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
#include <comdef.h>
#include "Simul/Platform/DirectX9/Macros.h"
#include "Simul/Sky/Float4.h"
class Plane
{
public:
	Plane();
	~Plane();
	//! Call this when the D3D device has been created or reset.
	void RestoreDeviceObjects(void *pd3dDevice);
	void InvalidateDeviceObjects();
	void Render();
protected:
	LPDIRECT3DDEVICE9				m_pd3dDevice;
	IDirect3DVertexDeclaration9Ptr	m_pVtxDecl;
	LPD3DXEFFECT					m_pEffect;

	D3DXHANDLE						worldViewProj;
	simul::sky::float4 pos;
};
