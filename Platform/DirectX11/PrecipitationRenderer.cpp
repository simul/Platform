#define NOMINMAX
// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

#include "PrecipitationRenderer.h"
#include "CreateEffectDX1x.h"
#include "FramebufferDX1x.h"
#include "Utilities.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include "Simul/Base/ProfilingInterface.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
extern simul::dx11::RenderPlatform renderPlatformDx11;

using namespace simul;
using namespace dx11;

PrecipitationRenderer::PrecipitationRenderer() :
	m_pd3dDevice(NULL)
	,m_pVtxDecl(NULL)
	,effect(NULL)
	,rain_texture(NULL)
	,cubemap_SRV(NULL)
	,view_initialized(false)
{
}

void PrecipitationRenderer::RecompileShaders()
{
	if(!m_pd3dDevice)
		return;
	CreateEffect(m_pd3dDevice,&effect,"rain.fx");
	SAFE_RELEASE(rain_texture);
	m_hTechniqueRain			=effect->GetTechniqueByName("simul_rain");
	m_hTechniqueParticles		=effect->GetTechniqueByName("simul_particles");
	m_hTechniqueRainParticles	=effect->GetTechniqueByName("rain_particles");
	techniqueMoveParticles		=effect->GetTechniqueByName("move_particles");
	rainTexture					=effect->GetVariableByName("rainTexture")->AsShaderResource();

	rainConstants.LinkToEffect(effect,"RainConstants");
	perViewConstants.LinkToEffect(effect,"RainPerViewConstants");
	moisturePerViewConstants.LinkToEffect(effect,"MoisturePerViewConstants");

	ID3D11DeviceContext *pImmediateContext=NULL;
	m_pd3dDevice->GetImmediateContext(&pImmediateContext);
	ID3DX11EffectTechnique *tech		=effect->GetTechniqueByName("create_rain_texture");
	ApplyPass(pImmediateContext,tech->GetPassByIndex(0));
	simul::dx11::Framebuffer make_rain_fb(512,64);
	make_rain_fb.RestoreDeviceObjects(m_pd3dDevice);
	make_rain_fb.Activate(pImmediateContext);
	simul::dx11::UtilityRenderer::DrawQuad(pImmediateContext);
	make_rain_fb.Deactivate(pImmediateContext);
	rain_texture=make_rain_fb.buffer_texture_SRV;
	// Make sure it isn't destroyed when the fb goes out of scope:
	rain_texture->AddRef();
	view_initialized=false;
	// The array of rain textures:
	rainArrayTexture.create(m_pd3dDevice,16,512,32,DXGI_FORMAT_R32G32B32A32_FLOAT,true);
	// Use a compute shader to initialize the rain texture:
	// shader has been created:
	dx11::setUnorderedAccessView(effect,"targetTextureArray",rainArrayTexture.unorderedAccessView);
	ApplyPass(pImmediateContext,effect->GetTechniqueByName("make_rain_texture_array")->GetPassByIndex(0));

	pImmediateContext->Dispatch(16,512,32);

	// We can't detect if this has worked or not.
	pImmediateContext->GenerateMips(rainArrayTexture.m_pArrayTexture_SRV);

	D3D11_INPUT_ELEMENT_DESC decl[] =
{
		{"POSITION"	,0	,DXGI_FORMAT_R32G32B32_FLOAT	,0	,0	,D3D11_INPUT_PER_VERTEX_DATA,0},
		{"TYPE"		,0	,DXGI_FORMAT_R32_UINT			,0	,12	,D3D11_INPUT_PER_VERTEX_DATA,0},
		{"VELOCITY"	,0	,DXGI_FORMAT_R32G32B32_FLOAT	,0	,16	,D3D11_INPUT_PER_VERTEX_DATA,0},
		//{"DUMMY"	,0	,DXGI_FORMAT_R32_FLOAT			,0	,28	,D3D11_INPUT_PER_VERTEX_DATA,0},
	};
	SAFE_RELEASE(m_pVtxDecl);
    D3DX11_PASS_DESC PassDesc;
	m_hTechniqueRainParticles->GetPassByIndex(0)->GetDesc(&PassDesc);
	V_CHECK(m_pd3dDevice->CreateInputLayout(decl,3,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl));

	ID3D11InputLayout* previousInputLayout;
	D3D10_PRIMITIVE_TOPOLOGY previousTopology;
	pImmediateContext->IAGetInputLayout(&previousInputLayout);
	pImmediateContext->IAGetPrimitiveTopology(&previousTopology);
	{
		pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );
		pImmediateContext->IASetInputLayout(m_pVtxDecl);
		vertexBuffer.setAsStreamOutTarget(pImmediateContext);
		ApplyPass(pImmediateContext,effect->GetTechniqueByName("init_particles")->GetPassByIndex(0));
		pImmediateContext->Draw(125000,0);
		ID3D11Buffer *pBuffer =NULL;
		UINT offset=0;
		pImmediateContext->SOSetTargets(1,&pBuffer,&offset);
	}
	pImmediateContext->IASetPrimitiveTopology(previousTopology );
	pImmediateContext->IASetInputLayout(previousInputLayout);

	SAFE_RELEASE(pImmediateContext);
}

void PrecipitationRenderer::SetCubemapTexture(void *t)
{
	cubemap_SRV=(ID3D11ShaderResourceView*)t;
}

void PrecipitationRenderer::SetRandomTexture3D(void *t)
{
	randomTexture3D=(ID3D11ShaderResourceView*)t;
}

void *PrecipitationRenderer::GetMoistureTexture()
{
	return moisture_fb.GetColorTex();
}

void PrecipitationRenderer::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(ID3D11Device*)dev;
	HRESULT hr=S_OK;
	cam_pos.x=cam_pos.y=cam_pos.z=0;
	MakeMesh();

	PrecipitationVertex *dat=new PrecipitationVertex[125000];
	memset(dat,0,125000);
	for(int i=0;i<125000;i++)
	{
		dat[i].position.y=6;
		dat[i].position.x=10*(i/125000.0f-.5f);
	}
	
	vertexBuffer.ensureBufferSize(m_pd3dDevice,125000,dat,true,false);
	vertexBufferSwap.ensureBufferSize(m_pd3dDevice,125000,dat,true,false);
	delete dat;

    RecompileShaders();
	
	rainConstants.RestoreDeviceObjects(m_pd3dDevice);
	perViewConstants.RestoreDeviceObjects(m_pd3dDevice);
	moisturePerViewConstants.RestoreDeviceObjects(m_pd3dDevice);
	

	moisture_fb.RestoreDeviceObjects(m_pd3dDevice);
	moisture_fb.SetWidthAndHeight(512,512);
	moisture_fb.SetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
}

void PrecipitationRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	cubemap_SRV=NULL;
	vertexBuffer.release();
	vertexBufferSwap.release();
	rainArrayTexture.release();
	SAFE_RELEASE(effect);
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(rain_texture);
	
	rainConstants.InvalidateDeviceObjects();
	perViewConstants.InvalidateDeviceObjects();
	moisturePerViewConstants.InvalidateDeviceObjects();
	moisture_fb.InvalidateDeviceObjects();
}

PrecipitationRenderer::~PrecipitationRenderer()
{
	InvalidateDeviceObjects();
}

void PrecipitationRenderer::PreRenderUpdate(void *context,float dt)
{
	if(dt<0)
		dt*=-1.0f;
	if(dt>1.0f)
		dt=1.0f;
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	BasePrecipitationRenderer::PreRenderUpdate(context,dt);
	
	rainConstants.meanFallVelocity	=meanVelocity;
	rainConstants.timeStepSeconds	=dt;
	rainConstants.Apply(pContext);
	
	ID3D11InputLayout* previousInputLayout;
	D3D10_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetInputLayout(&previousInputLayout);
	pContext->IAGetPrimitiveTopology(&previousTopology);
	{
		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		pContext->IASetInputLayout(m_pVtxDecl);
		vertexBuffer.apply(pContext,0);
		vertexBufferSwap.setAsStreamOutTarget(pContext);
		techniqueMoveParticles->GetPassByIndex(0)->Apply(0,pContext);
		ApplyPass(pContext,effect->GetTechniqueByName("move_particles")->GetPassByIndex(0));
		pContext->Draw(125000,0);
		ID3D11Buffer *pBuffer =NULL;
		UINT offset=0;
		pContext->SOSetTargets(1,&pBuffer,&offset );
		std::swap(vertexBuffer,vertexBufferSwap);
	}
	pContext->IASetPrimitiveTopology(previousTopology );
	pContext->IASetInputLayout(previousInputLayout);
}

// Render an image representing the optical thickness of moisture in any direction within a given view.
// The depth texture provides the distance to any solid object.
// The CloudShadowStruct tells us how much cloud is in any given 2D direction.
void PrecipitationRenderer::RenderMoisture(void *context
				,const DepthTextureStruct &depthStruct
				,const ViewStruct &viewStruct
				,const CloudShadowStruct &cloudShadowStruct)
{
	if(viewStruct.view_id!=0)
		return;
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	SIMUL_COMBINED_PROFILE_START(context,"Moisture")
	moisture_fb.SetWidthAndHeight(512,512);
	moisture_fb.Activate(context);
	{
		SetMoistureConstants(moisturePerViewConstants,depthStruct,viewStruct);
		moisturePerViewConstants.Apply(pContext);
		simul::dx11::setTexture(effect,"depthTexture"	,(ID3D11ShaderResourceView*)depthStruct.depth_tex);
		ID3DX11EffectTechnique *tech=effect->GetTechniqueByName("moisture");
		tech->GetPassByIndex(0)->Apply(0,pContext);
		simul::dx11::UtilityRenderer::DrawQuad(pContext);
	}
	moisture_fb.Deactivate(context);
	SIMUL_COMBINED_PROFILE_END(context)
}

/*	Here, we only consider the effect of exposure time on the transparency of the rain streak. It has been
shown [Garg and Nayar 2005] that the intensity Ir at a rain-affected
pixel is given by Ir =(1-a) Ib+a Istreak, where Ib and Istreak are the
intensities of the background and the streak at a rain-effected pixel.
The transparency factor a depends on drop size r0 and velocity v
and is given by a = 2 r0/v Texp. Also, the rain streaks become more
opaque with shorter exposure time. In the last step, we use the userspecified
depth map of the scene to find the pixels for which the rain
streak is not occluded by the scene. The streak is rendered only over
those pixels.
*/
void PrecipitationRenderer::Render(void *context
				,const void *depth_tex
				,const simul::math::Matrix4x4 &view
				,const simul::math::Matrix4x4 &proj
				,float max_fade_distance_metres
				,simul::sky::float4 viewportTextureRegionXYWH)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	static float cc=0.02f;
	intensity*=1.f-cc;
	intensity+=cc*Intensity;
	intensity=Intensity;
	if(intensity<=0.01)
		return;
	SIMUL_COMBINED_PROFILE_START(context,"Rain Overlay")
	rainTexture->SetResource(rain_texture);
	dx11::setTexture(effect,"cubeTexture",cubemap_SRV);
	dx11::setTexture(effect,"randomTexture3D",randomTexture3D);
	dx11::setTexture(effect,"depthTexture",(ID3D11ShaderResourceView*)depth_tex);
	dx11::setTextureArray(effect,"rainTextureArray",rainArrayTexture.m_pArrayTexture_SRV);
	
	//set up matrices
	D3DXMATRIX world,wvp;
	D3DXMatrixIdentity(&world);
	vec3 viewPos			=simul::dx11::GetCameraPosVector(view,false);
	simul::math::Matrix4x4 v=view;
	v._41=v._42=v._43=0;
	camera::MakeCentredViewProjMatrix((float*)&wvp,view,proj);
	
	rainConstants.lightColour	=(const float*)light_colour;
	rainConstants.lightDir		=(const float*)light_dir;
	rainConstants.phase			=Phase;
	rainConstants.flurry		=Waver;
	rainConstants.flurryRate	=1.0f;
	rainConstants.snowSize		=0.05f;

	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)proj);
	simul::math::Matrix4x4 p1=proj;
	if(ReverseDepth)
	{
		// Convert the proj matrix into a normal non-reversed matrix.
		p1=proj;//simul::dx11::ConvertReversedToRegularProjectionMatrix(proj);
		simul::camera::ConvertReversedToRegularProjectionMatrix(p1);
	}
	simul::math::Matrix4x4 vpt,viewproj,p((const float*)p1);

	simul::math::Multiply4x4(viewproj,v,p);
	viewproj.Transpose(vpt);
	simul::math::Matrix4x4 ivp;
	vpt.Inverse(ivp);
	if(view_initialized)
	{
		perViewConstants.invViewProj_2[0]	=perViewConstants.invViewProj_2[1];
		perViewConstants.worldViewProj[0]	=perViewConstants.worldViewProj[1];
		perViewConstants.worldView[0]		=perViewConstants.worldView[1];
		perViewConstants.offset[0]			=perViewConstants.offset[1];
		perViewConstants.viewPos[0]			=perViewConstants.viewPos[1];
	}

	perViewConstants.invViewProj_2[1]		=ivp;
	perViewConstants.invViewProj_2[1].transpose();
	perViewConstants.worldView[1]		=view;
	perViewConstants.worldView[1].transpose();
	perViewConstants.worldViewProj[1]	=wvp;
	perViewConstants.worldViewProj[1].transpose();
	perViewConstants.offset[1]		=offs;
	perViewConstants.tanHalfFov		=vec2(frustum.tanHalfHorizontalFov,frustum.tanHalfVerticalFov);
	perViewConstants.nearZ			=0;//frustum.nearZ*0.001f/fade_distance_km;
	perViewConstants.farZ			=0;//frustum.farZ*0.001f/fade_distance_km;
	perViewConstants.viewPos[1]		=viewPos;
	
	if(!view_initialized)
	{
		perViewConstants.invViewProj_2[0]		=perViewConstants.invViewProj_2[1];
		perViewConstants.worldViewProj[0]	=perViewConstants.worldViewProj[1];
		perViewConstants.worldView[0]		=perViewConstants.worldView[1];
		perViewConstants.offset[0]			=perViewConstants.offset[1];
		perViewConstants.viewPos[0]			=perViewConstants.viewPos[1];
		view_initialized=true;
	}
	// enforce the maximum offset for viewPos
	sky::float4 pos0=perViewConstants.viewPos[0];
	sky::float4 pos1=perViewConstants.viewPos[1];
	sky::float4 diff=pos1-pos0;
	float dist=sky::length(diff);
	if(dist>1.0)
	{
		pos0=pos1-diff/dist;
		perViewConstants.viewPos[0]=pos0;
	}

	static float near_rain_distance_metres=250.f;
	perViewConstants.nearRainDistance=near_rain_distance_metres/max_fade_distance_metres;
	perViewConstants.depthToLinFadeDistParams = simul::math::Vector3( proj.m[3][2], max_fade_distance_metres, proj.m[2][2]*max_fade_distance_metres );
	
	perViewConstants.viewportToTexRegionScaleBias = simul::sky::float4(viewportTextureRegionXYWH.z, viewportTextureRegionXYWH.w, viewportTextureRegionXYWH.x, viewportTextureRegionXYWH.y);

	perViewConstants.Apply(pContext);
	{
		rainConstants.Apply(pContext);
		UINT passes=1;
		for(unsigned i = 0 ; i < passes ; ++i )
		{
			ApplyPass(pContext,m_hTechniqueRain->GetPassByIndex(i));
		//	simul::dx11::UtilityRenderer::DrawQuad(pContext);
		}
	}
	SIMUL_COMBINED_PROFILE_END(context)
	SIMUL_COMBINED_PROFILE_START(context,"Rain/snow Particles")
	//if(RainToSnow>0)
	{
		RenderParticles(pContext);
	}
	simul::dx11::setTexture(effect,"cubeTexture"		,NULL);
	simul::dx11::setTexture(effect,"randomTexture3D"	,NULL);
	simul::dx11::setTexture(effect,"depthTexture"		,NULL);
	dx11::setTextureArray(	effect,"rainTextureArray"	,NULL);
	ApplyPass(pContext,m_hTechniqueRain->GetPassByIndex(0));
	SIMUL_COMBINED_PROFILE_END(context)
}

void PrecipitationRenderer::RenderParticles(void *context)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	ID3D11InputLayout* previousInputLayout;
	pContext->IAGetInputLayout(&previousInputLayout);
	pContext->IASetInputLayout(m_pVtxDecl);
	D3D10_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);
	rainConstants.intensity		=intensity;
	rainConstants.Apply(pContext);
	int numParticles			=(int)(intensity*125000.f);
	vertexBuffer.apply(pContext,0);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	if(RainToSnow>.5)
		ApplyPass(pContext,m_hTechniqueParticles->GetPassByIndex(0));
	else
		ApplyPass(pContext,m_hTechniqueRainParticles->GetPassByIndex(0));
	pContext->Draw(numParticles,0);
	pContext->IASetPrimitiveTopology(previousTopology);
	pContext->IASetInputLayout(previousInputLayout);
	SAFE_RELEASE(previousInputLayout);
}

void PrecipitationRenderer::RenderTextures(void *context,int x0,int y0,int dx,int dy)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
	HRESULT hr=S_OK;
	static int u=1;
	int w=(dx-8)/u;
	int h=w/8;
	simul::dx11::setTexture(effect,"showTexture",rain_texture);
	UtilityRenderer::DrawQuad2(pContext,x0,y0+dy-(h+8),w,h,effect,effect->GetTechniqueByName("show_texture"));
	simul::dx11::setTexture(effect,"showTexture",NULL);

	w=moisture_fb.Width;
	h=moisture_fb.Height;
	while(w>dx||h>dy)
	{
		w/=2;
		h/=2;
	}
	renderPlatformDx11.DrawTexture(pContext	,x0,y0	,w,h,(ID3D11ShaderResourceView*)moisture_fb.GetColorTex(),1.f);
	UtilityRenderer::Print(pContext			,x0,y0	,"Moisture");
}
