// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include <tchar.h>
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
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Sky/BaseAtmosphericsRenderer.h"
typedef long HRESULT;
namespace simul
{
	namespace sky
	{
		class AtmosphericScatteringInterface;
	}
}

namespace simul
{
	namespace dx11
	{
		// A class that takes an image buffer and a z-buffer and applies atmospheric fading.
		class SimulAtmosphericsRendererDX1x : public simul::sky::BaseAtmosphericsRenderer
		{
		public:
			SimulAtmosphericsRendererDX1x(simul::base::MemoryInterface *m);
			virtual ~SimulAtmosphericsRendererDX1x();
			
			void SetBufferSize(int w,int h);

			// BaseAtmosphericsRenderer.
			void SetLossTexture(void* t);
			void SetInscatterTextures(void* i,void *s,void *o);
			void SetIlluminationTexture(void *i);
			void SetCloudsTexture(void* t);
			void RecompileShaders();

			//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
			void RestoreDeviceObjects(void* pd3dDevice);
			//! Call this when the device has been lost.
			void InvalidateDeviceObjects();
			void SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p);
			//! Render the Atmospherics.
			void RenderAsOverlay(void *context,const void *depthTexture,float exposure,const simul::sky::float4& relativeViewportTextureRegionXYWH);
			void RenderGodrays(void *context,float strength,bool near_pass,const void *depthTexture,float exposure,const simul::sky::float4& relativeViewportTextureRegionXYWH,const void *cloudDepthTexture);
		protected:
			HRESULT Destroy();
			ID3D1xDevice*								m_pd3dDevice;
			D3DXMATRIX									view,proj;

			//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
			ID3D1xEffect*								effect;
			ID3D1xEffectTechnique*						singlePassTechnique;
			ID3D1xEffectTechnique*						twoPassOverlayTechnique;
			ID3D1xEffectTechnique*						godraysTechnique;
			ID3D1xEffectTechnique*							godraysNearPassTechnique;
			// Variables for this effect:
			ID3D1xEffectShaderResourceVariable*			depthTexture;
			ID3D1xEffectShaderResourceVariable*			cloudDepthTexture;
			ID3D1xEffectShaderResourceVariable*			lossTexture;
			ID3D1xEffectShaderResourceVariable*			inscTexture;
			ID3D1xEffectShaderResourceVariable*			skylTexture;
			ID3D1xEffectShaderResourceVariable*			illuminationTexture;
			ID3D1xEffectShaderResourceVariable*			overcTexture;
			ID3D1xEffectShaderResourceVariable*			cloudShadowTexture;
			ID3D1xEffectShaderResourceVariable*				cloudGodraysTexture;

			ID3D1xShaderResourceView*					skyLossTexture_SRV;
			ID3D1xShaderResourceView*					skyInscatterTexture_SRV;
			ID3D1xShaderResourceView*					overcInscTexture_SRV;
			ID3D1xShaderResourceView*					skylightTexture_SRV;

			ID3D1xShaderResourceView*					illuminationTexture_SRV;

			ConstantBuffer<AtmosphericsPerViewConstants>	atmosphericsPerViewConstants;
			ConstantBuffer<AtmosphericsUniforms>			atmosphericsUniforms;
		};
	}
}