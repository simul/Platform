
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
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Buffer.h"
#include "Simul/Math/Pi.h"
#include "Simul/Camera/Camera.h"
#include "D3dx11effect.h"

using namespace simul;
using namespace dx11;

SimulSkyRendererDX1x::SimulSkyRendererDX1x(simul::sky::SkyKeyframer *sk,simul::base::MemoryInterface *mem)
	:simul::sky::BaseSkyRenderer(sk,mem)
	,m_pd3dDevice(NULL)
	,cycle(0)
{
	skyKeyframer->SetCalculateAllAltitudes(true);
}

void SimulSkyRendererDX1x::SetStepsPerDay(unsigned steps)
{
	skyKeyframer->SetUniformKeyframes(steps);
}

void SimulSkyRendererDX1x::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	if(!r)
		return;
	renderPlatform=r;
	BaseSkyRenderer::RestoreDeviceObjects(r);
	m_pd3dDevice=renderPlatform->AsD3D11Device();
	sunQuery.RestoreDeviceObjects(m_pd3dDevice);
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
	if(light_table)
		light_table->InvalidateDeviceObjects();
	if(light_table_2d)
		light_table_2d->InvalidateDeviceObjects();
	// Set the stored texture sizes to zero, so the textures will be re-created.
	numFadeDistances=numFadeElevations=numAltitudes=0;
	sunQuery.InvalidateDeviceObjects();
	gpuSkyGenerator.InvalidateDeviceObjects();
	BaseSkyRenderer::InvalidateDeviceObjects();
}

bool SimulSkyRendererDX1x::Destroy()
{
	InvalidateDeviceObjects();
	::operator delete(loss_2d,memoryInterface);
	::operator delete(insc_2d,memoryInterface);
	::operator delete(over_2d,memoryInterface);
	::operator delete(skyl_2d,memoryInterface);
	return true;
}

SimulSkyRendererDX1x::~SimulSkyRendererDX1x()
{
	Destroy();
}

float SimulSkyRendererDX1x::GetFadeInterp() const
{
	return skyKeyframer->GetSubdivisionInterpolation(skyKeyframer->GetTime()).interpolation;
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
	for(int i=0;i<3;i++)
	{
		skyKeyframer->GetGpuSkyParameters(p,a,ir,i);
		int cycled_index=(texture_cycle+i)%3;
		if(gpuSkyGenerator.GetEnabled())
			gpuSkyGenerator.MakeLossAndInscatterTextures(cycled_index,skyKeyframer->GetSkyInterface(),p,a,ir);
		else
			skyKeyframer->cpuSkyGenerator.MakeLossAndInscatterTextures(cycled_index,skyKeyframer->GetSkyInterface(),p,a,ir);
	}
	SIMUL_COMBINED_PROFILE_END(pContext)
}

void SimulSkyRendererDX1x::CycleTexturesForward()
{
	cycle++;
	std::swap(fade_texture_iterator[0],fade_texture_iterator[1]);
	std::swap(fade_texture_iterator[1],fade_texture_iterator[2]);
	for(int i=0;i<3;i++)
		fade_texture_iterator[i].texture_index=i;
}

void SimulSkyRendererDX1x::EnsureTextureCycle()
{
	int cyc=(skyKeyframer->GetTextureCycle())%3;
	while(texture_cycle!=cyc)
	{
		this->CycleTexturesForward();
		texture_cycle++;
		texture_cycle=texture_cycle%3;
		if(texture_cycle<0)
			texture_cycle+=3;
	}
}

void SimulSkyRendererDX1x::FillFadeTex(ID3D11DeviceContext *pContext,int texture_index,int texel_index,int num_texels,
						const simul::sky::float4 *loss_float4_array,
						const simul::sky::float4 *insc_float4_array,
						const simul::sky::float4 *skyl_float4_array)
{
	int slice_size	=numFadeElevations*numAltitudes;
	int end_slice	=(texel_index+num_texels-1)/(slice_size);
	int end_row		=(texel_index+num_texels-1)/numAltitudes-end_slice*numFadeElevations;
	
	dx11::Texture *L=static_cast<dx11::Texture*>(loss_textures[(texture_cycle+texture_index)%3]);
	dx11::Texture *I=static_cast<dx11::Texture*>(insc_textures[(texture_cycle+texture_index)%3]);
	dx11::Texture *S=static_cast<dx11::Texture*>(skyl_textures[(texture_cycle+texture_index)%3]);

	int row_width	=L->mapped.RowPitch/sizeof(simul::sky::float4);
	int row_skip	=row_width-numAltitudes;
	int slice_width	=L->mapped.DepthPitch/sizeof(simul::sky::float4);
	int slice_skip	=slice_width-numFadeElevations*row_width;
	int end_texel=texel_index+num_texels;
	int last_row=0,last_slice=0;
	while(texel_index<end_texel)
	{
		int num_left=end_texel-texel_index;
		int slice	=texel_index/slice_size;
		int row		=texel_index/numAltitudes-slice*numFadeElevations;
		int col		=texel_index-(row+slice*numFadeElevations)*numAltitudes;
		simul::sky::float4 *loss_ptr=(simul::sky::float4 *)(L->mapped.pData);
		simul::sky::float4 *insc_ptr=(simul::sky::float4 *)(I->mapped.pData);
		simul::sky::float4 *skyl_ptr=(simul::sky::float4 *)(S->mapped.pData);
		loss_ptr+=slice*slice_width+row*row_width+col;
		insc_ptr+=slice*slice_width+row*row_width+col;
		skyl_ptr+=slice*slice_width+row*row_width+col;
		if(slice!=end_slice)
		{
			num_left=(slice+1)*slice_size-texel_index;
			if(row!=numFadeElevations-1)
				num_left=slice*slice_size+(row+1)*numAltitudes-texel_index;
		}
		else
			if(row!=end_row)
				num_left=slice*slice_size+(row+1)*numAltitudes-texel_index;
		memcpy(loss_ptr,loss_float4_array	,num_left*sizeof(simul::sky::float4));
		memcpy(insc_ptr,insc_float4_array	,num_left*sizeof(simul::sky::float4));
		memcpy(skyl_ptr,skyl_float4_array	,num_left*sizeof(simul::sky::float4));
		loss_float4_array+=num_left;
		insc_float4_array+=num_left;
		skyl_float4_array+=num_left;
		last_row=row;
		last_slice=slice;
		texel_index+=num_left;
	}
}

void SimulSkyRendererDX1x::RecompileShaders()
{
	HRESULT hr=S_OK;

	if(!m_pd3dDevice)
		return;
	BaseSkyRenderer::RecompileShaders();
			 
	techQuery			=effect->GetTechniqueByName("sun_query");

	earthShadowUniforms.LinkToEffect(effect,"EarthShadowUniforms");
	skyConstants.LinkToEffect(effect,"SkyConstants");
	gpuSkyGenerator.RecompileShaders();
}

float SimulSkyRendererDX1x::CalcSunOcclusion(crossplatform::DeviceContext &deviceContext,float cloud_occlusion)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
#ifndef _XBOX_ONE
	sun_occlusion=cloud_occlusion;
	if(!techQuery||!techQuery->asD3DX11EffectTechnique()->IsValid())
		return sun_occlusion;
	// Start the query
	vec3 sun_dir(skyKeyframer->GetDirectionToSun());
	SetConstantsForPlanet(skyConstants,deviceContext.viewStruct.view,deviceContext.viewStruct.proj,sun_dir,skyKeyframer->GetSkyInterface()->GetSunRadiusArcMinutes()/60.f*pi/180.f,sky::float4(1,1,1,1),sun_dir);
	// 2 * sun radius because we want glow arprofileData.DisjointQuery[currFrame]ound it.
	skyConstants.Apply(deviceContext);
/*d3dQuery->Begin();*/
	// Start the query
    sunQuery.Begin(pContext);
	{
		ApplyPass(pContext,techQuery->asD3DX11EffectTechnique()->GetPassByIndex(0));
		UtilityRenderer::DrawQuad(pContext);
	}
	// End the query, get the data
	sunQuery.End(pContext);
	D3D11_VIEWPORT viewport;
	UINT num_v=1;
	pContext->RSGetViewports(&num_v,&viewport);
	float tan_half_fov_horizontal=1.f/deviceContext.viewStruct.proj._11;
	float pixelRadius		=tan(skyConstants.radiusRadians/2.f)*viewport.Width/tan_half_fov_horizontal;
	float maxPixelsVisible	=pi*pixelRadius*pixelRadius;
    UINT64 pixelsVisible	=0;
	sunQuery.GetData(pContext,&pixelsVisible,sizeof(UINT64));
 	sun_occlusion			=1.f-(float)pixelsVisible/maxPixelsVisible;
	if(sun_occlusion<0)
		sun_occlusion		=0;
	sun_occlusion			=1.f-(1.f-cloud_occlusion)*(1.f-sun_occlusion);
#endif
	return sun_occlusion;
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
	UtilityRenderer::DrawCube(pContext);
}