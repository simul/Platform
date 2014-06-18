
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
	,m_pStarsVtxDecl(NULL)
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
	
	// Stars vertex declaration
	{
		D3D11_INPUT_ELEMENT_DESC decl[]=
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD",	0, DXGI_FORMAT_R32_FLOAT,			0,	12,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		SAFE_RELEASE(m_pStarsVtxDecl);
		//hr=m_pd3dDevice->CreateInputLayout(decl,2, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_pStarsVtxDecl);
		
		ID3DBlob *VS;
		ID3DBlob *errorMsgs=NULL;
		std::string dummy_shader;
		dummy_shader="struct vertexInput"
					"{";
		for(int i=0;i<2;i++)
		{
			D3D11_INPUT_ELEMENT_DESC &dec=decl[i];
			std::string format;
			switch(dec.Format)
			{
			case DXGI_FORMAT_R32G32B32A32_FLOAT:
				format="float4";
				break;
			case DXGI_FORMAT_R32G32B32_FLOAT:
				format="float3";
				break;
			case DXGI_FORMAT_R32G32_FLOAT:
				format="float2";
				break;
			case DXGI_FORMAT_R32_FLOAT:
				format="float";
				break;
			};
			dummy_shader+="   ";
			dummy_shader+=format+" ";
			std::string name=dec.SemanticName;
			if(strcmp(dec.SemanticName,"POSITION")!=0)
				name+=('0'+dec.SemanticIndex);
			dummy_shader+=name;
			dummy_shader+="_";
			dummy_shader+=" : ";
			dummy_shader+=name;
			dummy_shader+=";";
					//"	float3 position		: POSITION;"
					//"	float texCoords		: TEXCOORD0;";
		}
		dummy_shader+="};"
					"struct vertexOutput"
					"{"
					"	float4 hPosition	: SV_POSITION;"
					"};"
					"vertexOutput VS_Main(vertexInput IN) "
					"{"
					"	vertexOutput OUT;"
					"	OUT.hPosition	=float4(1.0,1.0,1.0,1.0);"
					"	return OUT;"
					"}";
		const char *str=dummy_shader.c_str();
		size_t len=strlen(str);
		HRESULT hr=D3DX11CompileFromMemory(str,len,"dummy",NULL,NULL,"VS_Main", "vs_4_0", 0, 0, 0, &VS, &errorMsgs, 0);
		if(hr!=S_OK)
		{
			const char *e=(const char*)errorMsgs->GetBufferPointer();
			std::cerr<<e<<std::endl;
		}
		m_pd3dDevice->CreateInputLayout(decl, 2, VS->GetBufferPointer(), VS->GetBufferSize(), &m_pStarsVtxDecl);
		if(VS)
			VS->Release();
		if(errorMsgs)
			errorMsgs->Release();
	}
}

void SimulSkyRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(m_pStarsVtxDecl);
	if(light_table)
		light_table->InvalidateDeviceObjects();
	if(light_table_2d)
		light_table_2d->InvalidateDeviceObjects();
	// Set the stored texture sizes to zero, so the textures will be re-created.
	numFadeDistances=numFadeElevations=numAltitudes=0;
	sunQuery.InvalidateDeviceObjects();
	earthShadowUniforms.InvalidateDeviceObjects();
	skyConstants.InvalidateDeviceObjects();
	gpuSkyGenerator.InvalidateDeviceObjects();
	::operator delete[](star_vertices,memoryInterface);
	star_vertices=NULL;
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

void SimulSkyRendererDX1x::EnsureCorrectTextureSizes()
{
	simul::sky::int3 i=skyKeyframer->GetTextureSizes();
	int num_dist=i.x;
	int num_elev=i.y;
	int num_alt=i.z;
	bool uav=gpuSkyGenerator.GetEnabled();
	for(int i=0;i<3;i++)
	{
		loss_textures[i]->ensureTexture3DSizeAndFormat(renderPlatform,num_alt,num_elev,num_dist,DXGI_FORMAT_R32G32B32A32_FLOAT,uav);
		insc_textures[i]->ensureTexture3DSizeAndFormat(renderPlatform,num_alt,num_elev,num_dist,DXGI_FORMAT_R32G32B32A32_FLOAT,uav);
		skyl_textures[i]->ensureTexture3DSizeAndFormat(renderPlatform,num_alt,num_elev,num_dist,DXGI_FORMAT_R32G32B32A32_FLOAT,uav);
	}
	light_table_2d->ensureTexture2DSizeAndFormat(renderPlatform,num_alt*32,4,DXGI_FORMAT_R32G32B32A32_FLOAT,true);
	
	if(!num_dist||!num_elev||!num_alt)
		return;
	if(numFadeDistances==num_dist&&numFadeElevations==num_elev&&numAltitudes==num_alt)
		return;
	if(!m_pd3dDevice)
		return;
	numFadeDistances=num_dist;
	numFadeElevations=num_elev;
	numAltitudes=num_alt;
	for(int i=0;i<3;i++)
	{
		fade_texture_iterator[i].texture_index=i;
	}
	CreateFadeTextures();
}

void SimulSkyRendererDX1x::CreateFadeTextures()
{
	if(loss_2d)
	{
		loss_2d->ensureTexture2DSizeAndFormat(renderPlatform,numFadeDistances,numFadeElevations,DXGI_FORMAT_R32G32B32A32_FLOAT,false,true);
	}
	if(insc_2d)
	{
		insc_2d->ensureTexture2DSizeAndFormat(renderPlatform,numFadeDistances,numFadeElevations,DXGI_FORMAT_R32G32B32A32_FLOAT,false,true);
	}
	if(over_2d)
	{
		over_2d->ensureTexture2DSizeAndFormat(renderPlatform,numFadeDistances,numFadeElevations,DXGI_FORMAT_R32G32B32A32_FLOAT,false,true);
	}
	if(skyl_2d)
	{
		skyl_2d->ensureTexture2DSizeAndFormat(renderPlatform,numFadeDistances,numFadeElevations,DXGI_FORMAT_R32G32B32A32_FLOAT,false,true);
	}
	illumination_2d->ensureTexture2DSizeAndFormat(renderPlatform,128,numFadeElevations,DXGI_FORMAT_R32G32B32A32_FLOAT,false,true);
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

	flareTexture		=effect->asD3DX11Effect()->GetVariableByName("flareTexture")->AsShaderResource();
			 
	inscTexture			=effect->asD3DX11Effect()->GetVariableByName("inscTexture")->AsShaderResource();
	skylTexture			=effect->asD3DX11Effect()->GetVariableByName("skylTexture")->AsShaderResource();
						 
	fadeTexture1		=effect->asD3DX11Effect()->GetVariableByName("fadeTexture1")->AsShaderResource();
	fadeTexture2		=effect->asD3DX11Effect()->GetVariableByName("fadeTexture2")->AsShaderResource();
	illuminationTexture	=effect->asD3DX11Effect()->GetVariableByName("illuminationTexture")->AsShaderResource();
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
/*	if(!effect)
		return (hr==S_OK);
	float magnitude=exposure*(1.f-sun_occlusion);
	float alt_km=0.001f*(cam_pos.z);
	sunlight=skyKeyframer->GetLocalIrradiance(alt_km);
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	static float sun_mult=0.5f;
	sunlight*=sun_mult*magnitude;
	colour->SetFloatVector(sunlight);
	//effect->SetTechnique(techFlare);
	//flareTexture->SetResource(flare_texture_SRV);
	D3DXVECTOR3 sun_dir(skyKeyframer->GetDirectionToSun());
//	hr=RenderAngledQuad(sun_dir,sun_angular_radius*20.f*magnitude);
	RenderAngledQuad(m_pd3dDevice,sun_dir,sun_angular_radius*20.f*magnitude,effect,techFlare,view,proj,sun_dir);*/
	return (hr==S_OK);
}

bool SimulSkyRendererDX1x::Render2DFades(crossplatform::DeviceContext &deviceContext)
{
	if(!techFade3DTo2D)
		return false;
	if(!loss_2d||!insc_2d||!skyl_2d)
		return false;
	// First render the illumination buffer, to get earthShadow and overcast properties.
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.platform_context;
	RenderIlluminationBuffer(deviceContext);
	SIMUL_COMBINED_PROFILE_START(pContext,"Render2DFades")
	// Current keyframe properties:
	const simul::sky::SkyKeyframe *K=skyKeyframer->GetInterpolatedKeyframe();
	math::Vector3 cam_pos=camera::GetCameraPosVector(deviceContext.viewStruct.view);
	// Clear the screen to black:
	static float clearColor[4]={0.0,0.0,0.0,0.0};
	skyConstants.skyInterp			=skyKeyframer->GetSubdivisionInterpolation(skyKeyframer->GetTime()).interpolation;
	skyConstants.altitudeTexCoord	=skyKeyframer->GetAltitudeTexCoord(cam_pos.z/1000.f);
	sky::OvercastStruct o			=skyKeyframer->GetCurrentOvercastState();
	skyConstants.overcast			=o.overcast;
	skyConstants.overcastBaseKm		=o.overcast_base_km;
	skyConstants.overcastRangeKm	=o.overcast_scale_km;
	skyConstants.eyePosition		=cam_pos;
	skyConstants.cloudShadowRange	=sqrt(80.f/skyKeyframer->GetMaxDistanceKm());
	skyConstants.cycled_index		=texture_cycle%3;
	skyConstants.Apply(deviceContext);
#if 1
	illuminationTexture->SetResource(illumination_2d->AsD3D11ShaderResourceView());
	{
		SIMUL_COMBINED_PROFILE_START(pContext,"Loss")
		V_CHECK(fadeTexture1->SetResource(loss_textures[(texture_cycle+0)%3]->AsD3D11ShaderResourceView()));
		V_CHECK(fadeTexture2->SetResource(loss_textures[(texture_cycle+1)%3]->AsD3D11ShaderResourceView()));
		effect->Apply(deviceContext,techFade3DTo2D,0);
		loss_2d->activateRenderTarget(deviceContext);
			simul::dx11::UtilityRenderer::DrawQuad(pContext);
		loss_2d->deactivateRenderTarget();
		effect->Unapply(deviceContext);
		SIMUL_COMBINED_PROFILE_END(pContext)
	}
	{
		SIMUL_COMBINED_PROFILE_START(pContext,"Inscatter")
		V_CHECK(fadeTexture1->SetResource(insc_textures[(texture_cycle+0)%3]->AsD3D11ShaderResourceView()));
		V_CHECK(fadeTexture2->SetResource(insc_textures[(texture_cycle+1)%3]->AsD3D11ShaderResourceView()));
		effect->Apply(deviceContext,techFade3DTo2D,0);
		insc_2d->activateRenderTarget(deviceContext);
			simul::dx11::UtilityRenderer::DrawQuad(pContext);
		insc_2d->deactivateRenderTarget();
		effect->Unapply(deviceContext);
		SIMUL_COMBINED_PROFILE_END(pContext)
	}

	{
		SIMUL_COMBINED_PROFILE_START(pContext,"Skylight")
		V_CHECK(fadeTexture1->SetResource(skyl_textures[(texture_cycle+0)%3]->AsD3D11ShaderResourceView()));
		V_CHECK(fadeTexture2->SetResource(skyl_textures[(texture_cycle+1)%3]->AsD3D11ShaderResourceView()));
		//V_CHECK(ApplyPass(pContext,techFade3DTo2D->GetPassByIndex(0)));
		effect->Apply(deviceContext,techFade3DTo2D,0);
		skyl_2d->activateRenderTarget(deviceContext);
			simul::dx11::UtilityRenderer::DrawQuad(pContext);
		skyl_2d->deactivateRenderTarget();
		effect->Unapply(deviceContext);
		SIMUL_COMBINED_PROFILE_END(pContext)
	}
	crossplatform::EffectTechnique* hTechniqueOverc		=effect->GetTechniqueByName("overcast_inscatter");
	// We will bake the overcast effect into the over_2d texture.
	{
		SIMUL_COMBINED_PROFILE_START(pContext,"Overcast")
		V_CHECK(inscTexture->SetResource(insc_2d->AsD3D11ShaderResourceView()));
		V_CHECK(ApplyPass(pContext,hTechniqueOverc->asD3DX11EffectTechnique()->GetPassByIndex(0)));
		over_2d->activateRenderTarget(deviceContext);
			simul::dx11::UtilityRenderer::DrawQuad(pContext);
		over_2d->deactivateRenderTarget();
		SIMUL_COMBINED_PROFILE_END(pContext)
	}
	// light_table - using compute.
	{
		SIMUL_COMBINED_PROFILE_START(pContext,"LightTable")
		simul::dx11::setTexture(effect->asD3DX11Effect()				,"sourceTexture"	,light_table->AsD3D11ShaderResourceView());
		simul::dx11::setUnorderedAccessView(effect->asD3DX11Effect()	,"targetTexture"	,light_table_2d->AsD3D11UnorderedAccessView());
		ID3DX11EffectTechnique* m_TechniqueLightTableInterp	=effect->GetTechniqueByName("interp_light_table")->asD3DX11EffectTechnique();
		V_CHECK(ApplyPass(pContext,m_TechniqueLightTableInterp->GetPassByIndex(0)));
		pContext->Dispatch(light_table_2d->width,light_table_2d->length,1);
		simul::dx11::setTexture(effect->asD3DX11Effect()				,"sourceTexture"	,(ID3D11ShaderResourceView*)NULL);
		simul::dx11::setUnorderedAccessView(effect->asD3DX11Effect()	,"targetTexture"	,NULL);
		V_CHECK(ApplyPass(pContext,m_TechniqueLightTableInterp->GetPassByIndex(0)));
		SIMUL_COMBINED_PROFILE_END(pContext)
	}
#endif
	V_CHECK(fadeTexture1->SetResource(NULL));
	V_CHECK(fadeTexture2->SetResource(NULL));
	illuminationTexture->SetResource(NULL);
	V_CHECK(ApplyPass(pContext,techFade3DTo2D->asD3DX11EffectTechnique()->GetPassByIndex(0)));
	SIMUL_COMBINED_PROFILE_END(pContext)
	return true;
}

void SimulSkyRendererDX1x::RenderIlluminationBuffer(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *context=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	SIMUL_COMBINED_PROFILE_START(context,"RenderIlluminationBuffer")
	math::Vector3 cam_pos=camera::GetCameraPosVector(deviceContext.viewStruct.view);
	SetIlluminationConstants(earthShadowUniforms,skyConstants,cam_pos);
	earthShadowUniforms.Apply(deviceContext);
	skyConstants.Apply(deviceContext);
	// Clear the screen to black:
	static float clearColor[4]={0.0,1.0,0.0,1.0};
	{
		ID3DX11EffectTechnique *tech=effect->GetTechniqueByName("illumination_buffer")->asD3DX11EffectTechnique();
		ApplyPass(context,tech->GetPassByIndex(0));
		illumination_2d->activateRenderTarget(deviceContext);
		simul::dx11::UtilityRenderer::DrawQuad(context);
		illumination_2d->deactivateRenderTarget();
	}
	SIMUL_COMBINED_PROFILE_END(context)
}

bool SimulSkyRendererDX1x::RenderPointStars(crossplatform::DeviceContext &deviceContext,float exposure)
{
	HRESULT hr=S_OK;
	math::Matrix4x4 tmp1, tmp2,wvp;
	deviceContext.viewStruct.view.Inverse(tmp1);
	math::Vector3 cam_pos(tmp1._41,tmp1._42,tmp1._43);
	math::Matrix4x4 world;
	GetSiderealTransform((float*)&world);
	world._41=cam_pos.x;
	world._42=cam_pos.y;
	world._43=cam_pos.z;
	world.Inverse(tmp2);
	math::Multiply4x4(tmp1,world,deviceContext.viewStruct.view);
	math::Multiply4x4(tmp2,tmp1,deviceContext.viewStruct.proj);
	tmp2.Transpose(wvp);
	skyConstants.worldViewProj=(const float *)(&tmp2);
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();


	skyConstants.starBrightness	= exposure * skyKeyframer->GetCurrentStarBrightness();
	if(skyConstants.starBrightness<minimumStarBrightness)
		return true;
	if(skyConstants.starBrightness<minimumStarBrightness)
		return true;
	effect->Apply(deviceContext,techPointStars,0);
	//hr=ApplyPass(pContext,techPointStars->asD3DX11EffectTechnique()->GetPassByIndex(0));
	skyConstants.Apply(deviceContext);

	int current_num_stars=skyKeyframer->stars.GetNumStars();
	if(!star_vertices||current_num_stars!=num_stars)
	{
		BuildStarsBuffer();
	}

	ID3D11InputLayout* previousInputLayout;
	pContext->IAGetInputLayout( &previousInputLayout );
	D3D_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);

	pContext->IASetInputLayout( m_pStarsVtxDecl );
	UINT stride = sizeof(StarVertext);
	UINT offset = 0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0] = 0;
    Offsets[0] = 0;
	ID3D11Buffer *buf=m_pStarsVertexBuffer->AsD3D11Buffer();
	pContext->IASetVertexBuffers(	0,			// the first input slot for binding
									1,			// the number of buffers in the array
									&buf,		// the array of vertex buffers
									&stride,	// array of stride values, one for each buffer
									&offset );	// array of offset values, one for each buffer
	
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	pContext->Draw(num_stars,0);

	pContext->IASetPrimitiveTopology(previousTopology);
	pContext->IASetInputLayout( previousInputLayout );
	SAFE_RELEASE(previousInputLayout);
	effect->Unapply(deviceContext);
	return true;
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

	ID3DX11EffectTechnique*	techShowFadeTable				=effect->GetTechniqueByName("show_fade_table")->asD3DX11EffectTechnique();
	ID3DX11EffectTechnique*	techniqueShowFade				=effect->GetTechniqueByName("show_fade_texture")->asD3DX11EffectTechnique();
	ID3DX11EffectShaderResourceVariable*	inscTexture		=effect->asD3DX11Effect()->GetVariableByName("inscTexture")->AsShaderResource();

	ID3DX11EffectTechnique*	techniqueShowIlluminationBuffer	=effect->GetTechniqueByName("show_illumination_buffer")->asD3DX11EffectTechnique();
	
	int y=y0+8;
	y+=size+8;
	y+=size+8;
	//inscTexture->SetResource(illumination_2d->AsD3D11ShaderResourceView());
	//UtilityRenderer::DrawQuad2(context,x0,y,size,size,effect->asD3DX11Effect(),techniqueShowIlluminationBuffer);
	//deviceContext.renderPlatform->Print(deviceContext	,x0,y		,"illumination");

	int x=16+size;
	y=y0+8;
	bool show_3=gpuSkyGenerator.GetEnabled();
	BaseSkyRenderer::RenderFades(deviceContext,x0,y0,width,height);
/*
	x0+=2*(size+8);
	for(int i=0;i<numAltitudes;i++)
	{
		float atc=(float)(numAltitudes-0.5f-i)/(float)(numAltitudes);
		int x1=x0,x2=x0+s+8;
		int y=y0+i*(s+2);
		skyConstants.altitudeTexCoord=atc;
		skyConstants.Apply(deviceContext);
		for(int j=0;j<(show_3?3:2);j++)
		{
			int x=x0+(s+8)*j;
			fadeTexture1->SetResource(loss_textures[(texture_cycle+j)%3]->AsD3D11ShaderResourceView());
			UtilityRenderer::DrawQuad2(context,x	,y+8		,s,s	,effect->asD3DX11Effect(),techniqueShowFade);
			fadeTexture1->SetResource(insc_textures[(texture_cycle+j)%3]->AsD3D11ShaderResourceView());
			UtilityRenderer::DrawQuad2(context,x	,y+16+size	,s,s	,effect->asD3DX11Effect(),techniqueShowFade);
			fadeTexture1->SetResource(skyl_textures[(texture_cycle+j)%3]->AsD3D11ShaderResourceView());
			UtilityRenderer::DrawQuad2(context,x	,y+24+2*size,s,s	,effect->asD3DX11Effect(),techniqueShowFade);
		}
	}*/
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
//	D3DXMatrixInverse(&tmp1,NULL,&view);
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

void SimulSkyRendererDX1x::SetMatrices(const simul::math::Matrix4x4 &v,const simul::math::Matrix4x4 &p)
{
}

void SimulSkyRendererDX1x::Get2DLossAndInscatterTextures(void* *l1,void* *i1,void* *s,void* *o)
{
	if(loss_2d)
		*l1=(void*)loss_2d->AsD3D11ShaderResourceView();
	else
		*l1=NULL;
	if(insc_2d)
		*i1=(void*)insc_2d->AsD3D11ShaderResourceView();
	else
		*l1=NULL;
	if(skyl_2d)
		*s=(void*)skyl_2d->AsD3D11ShaderResourceView();
	else
		*s=NULL;
	if(over_2d)
		*o=(void*)over_2d->AsD3D11ShaderResourceView();
	else
		*o=NULL;
}

void *SimulSkyRendererDX1x::GetIlluminationTexture()
{
	return illumination_2d->AsD3D11ShaderResourceView();
}

void *SimulSkyRendererDX1x::GetLightTableTexture()
{
	return light_table_2d->AsD3D11ShaderResourceView();
}