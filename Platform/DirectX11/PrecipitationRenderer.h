// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include <d3dx9.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <D3dx11effect.h>
typedef long HRESULT;
#include <vector>
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Math/float3.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Clouds/BasePrecipitationRenderer.h"
#include "Simul/Platform/DirectX11/HLSL/CppHLSL.hlsl"
#include "Simul/Platform/CrossPlatform/CppSl.hs"
#include "Simul/Platform/CrossPlatform/rain_constants.sl"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Math/Matrix4x4.h"
typedef long HRESULT;

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
			void RestoreDeviceObjects(void* dev);
			void RecompileShaders();
			void SetCubemapTexture(void *);
			//! Call this when the D3D device has been shut down.
			void InvalidateDeviceObjects();
			void SetMatrices(const simul::math::Matrix4x4 &v,const simul::math::Matrix4x4 &p);
			void PreRenderUpdate(void *context,float time_step_seconds);
			void Render(void *context,void *depth_tex,float max_fade_distance_metres,simul::sky::float4 viewportTextureRegionXYWH);
			//! Put textures to screen for debugging
			void RenderTextures(void *context,int width,int height);
			//! Provide a random 3D texture. This is set externally so the texture can be shared.
			void SetRandomTexture3D(void *text);
		protected:
			void RenderParticles(void *context);
			ID3D11Device*							m_pd3dDevice;
			ID3D11InputLayout*						m_pVtxDecl;
			VertexBuffer<PrecipitationVertex>		vertexBuffer;
			VertexBuffer<PrecipitationVertex>		vertexBufferSwap;
			simul::dx11::ArrayTexture				rainArrayTexture;
			//ID3D11Buffer*							m_pVertexBufferSwap;
			ID3DX11Effect*							effect;					// The fx file for this renderer
			ID3D11ShaderResourceView*				rain_texture;
			ID3D11ShaderResourceView*				randomTexture3D;
			ID3D11ShaderResourceView*				cubemap_SRV;
			ID3D1xEffectShaderResourceVariable*		rainTexture;
			vec3  *particles;
			
			ID3DX11EffectTechnique*					m_hTechniqueRain;
			ID3DX11EffectTechnique*					m_hTechniqueParticles;
			ID3DX11EffectTechnique*					m_hTechniqueRainParticles;
			ID3DX11EffectTechnique*					techniqueMoveParticles;
			simul::math::Matrix4x4								view,proj;
			ConstantBuffer<RainConstants>			rainConstants;
			ConstantBuffer<RainPerViewConstants>	perViewConstants;
			float intensity;

			bool view_initialized;
		};
	}
}