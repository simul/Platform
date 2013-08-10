// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Clouds/BaseWeatherRenderer.h"
#include "Simul/Platform/DirectX9/SimulLightningRenderer.h"
#include "Simul/Platform/DirectX9/SimulPrecipitationRenderer.h"
#include "Simul/Platform/DirectX9/Export.h"
#include "Simul/Platform/DirectX9/Framebuffer.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

//! A rendering class that encapsulates Simul skies and clouds. Create an instance of this class within a DirectX 9 program.
//! You can take this entire class and use it as source in your project, and 
//! make appropriate modifications where required.

namespace simul
{
	namespace clouds
	{
		class Environment;
	}
}
class SimulSkyRenderer;
class SimulCloudRenderer;
class SimulLightningRenderer;
class Simul2DCloudRenderer;
class SimulPrecipitationRenderer;
class SimulAtmosphericsRenderer;

namespace simul
{
	//! The namespace for the DirectX 9 platform library and its rendering classes.

	//! This library is deprecated in favour of the \ref dx11 DirectX 11 library.
	namespace dx9
	{
		//! A rendering class that encapsulates Simul skies and clouds.
		//! Create an instance of this class within a DirectX 9 program.
		//! You can take this entire class and use it as source in your project.
		//! Make appropriate modifications where required.
SIMUL_DIRECTX9_EXPORT_CLASS SimulWeatherRenderer:public simul::clouds::BaseWeatherRenderer
{
public:
	SimulWeatherRenderer(simul::clouds::Environment *env
							,simul::base::MemoryInterface *mem
							,bool usebuffer=true
							,int width=320,
		int height=240,bool sky=true,
		bool rain=true);
	virtual ~SimulWeatherRenderer();
	void SetScreenSize(int w,int h);
	//standard d3d object interface functions
	bool Create( LPDIRECT3DDEVICE9 pd3dDevice);
	//! Call this when the device has been created
	void RestoreDeviceObjects(void *pd3dDevice);
	//! Call this when the 3D device has been lost.
	void InvalidateDeviceObjects();
	//! Call this to draw the sky and clouds.
	bool RenderSky(void *,float exposure,bool buffer,bool is_cubemap);
	//! Call this to draw the clouds after the main scene.
	void RenderLateCloudLayer(void *context,float exposure,bool buf,int viewport_id,const simul::sky::float4 &relativeViewportTextureRegionXYWH);
	//! Call this to draw lightning.
	void RenderLightning(void *context,int viewport_id);
	//! Call this to draw rain etc.
	void RenderPrecipitation(void *context);
	//! Perform the once-per-frame time update.
	void PreRenderUpdate(void *context,float dt);
#if defined(XBOX) || defined(DOXYGEN)
	//! Call this once per frame to set the matrices (X360 only).
	void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);
#endif
	//! Set the exposure, if we're using an hdr shader to render the sky buffer.
	void SetExposure(float ex){exposure=ex;}
	//! Get a pointer to the sky renderer owned by this class instance.
	SimulSkyRenderer *GetSkyRenderer();
	//! Get a pointer to the 3d cloud renderer owned by this class instance.
	SimulCloudRenderer *GetCloudRenderer();
	//! Get a pointer to the 2d cloud renderer owned by this class instance.
	Simul2DCloudRenderer *Get2DCloudRenderer();
	//! Get a pointer to the rain renderer owned by this class instance.
	SimulPrecipitationRenderer *GetPrecipitationRenderer();
	//! Get a pointer to the atmospherics renderer owned by this class instance.
	SimulAtmosphericsRenderer *GetAtmosphericsRenderer();
	//! Get the current debug text as a c-string pointer. We don't use TCHAR here because Qt does not support using wchar_t as a built-in type.
	const char *GetDebugText() const;
	void EnableRain(bool e=true);
	float GetTotalBrightness() const;
	// Connect-up sky, clouds - now in base
	//void ConnectInterfaces();
protected:
	 void ReverseDepthChanged()
	 {
		 ReverseDepth=false;
	 }
	Framebuffer framebuffer;
	Framebuffer lowdef_framebuffer;
	bool Restore3DCloudObjects();
	bool Restore2DCloudObjects();
	//! The size of the 2D buffer the sky is rendered to.
	int BufferWidth,BufferHeight;
	LPDIRECT3DDEVICE9				m_pd3dDevice;
	LPDIRECT3DVERTEXDECLARATION9	m_pBufferVertexDecl;
	LPDIRECT3DTEXTURE9 temp_depth_texture;
	//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
	LPD3DXEFFECT					m_pBufferToScreenEffect;
	D3DXHANDLE						SkyOverStarsTechnique;
	D3DXHANDLE						CloudBlendTechnique;

	D3DXHANDLE						bufferTexture;

	bool CreateBuffers();
	bool RenderBufferToScreen(LPDIRECT3DTEXTURE9 texture);
	simul::base::SmartPtr<SimulSkyRenderer> simulSkyRenderer;
	simul::base::SmartPtr<SimulCloudRenderer> simulCloudRenderer;
	simul::base::SmartPtr<SimulLightningRenderer> simulLightningRenderer;
	simul::base::SmartPtr<Simul2DCloudRenderer> simul2DCloudRenderer;
	simul::base::SmartPtr<SimulPrecipitationRenderer> simulPrecipitationRenderer;
	simul::base::SmartPtr<SimulAtmosphericsRenderer> simulAtmosphericsRenderer;
	float							exposure;
	float							gamma;
	bool							use_buffer;
	float							exposure_multiplier;
	bool							show_rain;
};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif