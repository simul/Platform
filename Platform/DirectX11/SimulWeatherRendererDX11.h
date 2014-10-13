// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include <tchar.h>
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Clouds/BaseWeatherRenderer.h"

#ifndef RENDERDEPTHBUFFERCALLBACK
#define RENDERDEPTHBUFFERCALLBACK
class RenderDepthBufferCallback
{
public:
	virtual void Render()=0;
};
#endif
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#endif
namespace simul
{
	//! The namespace for the DirectX 11 platform library and its rendering classes.
	namespace dx11
	{
		/// An implementation of \link simul::clouds::BaseWeatherRenderer BaseWeatherRenderer\endlink for DirectX 11
		SIMUL_DIRECTX11_EXPORT_CLASS SimulWeatherRendererDX11 : public simul::clouds::BaseWeatherRenderer
		{
		public:
			SimulWeatherRendererDX11(simul::clouds::Environment *env,simul::base::MemoryInterface *mem);
			virtual ~SimulWeatherRendererDX11();
	
			void SaveCubemapToFile(crossplatform::RenderPlatform *renderPlatform,const char *filename,float exposure,float gamma);

		};
	}
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
