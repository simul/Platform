// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif

typedef long HRESULT;
#include <vector>
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Math/float3.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Clouds/BasePrecipitationRenderer.h"
#include "Simul/Platform/DirectX11/HLSL/CppHLSL.hlsl"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Math/Matrix4x4.h"
typedef long HRESULT;
struct ID3DX11EffectShaderResourceVariable;
namespace simul
{
	namespace dx11
	{
		//! A rain/snow renderer for DirectX 11.
		class PrecipitationRenderer:public simul::clouds::BasePrecipitationRenderer
		{
		public:
			PrecipitationRenderer();
			virtual ~PrecipitationRenderer();
			//standard d3d object interface functions:
			//! Call this when the D3D device has been created or reset.
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			void RecompileShaders();
			//! Call this when the D3D device has been shut down.
			void InvalidateDeviceObjects();
			void PreRenderUpdate(crossplatform::DeviceContext &deviceContext,float time_step_seconds);
			void RenderMoisture(void *context,
				const DepthTextureStruct &depth
				,const crossplatform::ViewStruct &viewStruct
				,const CloudShadowStruct &cloudShadowStruct);
			void Render(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depth_tex
				,float max_fade_distance_metres,simul::sky::float4 viewportTextureRegionXYWH);
			//! Put textures to screen for debugging
			void RenderTextures(crossplatform::DeviceContext &deviceContext,int x0,int y0,int dx,int dy);
			//! Provide a random 3D texture. This is set externally so the texture can be shared.
			void SetRandomTexture3D(void *texture);
			void *GetMoistureTexture();
		protected:
			void RenderParticles(crossplatform::DeviceContext &deviceContext);
			ID3D11Device*								m_pd3dDevice;
			ID3D11InputLayout*							m_pVtxDecl;
			VertexBuffer<PrecipitationVertex>			vertexBuffer;
			VertexBuffer<PrecipitationVertex>			vertexBufferSwap;

			
			VertexBuffer<SplashVertex>					splashBuffer;
			VertexBuffer<SplashVertex>					splashBufferSwap;

			dx11::ArrayTexture							rainArrayTexture;
			dx11::Framebuffer							moisture_fb;
		
			crossplatform::Effect*						effect;
			crossplatform::Texture*					rain_texture;
			ID3D11ShaderResourceView*					randomTexture3D;
			ID3DX11EffectShaderResourceVariable*		rainTexture;
			vec3  *particles;
			
			crossplatform::EffectTechnique*				m_hTechniqueRain;
			crossplatform::EffectTechnique*				m_hTechniqueParticles;
			crossplatform::EffectTechnique*				m_hTechniqueRainParticles;
			crossplatform::EffectTechnique*				techniqueMoveParticles;
			crossplatform::ConstantBuffer<RainConstants>			rainConstants;
			crossplatform::ConstantBuffer<RainPerViewConstants>		perViewConstants;
			crossplatform::ConstantBuffer<MoisturePerViewConstants>	moisturePerViewConstants;
			float intensity;
			math::Vector3 last_cam_pos;
			bool view_initialized;
		};
	}
}