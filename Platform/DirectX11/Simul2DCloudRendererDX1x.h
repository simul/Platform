// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// Simul2DCloudRendererDX11.h A renderer for 2D cloud layers.

#pragma once
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif

#include "Simul/Clouds/Base2DCloudRenderer.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/HLSL/CppHlsl.hlsl"

namespace simul
{
	namespace dx11
	{
		//! A renderer for 2D cloud layers, e.g. cirrus clouds.
		class Simul2DCloudRendererDX11: public simul::clouds::Base2DCloudRenderer
		{
		public:
			Simul2DCloudRendererDX11(simul::clouds::CloudKeyframer *ck2d,simul::base::MemoryInterface *mem);
			virtual ~Simul2DCloudRendererDX11();
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			void RecompileShaders();
			void InvalidateDeviceObjects();
			void SetMatrices(const simul::math::Matrix4x4 &v,const simul::math::Matrix4x4 &p);
			void PreRenderUpdate(crossplatform::DeviceContext &deviceContext);
			bool Render(crossplatform::DeviceContext &deviceContext,float exposure,bool cubemap,bool near_pass
				,crossplatform::Texture *depth_tex,bool write_alpha
				,const simul::sky::float4& viewportTextureRegionXYWH
				,const simul::sky::float4& );
			void RenderCrossSections(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height);
			void RenderAuxiliaryTextures(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height);
			void SetLossTexture(void *l);
			void SetInscatterTextures(void* i,void *s,void *o);
			void SetIlluminationTexture(void *i);
			void SetLightTableTexture(void *l);
			void SetWindVelocity(float x,float y);
		protected:
			void RenderDetailTexture(crossplatform::DeviceContext &deviceContext);
			void EnsureCorrectTextureSizes();
			virtual void EnsureTexturesAreUpToDate(crossplatform::DeviceContext &deviceContext);
			void EnsureTextureCycle();
			void EnsureCorrectIlluminationTextureSizes(){}
			void EnsureIlluminationTexturesAreUpToDate(){}
			void CreateNoiseTexture(crossplatform::DeviceContext &deviceContext){}
			simul::math::Matrix4x4		view,proj;
			ID3D11Device*				m_pd3dDevice;
			ID3DX11Effect*				effect;
			ID3DX11EffectTechnique*		msaaTechnique;
			ID3DX11EffectTechnique*		technique;
			ID3D11Buffer*				vertexBuffer;
			ID3D11Buffer*				indexBuffer;
			ID3D11InputLayout*			inputLayout;
			
			ConstantBuffer<Cloud2DConstants>	cloud2DConstants;
			ConstantBuffer<Detail2DConstants>	detail2DConstants;
			int num_indices;
			
			ID3D11ShaderResourceView*	skyLossTexture_SRV;
			ID3D11ShaderResourceView*	skyInscatterTexture_SRV;
			ID3D11ShaderResourceView*	skylightTexture_SRV;
			ID3D11ShaderResourceView*	illuminationTexture_SRV;
			ID3D11ShaderResourceView*	lightTableTexture_SRV;

			simul::dx11::Framebuffer	coverage_fb;
			simul::dx11::Framebuffer	detail_fb;
			simul::dx11::Framebuffer	noise_fb;
			simul::dx11::Framebuffer	dens_fb;
		};
	}
}