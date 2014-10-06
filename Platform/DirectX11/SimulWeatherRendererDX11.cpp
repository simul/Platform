#define NOMINMAX
// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulWeatherRendererDX11.cpp A renderer for skies, clouds and weather effects.

#include "SimulWeatherRendererDX11.h"
#if WINVER<0x602
#include <dxerr.h>
#endif
#include <string>

#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Sky/BaseSkyRenderer.h"
#include "Simul/Sky/BaseAtmosphericsRenderer.h"
#include "Simul/Clouds/BasePrecipitationRenderer.h"

#include "Simul/Platform/DirectX11/SaveTextureDX1x.h"
#include "Simul/Clouds/Base2DCloudRenderer.h"
#include "Simul/Clouds/BaseLightningRenderer.h"
#include "Simul/Base/Timer.h"
#include "CreateEffectDX1x.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/DirectX11/Effect.h"
#include "MacrosDX1x.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "D3dx11effect.h"

using namespace simul;
using namespace dx11;


SimulWeatherRendererDX11::SimulWeatherRendererDX11(simul::clouds::Environment *env
													,simul::base::MemoryInterface *mem) :
	BaseWeatherRenderer(env,mem)
{
	sky::SkyKeyframer *sk			=env->skyKeyframer;
	clouds::CloudKeyframer *ck3d	=env->cloudKeyframer;
	baseSkyRenderer					=::new(memoryInterface) sky::BaseSkyRenderer(sk, memoryInterface);
	baseCloudRenderer				=::new(memoryInterface) clouds::BaseCloudRenderer(ck3d, memoryInterface);

	ConnectInterfaces();
}

SimulWeatherRendererDX11::~SimulWeatherRendererDX11()
{
	InvalidateDeviceObjects();
}

void SimulWeatherRendererDX11::SaveCubemapToFile(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,float exposure,float gamma)
{
	std::wstring filenamew=simul::base::Utf8ToWString(filename_utf8);
	crossplatform::DeviceContext deviceContext=renderPlatform->GetImmediateContext();
	CubemapFramebuffer	fb_cubemap;
	static int cubesize=1024;
	fb_cubemap.SetWidthAndHeight(cubesize,cubesize);
	fb_cubemap.SetFormat(crossplatform::RGBA_32_FLOAT);
	fb_cubemap.RestoreDeviceObjects(renderPlatform);
	dx11::Framebuffer gamma_correct;
	gamma_correct.SetWidthAndHeight(cubesize,cubesize);
	gamma_correct.RestoreDeviceObjects(renderPlatform);

	crossplatform::EffectTechnique *tech=effect->GetTechniqueByName("exposure_gamma");
	math::Matrix4x4 view;
	view.Identity();
	math::Vector3 cam_pos=GetCameraPosVector(view);
	math::Matrix4x4 view_matrices[6];
	camera::MakeCubeMatrices(view_matrices,cam_pos,ReverseDepth);
	bool noise3d=environment->cloudKeyframer->GetUse3DNoise();
	bool godrays=GetBaseAtmosphericsRenderer()->GetShowGodrays();
	GetBaseAtmosphericsRenderer()->SetShowGodrays(false);
	environment->cloudKeyframer->SetUse3DNoise(true);

	bool gamma_correction=false;//(filenamew.find(L".hdr")>=filenamew.length());
	int l=100;
	if(baseCloudRenderer)
	{
		baseCloudRenderer->RecompileShaders();
		l=baseCloudRenderer->GetMaxSlices(0);
		baseCloudRenderer->SetMaxSlices(0,250);
	}
	for(int i=0;i<6;i++)
	{
		fb_cubemap.SetCurrentFace(i);
		fb_cubemap.Activate(deviceContext);
		if(gamma_correction)
		{
			gamma_correct.Activate(deviceContext);
			gamma_correct.Clear(deviceContext, 0.f, 0.f, 0.f, 0.f, 0.f);
		}
		if(baseSkyRenderer)
		{
			D3DXMATRIX cube_proj;
			D3DXMatrixPerspectiveFovRH(&cube_proj,
				3.1415926536f/2.f,
				1.f,
				1.f,
				600000.f);
	
			deviceContext.viewStruct.view_id=0;
			deviceContext.viewStruct.proj	=(const float*)&cube_proj;
			deviceContext.viewStruct.view	=(const float*)&view_matrices[i];
			RenderSkyAsOverlay(deviceContext
				,false,exposure,gamma,false,NULL,simul::sky::float4(0,0,1.f,1.f),true,vec2(0,0));
			if(gamma_correction)
			{
				gamma_correct.Deactivate(deviceContext);
				simul::dx11::setTexture((ID3DX11Effect*)effect->platform_effect,"imageTexture",gamma_correct.GetBufferResource());
				hdrConstants.gamma=gamma;
				hdrConstants.exposure=exposure;
				hdrConstants.Apply(deviceContext);
				deviceContext.renderPlatform->ApplyShaderPass(deviceContext,effect,tech,0);
				//ApplyPass(m_pImmediateContext,((ID3DX11EffectTechnique*)tech->platform_technique)->GetPassByIndex(0));
				simul::dx11::UtilityRenderer::DrawQuad(deviceContext);
			}
		}
		fb_cubemap.Deactivate(deviceContext);
	}
	ID3D11Texture2D *tex=fb_cubemap.GetCopy(deviceContext.asD3D11DeviceContext());
	
	
	HRESULT hr=D3DX11SaveTextureToFileW(deviceContext.asD3D11DeviceContext(),tex,D3DX11_IFF_DDS,filenamew.c_str());

	SAFE_RELEASE(tex);
	environment->cloudKeyframer->SetUse3DNoise(noise3d);
	if(baseCloudRenderer)
	{
		baseCloudRenderer->SetMaxSlices(0,l);
	}
	GetBaseAtmosphericsRenderer()->SetShowGodrays(godrays);
}