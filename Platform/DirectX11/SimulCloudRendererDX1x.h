// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// SimulCloudRendererDX1x.h A DirectX 10/11 renderer for clouds. Create an instance of this class in a dx10 program
// and use the Render() function once per frame.

#pragma once

#include <tchar.h>
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif

#include "Simul/Graph/Meta/Group.h"

#include "Simul/Clouds/BaseCloudRenderer.h"
#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/GpuCloudGenerator.h"
struct ID3DX11EffectMatrixVariable;
struct ID3DX11EffectShaderResourceVariable;
namespace simul
{
	namespace crossplatform
	{
		struct DeviceContext;
	}
	namespace clouds
	{
		class CloudInterface;
		class LightningRenderInterface;
		class CloudGeometryHelper;
	}
	namespace sky
	{
		class BaseSkyInterface;
		class OvercastCallback;
	}
}
typedef long HRESULT;
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
	}
	namespace dx11
	{
	//! A cloud rendering class. Create an instance of this class within a DirectX program.
		SIMUL_DIRECTX11_EXPORT_CLASS SimulCloudRendererDX1x : public simul::clouds::BaseCloudRenderer
		{
		public:
			SimulCloudRendererDX1x(simul::clouds::CloudKeyframer *cloudKeyframer,simul::base::MemoryInterface *mem);
			virtual ~SimulCloudRendererDX1x();
			//! Call this when the D3D device has been created or reset
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			//! Call this when the 3D device has been lost.
			void InvalidateDeviceObjects();
			void PreRenderUpdate(crossplatform::DeviceContext &deviceContext);
			//! Call this to draw the clouds, including any illumination by lightning.
			bool Render(crossplatform::DeviceContext &deviceContext,float exposure
				,bool cubemap,crossplatform::NearFarPass nearFarPass,crossplatform::Texture *depth_tex,bool write_alpha,const simul::sky::float4& viewportTextureRegionXYWH,const simul::sky::float4& mixedResTransformXYWH);
			void RenderAuxiliaryTextures(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height);
			void RenderTestXXX(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height);
			void RenderCrossSections(crossplatform::DeviceContext &,int x0,int y0,int width,int height);
			//! Return true if the camera is above the cloudbase altitude.
			bool IsCameraAboveCloudBase() const;
			float GetTiming() const;
			crossplatform::Texture *GetRandomTexture3D();
			simul::clouds::BaseGpuCloudGenerator *GetBaseGpuCloudGenerator(){return &gpuCloudGenerator;}

			void CycleTexturesForward(){}

			void FillIlluminationSequentially(int source_index,int texel_index,int num_texels,const unsigned char *uchar8_array);
			void FillIlluminationBlock(int,int,int,int,int,int,int,const unsigned char *){}

			// Save and load a sky sequence
			std::ostream &Save(std::ostream &os) const;
			std::istream &Load(std::istream &is) const;
			//! Clear the sequence()
			void New();
			simul::dx11::GpuCloudGenerator *GetGpuCloudGenerator(){return &gpuCloudGenerator;}
		protected:
			void Recompile();
			simul::dx11::GpuCloudGenerator gpuCloudGenerator;
			void RenderCombinedCloudTexture(crossplatform::DeviceContext &deviceContext);
			// Make up to date with respect to keyframer:
			void EnsureCorrectTextureSizes();
			void EnsureTexturesAreUpToDate(crossplatform::DeviceContext &deviceContext);

			unsigned texel_index[4];
			bool lightning_active;
			ID3D11Device*							m_pd3dDevice;
			
			D3D1x_MAPPED_TEXTURE3D					mapped_illumination;

			ID3D11Texture2D*						cloud_cubemap;

			bool UpdateIlluminationTexture(float dt);
			float LookupLargeScaleTexture(float x,float y);

			bool CreateCloudEffect();
			
		};
	}
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif