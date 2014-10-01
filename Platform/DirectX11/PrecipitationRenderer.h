// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include <vector>
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Math/float3.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Clouds/BasePrecipitationRenderer.h"
#include "Simul/Platform/DirectX11/HLSL/CppHLSL.hlsl"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Math/Matrix4x4.h"
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
		};
	}
}