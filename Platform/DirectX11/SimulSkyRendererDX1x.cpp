
// Copyright (c) 2007-2012 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulSkyRendererDX1x.cpp A renderer for skies.
#define NOMINMAX
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"

#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <dxerr.h>
#endif
#include <string>
#include <algorithm>			// for std::min / max
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Pi.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Sky.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/Texture.h"
#include "Simul/Platform/DirectX11/SimulSkyRendererDX1x.h"
#include "Simul/Platform/DirectX11/Effect.h"
#include "Simul/Platform/DirectX11/Layout.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Buffer.h"
#include "Simul/Math/Pi.h"
#include "Simul/Camera/Camera.h"
#include "D3dx11effect.h"

using namespace simul;
using namespace dx11;

SimulSkyRendererDX1x::SimulSkyRendererDX1x(simul::sky::SkyKeyframer *sk,simul::base::MemoryInterface *mem)
	:simul::sky::BaseSkyRenderer(sk,mem)
{
}


void SimulSkyRendererDX1x::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	if(!r)
		return;
	renderPlatform=r;
	BaseSkyRenderer::RestoreDeviceObjects(r);
	HRESULT hr=S_OK;
	gpuSkyGenerator.RestoreDeviceObjects(renderPlatform);
	crossplatform::Texture *loss[3],*insc[3],*skyl[3];
	for(int i=0;i<3;i++)
	{
		loss[i]=loss_textures[i];
		insc[i]=insc_textures[i];
		skyl[i]=skyl_textures[i];
	}
	gpuSkyGenerator.SetDirectTargets(loss,insc,skyl,light_table);
	RecompileShaders();
	
	ClearIterators();
}

void SimulSkyRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	gpuSkyGenerator.InvalidateDeviceObjects();
	BaseSkyRenderer::InvalidateDeviceObjects();
}

void SimulSkyRendererDX1x::EnsureTexturesAreUpToDate(void *context)
{
	SIMUL_COMBINED_PROFILE_START(context,"SimulSkyRendererDX1x::EnsureTexturesAreUpToDate")
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	EnsureCorrectTextureSizes();
	EnsureTextureCycle();
	sky::GpuSkyParameters p;
	sky::GpuSkyAtmosphereParameters a;
	sky::GpuSkyInfraredParameters ir;
	sky::float4 wavelengthsNm=skyKeyframer->GetColourWavelengthsNm();
	for(int i=0;i<3;i++)
	{
		skyKeyframer->GetGpuSkyParameters(p,a,ir,i);
		int cycled_index=(texture_cycle+i)%3;
		if(gpuSkyGenerator.GetEnabled())
			gpuSkyGenerator.MakeLossAndInscatterTextures( wavelengthsNm,cycled_index,skyKeyframer->GetSkyInterface(),p,a,ir);
		else
			skyKeyframer->cpuSkyGenerator.MakeLossAndInscatterTextures( wavelengthsNm,cycled_index,skyKeyframer->GetSkyInterface(),p,a,ir);
	}
	SIMUL_COMBINED_PROFILE_END(pContext)
}

void SimulSkyRendererDX1x::RecompileShaders()
{
	if(!renderPlatform)
		return;
	BaseSkyRenderer::RecompileShaders();
	gpuSkyGenerator.RecompileShaders();
}


bool SimulSkyRendererDX1x::RenderFlare(float exposure)
{
	HRESULT hr=S_OK;
	return (hr==S_OK);
}

bool SimulSkyRendererDX1x::Render(void *context,bool blend)
{
	HRESULT hr=S_OK;
	return (hr==S_OK);
}

bool SimulSkyRendererDX1x::RenderFades(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height)
{
	int size=width/3;
	if(height/4<size)
		size=height/4;
	if(size<2)
		return false;
	int s							=size/numAltitudes-2;
	ID3D11DeviceContext *context	=deviceContext.asD3D11DeviceContext();
	
	D3D11_VIEWPORT viewport;
	UINT num_v		=1;
	context->RSGetViewports(&num_v,&viewport);
	UtilityRenderer::SetScreenSize((int)viewport.Width,(int)viewport.Height);
	
	int y=y0+8;
	y+=size+8;
	y+=size+8;
	int x=16+size;
	y=y0+8;
	bool show_3=gpuSkyGenerator.GetEnabled();
	BaseSkyRenderer::RenderFades(deviceContext,x0,y0,width,height);
	crossplatform::EffectTechnique*	techniqueColour=effect->GetTechniqueByName("colour_technique");
	
	float time_now=skyKeyframer->GetTime();
	math::Vector3 cam_pos=camera::GetCameraPosVector(deviceContext.viewStruct.view);
	float alt_km=cam_pos.z/1000.f;
	skyConstants.colour=skyKeyframer->GetLocalSunIrradiance(time_now,alt_km);
	skyConstants.Apply(deviceContext);
	deviceContext.renderPlatform->DrawQuad(deviceContext,x0,y+3*size+4,size,s,effect,techniqueColour);
	return true;
}

void SimulSkyRendererDX1x::DrawCubemap(crossplatform::DeviceContext &deviceContext,ID3D11ShaderResourceView *m_pCubeEnvMapSRV)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	D3DXMATRIX tmp1,tmp2,wvp;
	math::Matrix4x4 world;
	world.Identity();
	float tan_x=1.0f/deviceContext.viewStruct.proj(0, 0);
	float tan_y=1.0f/deviceContext.viewStruct.proj(1, 1);
	math::Vector3 cam_pos=simul::camera::GetCameraPosVector(deviceContext.viewStruct.view);//(tmp1._41,tmp1._42,tmp1._43);
	math::Vector3 pos((const float*)cam_pos);
	float size_req=tan_x*0.2f;
	static float size=5.f;
	float d=2.0f*size/size_req;
	math::Vector3 offs0(-0.7f*(tan_x-size_req)*d,0.7f*(tan_y-size_req)*d,-d);
	math::Vector3 offs;
	Multiply3(offs,*((const math::Matrix4x4*)(const float*)&deviceContext.viewStruct.view),offs0);
	pos+=offs;
	world._41=pos.x;
	world._42=pos.y;
	world._43=pos.z;
	camera::MakeWorldViewProjMatrix((float*)&wvp,(const float*)&world,(const float*)&deviceContext.viewStruct.view,(const float*)&deviceContext.viewStruct.proj);
	skyConstants.worldViewProj=&wvp._11;
	skyConstants.worldViewProj.transpose();
	skyConstants.Apply(deviceContext);
	ID3DX11EffectTechnique*				tech		=effect->asD3DX11Effect()->GetTechniqueByName("draw_cubemap");
	ID3D1xEffectShaderResourceVariable*	cubeTexture	=effect->asD3DX11Effect()->GetVariableByName("cubeTexture")->AsShaderResource();
	cubeTexture->SetResource(m_pCubeEnvMapSRV);
	HRESULT hr=ApplyPass(pContext,tech->GetPassByIndex(0));
	((dx11::RenderPlatform*)renderPlatform)->DrawCube(deviceContext);
}