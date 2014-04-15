// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.
#pragma once
#include <d3dx9.h>
#include <map>
#include <string>
#include "Simul/Platform/DirectX9/Export.h"
#include "Simul/Clouds/BaseCloudRenderer.h"
#include "Simul/Base/RuntimeError.h"
namespace simul
{
	namespace dx9
	{
		struct SIMUL_DIRECTX9_EXPORT TextureStruct
		{
			TextureStruct();
			~TextureStruct();
			void release();
			LPDIRECT3DBASETEXTURE9	texture;
			int width,length,depth;
			D3DFORMAT format;
			//void copyToMemory(ID3D11Device *pd3dDevice,ID3D11DeviceContext *context,void *target,int start_texel=0,int texels=0);
			void setTexels(const void *src,int texel_index,int num_texels);
			//void init(ID3D11Device *pd3dDevice,int w,int l,DXGI_FORMAT f);
			void ensureTexture3DSizeAndFormat(IDirect3DDevice9 *pd3dDevice,int w,int l,int d,D3DFORMAT f);
			//void ensureTexture2DSizeAndFormat(ID3D11Device *pd3dDevice,int w,int l,DXGI_FORMAT f,bool computable=false,bool rendertarget=false);
			//void ensureTexture1DSizeAndFormat(ID3D11Device *pd3dDevice,int w,DXGI_FORMAT f,bool computable=false);
		};
	}
}

namespace std
{
	template<> inline void swap(simul::dx9::TextureStruct& _Left, simul::dx9::TextureStruct& _Right)
	{
		std::swap(_Left.texture				,_Right.texture);
		std::swap(_Left.width				,_Right.width);
		std::swap(_Left.length				,_Right.length);
		std::swap(_Left.depth				,_Right.depth);
		std::swap(_Left.format				,_Right.format);
	}
}