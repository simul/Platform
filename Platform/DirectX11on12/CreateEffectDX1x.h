// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// CreateEffect.h Create a DirectX .fx effect and report errors.
#ifndef CREATEEFFECTDX1X_H
#define CREATEEFFECTDX1X_H
#include "SimulDirectXHeader.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx11.h>
#else
#include <DirectXMath.h>
#endif

#include <map>
#include <vector>
#include "MacrosDX1x.h"
#include "Export.h"

struct VertexXyzRgba;
struct ID3DX11Effect;
struct ID3DX11EffectTechnique;
struct ID3DX11EffectPass;

typedef long HRESULT;
namespace simul
{
	namespace crossplatform
	{
		class Effect;
	}
	/// The Simul namespace for DirectX 11 implementations of rendering functions.
	namespace dx11on12
	{
		//! Call this to make the FX compiler put its warnings and errors to the standard output when used.
		extern SIMUL_DIRECTX12_EXPORT void PipeCompilerOutput(bool p);
		extern SIMUL_DIRECTX12_EXPORT ID3D11ShaderResourceView* LoadTexture(ID3D11Device* dev,const char *filename,const std::vector<std::string> &texturePathsUtf8);
		extern SIMUL_DIRECTX12_EXPORT ID3D11Texture2D* LoadStagingTexture(ID3D11Device* dev,const char *filename,const std::vector<std::string> &texturePathsUtf8);
		// These functions encapsulate getting an effect variable of the given name if it exists, and
		// if so, setting its value. Due to inefficiency it is best to replace usage of this over time
		// with Effect variable pointers, but this is a good way to write new render code quickly
		void SIMUL_DIRECTX12_EXPORT setDepthState			(ID3DX11Effect *effect	,const char *name	,ID3D11DepthStencilState * value);
		void SIMUL_DIRECTX12_EXPORT setSamplerState			(ID3DX11Effect *effect	,const char *name	,ID3D11SamplerState * value);
		void SIMUL_DIRECTX12_EXPORT applyPass				(ID3D11DeviceContext *pContext,ID3DX11Effect *effect,const char *name,int pass=0);
		void SIMUL_DIRECTX12_EXPORT applyPass				(ID3D11DeviceContext *pContext,ID3DX11Effect *effect,const char *name,const char *pass);
		bool SIMUL_DIRECTX12_EXPORT setUnorderedAccessView	(ID3DX11Effect *effect	,const char *name	,ID3D11UnorderedAccessView * value);
		void SIMUL_DIRECTX12_EXPORT setTextureArray			(ID3DX11Effect *effect	,const char *name	,ID3D11ShaderResourceView *value);
		void SIMUL_DIRECTX12_EXPORT setStructuredBuffer		(ID3DX11Effect *effect	,const char *name	,ID3D11ShaderResourceView * value);
		void SIMUL_DIRECTX12_EXPORT setParameter			(ID3DX11Effect *effect	,const char *name	,float value);
		void SIMUL_DIRECTX12_EXPORT setParameter			(ID3DX11Effect *effect	,const char *name	,float x,float y);
		void SIMUL_DIRECTX12_EXPORT setParameter			(ID3DX11Effect *effect	,const char *name	,float x,float y,float z,float w);
		void SIMUL_DIRECTX12_EXPORT setParameter			(ID3DX11Effect *effect	,const char *name	,int value);
		void SIMUL_DIRECTX12_EXPORT setParameter			(ID3DX11Effect *effect	,const char *name	,const float *vec);
		void SIMUL_DIRECTX12_EXPORT setMatrix				(ID3DX11Effect *effect	,const char *name	,const float *value);
		void SIMUL_DIRECTX12_EXPORT setConstantBuffer		(ID3DX11Effect *effect	,const char *name	,ID3D11Buffer *b);
							
		int ByteSizeOfFormatElement( DXGI_FORMAT format );
		//! Create an effect from the named .fx file. Depending on what was passed to SetShaderBuildMode(), this may instead simply load the binary .fxo file that corresponds to the given filename.
		extern SIMUL_DIRECTX12_EXPORT HRESULT CreateEffect(ID3D11Device *d3dDevice,ID3DX11Effect **effect,const char *filename,crossplatform::ShaderBuildMode shaderBuildMode,const std::vector<std::string> &shaderPathsUtf8,const std::string &shaderBinPathUtf8);
		//! Create an effect from the named .fx file. Depending on what was passed to SetShaderBuildMode(), this may instead simply load the binary .fxo file that corresponds to the given filename and defines.
		extern SIMUL_DIRECTX12_EXPORT HRESULT CreateEffect(ID3D11Device *d3dDevice,ID3DX11Effect **effect,const char *filename,const std::map<std::string,std::string>&defines,unsigned int shader_flags,crossplatform::ShaderBuildMode shaderBuildMode,const std::vector<std::string> &shaderPathsUtf8,const std::string &shaderBinPathUtf8);
	}
}

#ifndef D3DCOMPILE_SKIP_OPTIMIZATION
#define D3DCOMPILE_SKIP_OPTIMIZATION            (1 << 2)
#endif
#ifndef D3DCOMPILE_OPTIMIZATION_LEVEL3
#define D3DCOMPILE_OPTIMIZATION_LEVEL3            (1 << 15)
#endif
extern SIMUL_DIRECTX12_EXPORT HRESULT ApplyPass(ID3D11DeviceContext *pImmediateContext,ID3DX11EffectPass *pass);

#define PAD16(n) (((n)+15)/16*16)
#endif
