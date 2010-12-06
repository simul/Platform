// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// CreateEffect.h Create a DirectX .fx effect and report errors.

#ifdef XBOX
	#include <xgraphics.h>
#else
	#include <d3dx11.h>
	#include <D3dx11effect.h>
#endif
extern HRESULT CreateEffect(ID3D11Device *d3dDevice,ID3DX11Effect **effect,const TCHAR *filename,int num_defines=0,...);