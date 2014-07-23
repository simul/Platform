// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// Simul2DCloudRendererDX11.h A renderer for 2D cloud layers.

#pragma once
#include "SimulDirectXHeader.h"

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
			bool Render(crossplatform::DeviceContext &deviceContext
							,float exposure
							,bool cubemap
							,crossplatform::NearFarPass nearFarPass
							,crossplatform::Texture *depth_tex
							,bool write_alpha
							,const simul::sky::float4& viewportTextureRegionXYWH
							,const simul::sky::float4& );
		};
	}
}