// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include <tchar.h>
#include <d3dx9.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <D3dx11effect.h>
#include "Simul/Math/Matrix4x4.h"
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
			void SetMatrices(const simul::math::Matrix4x4 &view,const simul::math::Matrix4x4 &proj);
			//! Render the Atmospherics.
			void RenderAsOverlay(void *context,const void *depthTexture,float exposure,const simul::sky::float4& relativeViewportTextureRegionXYWH);
			virtual void RenderLoss(void *context,const void *depthTexture,const simul::sky::float4& relativeViewportTextureRegionXYWH,bool near_pass);
			virtual void RenderInscatter(void *context,const void *depthTexture,float exposure,const simul::sky::float4& relativeViewportTextureRegionXYWH,bool near_pass);
			void RenderGodrays(void *context,float strength,bool near_pass,const void *depthTexture,float exposure,const simul::sky::float4& relativeViewportTextureRegionXYWH,const void *cloudDepthTexture);
		protected:
			HRESULT Destroy();
			ID3D11Device*								m_pd3dDevice;
			simul::math::Matrix4x4						view,proj;

			//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
			ID3DX11Effect*								effect;

			ID3DX11EffectTechnique*						twoPassOverlayTechnique;
			ID3DX11EffectTechnique*						twoPassOverlayTechniqueMSAA;

			ID3DX11EffectTechnique*						godraysTechnique;
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

			ID3D11ShaderResourceView*					skyLossTexture_SRV;
			ID3D11ShaderResourceView*					skyInscatterTexture_SRV;
			ID3D11ShaderResourceView*					overcInscTexture_SRV;
			ID3D11ShaderResourceView*					skylightTexture_SRV;

			ID3D11ShaderResourceView*					illuminationTexture_SRV;
			ID3D11ShaderResourceView*				rainbowLookupTexture;
			ID3D11ShaderResourceView*				coronaLookupTexture;

			ConstantBuffer<AtmosphericsPerViewConstants>	atmosphericsPerViewConstants;
			ConstantBuffer<AtmosphericsUniforms>			atmosphericsUniforms;
		};
	}
}