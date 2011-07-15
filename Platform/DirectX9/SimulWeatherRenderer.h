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

#ifndef RENDERDEPTHBUFFERCALLBACK
#define RENDERDEPTHBUFFERCALLBACK
class RenderDepthBufferCallback
{
public:
	virtual void Render()=0;
};
#endif
//! A rendering class that encapsulates Simul skies and clouds. Create an instance of this class within a DirectX 9 program.
//! You can take this entire class and use it as source in your project, and 
//! make appropriate modifications where required.

//!

/*!
	\code
#include "Simul/Platform/Windows/DirectX 9/SimulWeatherRenderer.h"
	simulWeatherRenderer=new SimulWeatherRenderer(true,ScreenWidth/2,ScreenHeight/2,true,true,false,true,false);
	simulWeatherRenderer->SetDaytime(0.3f);
	\endcode

	We can load a sequence file containing keyframes and weather setup:
	\code
	std::ifstream ifs("default.seq",std::ios_base::binary);
	if(ifs.good())
	{
		simulWeatherRenderer->Load(ifs);
		ifs.close();
	}
	\endcode

	We can apply global modifications to all the keyframes, by iterating them in turn:
	\code
	simul::clouds::CloudKeyframer *ck=simulWeatherRenderer->GetCloudRenderer()->GetCloudKeyframer();
	for(int i=0;i<ck->GetNumKeyframes();i++)
	{
		simul::clouds::CloudKeyframer::Keyframe *k=(simul::clouds::CloudKeyframer::Keyframe *)ck->GetKeyframe(i);
		k->cloudiness=.6f;
		k->precipitation=0.f;
		k->lightning=0.f;
	}
	\endcode
	
	Create() initializes the renderers with the D3D device pointer.
	\code
	simulWeatherRenderer->Create(pd3dDevice);
	\endcode

\section devc Handling device changes
	When a new device has been set up:
\code
	simulWeatherRenderer->SetBufferSize(pBackBufferSurfaceDesc->Width/2,pBackBufferSurfaceDesc->Height/2);
	hr=simulWeatherRenderer->RestoreDeviceObjects(pd3dDevice);
\endcode
	When the device has been lost (e.g. the screen resolution was changed):
\code
	simulWeatherRenderer->InvalidateDeviceObjects();
\endcode
	\section updr Update and rendering
Usually once per frame, the weather update should be called, this will update the weather renderer's sub-renderers,
e.g. clouds, sky:
\code
	simulWeatherRenderer->Update(timestep_seconds);
\endcode
	To render, the following code is used:
\code
	simulWeatherRenderer->SetMatrices(view,proj);
	simulWeatherRenderer->Render();
	simulWeatherRenderer->RenderLateCloudLayer();
	simulWeatherRenderer->RenderLightning();
	simulWeatherRenderer->RenderPrecipitation();
\endcode
*/
SIMUL_DIRECTX9_EXPORT_CLASS SimulWeatherRenderer:public simul::clouds::BaseWeatherRenderer
{
public:
	SimulWeatherRenderer(const char *license_key,bool usebuffer=true,int width=320,
		int height=240,bool sky=true,bool clouds3d=true,bool clouds2d=true,
		bool rain=true,
		bool colour_sky=false);
	virtual ~SimulWeatherRenderer();
	//standard d3d object interface functions
	bool Create( LPDIRECT3DDEVICE9 pd3dDevice);
	//! Call this when the device has been created
	bool RestoreDeviceObjects(void *pd3dDevice);
	//! Call this when the 3D device has been lost.
	bool InvalidateDeviceObjects();
	//! Call this to draw the sky and clouds.
	bool RenderSky(bool buffer,bool is_cubemap);
	//! Call this to draw the clouds after the main scene.
	bool RenderLateCloudLayer(bool buf);
	//! Call this to draw lightning.
	bool RenderLightning();
	//! Call this to draw rain etc.
	bool RenderPrecipitation();
	//! Perform the once-per-frame time update.
	void Update(float dt);
#if defined(XBOX) || defined(DOXYGEN)
	//! Call this once per frame to set the matrices (X360 only).
	void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);
#endif
	//! Set the exposure, if we're using an hdr shader to render the sky buffer.
	void SetExposure(float ex){exposure=ex;}
	//! Get a pointer to the sky renderer owned by this class instance.
	class SimulSkyRenderer *GetSkyRenderer();
	//! Get a pointer to the 3d cloud renderer owned by this class instance.
	class SimulCloudRenderer *GetCloudRenderer();
	//! Get a pointer to the 2d cloud renderer owned by this class instance.
	class Simul2DCloudRenderer *Get2DCloudRenderer();
	//! Get a pointer to the rain renderer owned by this class instance.
	class SimulPrecipitationRenderer *GetPrecipitationRenderer();
	//! Get a pointer to the atmospherics renderer owned by this class instance.
	class SimulAtmosphericsRenderer *GetAtmosphericsRenderer();
	//! Get the current debug text as a c-string pointer. We don't use TCHAR here because Qt does not support using wchar_t as a built-in type.
	const char *GetDebugText() const;
	//! Get a timing value - useful for performance evaluation.
	float GetTiming() const;
	//! Set a callback to fill in the depth/Z buffer in the lo-res sky texture.
	void SetRenderDepthBufferCallback(RenderDepthBufferCallback *cb);
	void SetBufferSize(int w,int h);
	//! Enable or disable the 3d and 2d cloud layers.
	virtual void EnableLayers(bool clouds3d,bool clouds2d);
	void EnableRain(bool e=true);
	float GetTotalBrightness() const;
	//! Connect-up sky, clouds:
	void ConnectInterfaces();
protected:
	Framebuffer framebuffer;
	Framebuffer lowdef_framebuffer;
	bool RenderLateCloudLayer(int buffer_index,bool buf);
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
	RenderDepthBufferCallback *renderDepthBufferCallback;
	simul::base::SmartPtr<class SimulSkyRenderer> simulSkyRenderer;
	simul::base::SmartPtr<class SimulCloudRenderer> simulCloudRenderer;
	simul::base::SmartPtr<class SimulLightningRenderer> simulLightningRenderer;
	simul::base::SmartPtr<class Simul2DCloudRenderer> simul2DCloudRenderer;
	simul::base::SmartPtr<class SimulPrecipitationRenderer> simulPrecipitationRenderer;
	simul::base::SmartPtr<class SimulAtmosphericsRenderer> simulAtmosphericsRenderer;
	float							exposure;
	float							gamma;
	bool							use_buffer;
	float							exposure_multiplier;
	bool							show_rain;
};
#ifdef _MSC_VER
	#pragma warning(pop)
#endif