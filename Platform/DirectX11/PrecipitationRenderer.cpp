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
#include "Simul/Base/StringFunctions.h"
#include "Simul/Base/ProfilingInterface.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Sky/SkyInterface.h"
#include "D3dx11effect.h"
#pragma optimize("",off)
using namespace simul;
using namespace dx11;
static float length(vec3 v)
{
	return sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
}

PrecipitationRenderer::PrecipitationRenderer() :
	m_pd3dDevice(NULL)
	,m_pVtxDecl(NULL)
	,view_initialized(false)
	,last_cam_pos(0.0f,0.0f,0.0f)
{
}

void PrecipitationRenderer::RecompileShaders()
{
	if(!m_pd3dDevice)
		return;
	std::vector<crossplatform::EffectDefineOptions> opts;
	opts.push_back(crossplatform::CreateDefineOptions("REVERSE_DEPTH","0","1"));
	renderPlatform->EnsureEffectIsBuilt("rain",opts);
	SAFE_DELETE(effect);
	std::map<std::string,std::string> defines;
	defines["REVERSE_DEPTH"]	=ReverseDepth?"1":"0";
	effect=renderPlatform->CreateEffect("rain",defines);
	SAFE_DELETE(rain_texture);
	m_hTechniqueRain			=effect->GetTechniqueByName("simul_rain");
	m_hTechniqueParticles		=effect->GetTechniqueByName("snow");
	m_hTechniqueRainParticles	=effect->GetTechniqueByName("rain_particles");
	techniqueMoveParticles		=effect->GetTechniqueByName("move_particles");
	rainTexture					=effect->asD3DX11Effect()->GetVariableByName("rainTexture")->AsShaderResource();
	rainConstants.LinkToEffect(effect,"RainConstants");
	perViewConstants.LinkToEffect(effect,"RainPerViewConstants");
	moisturePerViewConstants.LinkToEffect(effect,"MoisturePerViewConstants");

	ID3D11DeviceContext *pImmediateContext=NULL;
	m_pd3dDevice->GetImmediateContext(&pImmediateContext);
	crossplatform::DeviceContext deviceContext;
	deviceContext.platform_context		=pImmediateContext;
	deviceContext.renderPlatform		=renderPlatform;
	crossplatform::EffectTechnique *tech		=effect->GetTechniqueByName("create_rain_texture");
	effect->Apply(deviceContext,tech,0);
	rain_texture=renderPlatform->CreateTexture();
	rain_texture->ensureTexture2DSizeAndFormat(renderPlatform,512,64,crossplatform::PixelFormat::RGBA_32_FLOAT,false,true);
	

	rain_texture->activateRenderTarget(deviceContext);
	renderPlatform->DrawQuad(deviceContext);
	rain_texture->deactivateRenderTarget();
	effect->Unapply(deviceContext);

	view_initialized=false;
	// The array of rain textures:
	rainArrayTexture.create(m_pd3dDevice,16,512,32,DXGI_FORMAT_R32G32B32A32_FLOAT,true);
	// Use a compute shader to initialize the rain texture:
	// shader has been created:
	dx11::setUnorderedAccessView(effect->asD3DX11Effect(),"targetTextureArray",rainArrayTexture.unorderedAccessView);
	effect->Apply(deviceContext,effect->GetTechniqueByName("make_rain_texture_array"),0);

	pImmediateContext->Dispatch(16,512,32);

	// We can't detect if this has worked or not.
	pImmediateContext->GenerateMips(rainArrayTexture.m_pArrayTexture_SRV);
	effect->Unapply(deviceContext);

	D3D11_INPUT_ELEMENT_DESC decl[] =
	{
		{"POSITION"	,0	,DXGI_FORMAT_R32G32B32_FLOAT	,0	,0	,D3D11_INPUT_PER_VERTEX_DATA,0},
		{"TYPE"		,0	,DXGI_FORMAT_R32_UINT			,0	,12	,D3D11_INPUT_PER_VERTEX_DATA,0},
		{"VELOCITY"	,0	,DXGI_FORMAT_R32G32B32_FLOAT	,0	,16	,D3D11_INPUT_PER_VERTEX_DATA,0},
		//{"DUMMY"	,0	,DXGI_FORMAT_R32_FLOAT			,0	,28	,D3D11_INPUT_PER_VERTEX_DATA,0},
	};
	SAFE_RELEASE(m_pVtxDecl);
    D3DX11_PASS_DESC PassDesc;
	m_hTechniqueRainParticles->asD3DX11EffectTechnique()->GetPassByIndex(0)->GetDesc(&PassDesc);
	V_CHECK(m_pd3dDevice->CreateInputLayout(decl,3,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl));
#if 1
	ID3D11InputLayout* previousInputLayout;
	D3D_PRIMITIVE_TOPOLOGY previousTopology;
	pImmediateContext->IAGetInputLayout(&previousInputLayout);
	pImmediateContext->IAGetPrimitiveTopology(&previousTopology);
	{
		pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );
		pImmediateContext->IASetInputLayout(m_pVtxDecl);
		vertexBufferSwap.apply(pImmediateContext,0);
		vertexBuffer.setAsStreamOutTarget(pImmediateContext);
		effect->Apply(deviceContext,effect->GetTechniqueByName("init_particles"),0);
		pImmediateContext->Draw(125000,0);
		cancelStreamOutTarget(pImmediateContext);
		effect->Unapply(deviceContext);
	}
	pImmediateContext->IASetPrimitiveTopology(previousTopology );
	pImmediateContext->IASetInputLayout(previousInputLayout);
#endif
	SAFE_RELEASE(pImmediateContext);
}

void *PrecipitationRenderer::GetMoistureTexture()
{
	return moisture_fb.GetColorTex();
}
#include "Simul/Math/RandomNumberGenerator.h"
void PrecipitationRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	m_pd3dDevice=renderPlatform->AsD3D11Device();
	HRESULT hr=S_OK;
	MakeMesh();

	{
		PrecipitationVertex *dat=new PrecipitationVertex[125000];
		memset(dat,0,125000);
		for(int i=0;i<125000;i++)
		{
			dat[i].position.y=6;
			dat[i].position.x=10*(i/125000.0f-.5f);
		}
	
		vertexBuffer.ensureBufferSize(m_pd3dDevice,125000,dat,true,false);
		vertexBufferSwap.ensureBufferSize(m_pd3dDevice,125000,dat,true,false);

		delete [] dat;
	}
	{
		SplashVertex *splash_data=new SplashVertex[10000];
		memset(splash_data,0,10000);
	/*	for(int i=0;i<10000;i++)
		{
			simul::math::RandomNumberGenerator rnd;
			splash_data[i].position.x=rnd.FRand(0.f,100.f);
			splash_data[i].position.y=rnd.FRand(0.f,100.f);
			splash_data[i].position.z=800.0f;
			splash_data[i].normal.x=0;
			splash_data[i].normal.y=0;
			splash_data[i].normal.z=1.0f;
			splash_data[i].strength=1.0f;
		}*/
		splashBuffer.ensureBufferSize(m_pd3dDevice,10000,splash_data,true,false);
		splashBufferSwap.ensureBufferSize(m_pd3dDevice,10000,splash_data,true,false);
		delete [] splash_data;
	}
	rainConstants.RestoreDeviceObjects(renderPlatform);
	perViewConstants.RestoreDeviceObjects(renderPlatform);
	moisturePerViewConstants.RestoreDeviceObjects(renderPlatform);
	
	moisture_fb.RestoreDeviceObjects(renderPlatform);
	moisture_fb.SetWidthAndHeight(512,512);
	moisture_fb.SetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);

    RecompileShaders();
}

void PrecipitationRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	cubemapTexture=NULL;
	vertexBuffer.release();
	vertexBufferSwap.release();
		splashBuffer.release();
		splashBufferSwap.release();
	rainArrayTexture.release();
	SAFE_DELETE(effect);
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_DELETE(rain_texture);
	
	rainConstants.InvalidateDeviceObjects();
	perViewConstants.InvalidateDeviceObjects();
	moisturePerViewConstants.InvalidateDeviceObjects();
	moisture_fb.InvalidateDeviceObjects();
	BasePrecipitationRenderer::InvalidateDeviceObjects();
}

PrecipitationRenderer::~PrecipitationRenderer()
{
	InvalidateDeviceObjects();
}

void PrecipitationRenderer::PreRenderUpdate(crossplatform::DeviceContext &deviceContext,float dt)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"PrecipitationRenderer::PreRenderUpdate")
	if(dt<0)
		dt*=-1.0f;
	if(dt>1.0f)
		dt=1.0f;
	BasePrecipitationRenderer::PreRenderUpdate(deviceContext,dt);
	math::Vector3 cam_pos=simul::camera::GetCameraPosVector(deviceContext.viewStruct.view);
	if(last_cam_pos.Magnitude()==0.0f)
		last_cam_pos=cam_pos;
	rainConstants.rainMapMatrix		=rainMapMatrix;
	rainConstants.meanFallVelocity	=meanVelocity;
	rainConstants.timeStepSeconds	=dt;
	rainConstants.viewPositionOffset=cam_pos-last_cam_pos;
	float l=length(rainConstants.viewPositionOffset);
	static float max_offs=2.0f;
	if(l>max_offs)
	{
		rainConstants.viewPositionOffset*=max_offs/l;
	}
	last_cam_pos=cam_pos;

	rainConstants.Apply(deviceContext);
	
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	ID3D11InputLayout* previousInputLayout;
	D3D_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetInputLayout(&previousInputLayout);
	pContext->IAGetPrimitiveTopology(&previousTopology);
	{
		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		pContext->IASetInputLayout(m_pVtxDecl);
		vertexBuffer.apply(pContext,0);
		vertexBufferSwap.setAsStreamOutTarget(pContext);
		effect->Apply(deviceContext,techniqueMoveParticles,0);
		pContext->Draw(125000,0);
		effect->Unapply(deviceContext);
		ID3D11Buffer *pBuffer =NULL;
		UINT offset=0;
		pContext->SOSetTargets(1,&pBuffer,&offset );
		std::swap(vertexBuffer,vertexBufferSwap);
	}
	pContext->IASetPrimitiveTopology(previousTopology );
	pContext->IASetInputLayout(previousInputLayout);
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
}

// Render an image representing the optical thickness of moisture in any direction within a given view.
// The depth texture provides the distance to any solid object.
// The CloudShadowStruct tells us how much cloud is in any given 2D direction.
void PrecipitationRenderer::RenderMoisture(void *context
				,const DepthTextureStruct &depthStruct
				,const crossplatform::ViewStruct &viewStruct
				,const CloudShadowStruct &cloudShadowStruct)
{
	if(viewStruct.view_id!=0)
		return;
/*	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	SIMUL_COMBINED_PROFILE_START(context,"Moisture")
	moisture_fb.SetWidthAndHeight(512,512);
	moisture_fb.Activate(context);
	{
		SetMoistureConstants(moisturePerViewConstants,depthStruct,viewStruct);
		moisturePerViewConstants.Apply(deviceContext);
		simul::dx11::setTexture(effect,"depthTexture"	,(ID3D11ShaderResourceView*)depthStruct.depth_tex);
		ID3DX11EffectTechnique *tech=effect->GetTechniqueByName("moisture");
		tech->GetPassByIndex(0)->Apply(0,pContext);
		simul::dx11::UtilityRenderer::DrawQuad(pContext);
	}
	moisture_fb.Deactivate(context);
	SIMUL_COMBINED_PROFILE_END(context)*/
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
void PrecipitationRenderer::Render(crossplatform::DeviceContext &deviceContext
				,crossplatform::Texture *depth_tex
				,float max_fade_distance_metres
				,simul::sky::float4 viewportTextureRegionXYWH)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	math::Vector3 cam_pos=simul::camera::GetCameraPosVector(deviceContext.viewStruct.view);
	static float ll=0.07f;
	sky::float4 light_colour=ll*baseSkyInterface->GetLocalIrradiance(cam_pos.z/1000.f);
	sky::float4 light_dir=baseSkyInterface->GetDirectionToLight(cam_pos.z/1000.f);
   
	static float cc=0.02f;
	intensity*=1.f-cc;
	intensity+=cc*Intensity;
	intensity=Intensity;
	if(intensity<=0.01)
		return;
	float underRain=0.0f;
	rainTexture->SetResource(rain_texture->AsD3D11ShaderResourceView());
	effect->SetTexture(deviceContext,"cubeTexture",cubemapTexture);
	effect->SetTexture(deviceContext,"randomTexture3D",randomTexture3D);
	effect->SetTexture(deviceContext,"depthTexture",depth_tex);
	effect->SetTexture(deviceContext,"rainMapTexture",rainMapTexture);
	dx11::setTextureArray(effect->asD3DX11Effect(),"rainTextureArray",rainArrayTexture.m_pArrayTexture_SRV);
	
	//set up matrices
	D3DXMATRIX world,wvp;
	D3DXMatrixIdentity(&world);
	vec3 viewPos			=simul::dx11::GetCameraPosVector(deviceContext.viewStruct.view,false);
/*	SIMUL_COMBINED_PROFILE_START(pContext,"lookup")
	if(rainMapTexture)
	{
		math::Matrix4x4 mat=rainMapMatrix;
		math::Vector3 texc;
		math::Multiply4(texc,mat,(math::Vector3)viewPos);
		dx11::Texture *t=(dx11::Texture *)rainMapTexture;
		vec4 T=t->GetTexel(deviceContext,vec2(texc.x,texc.y),true);
		if(T.x<0.5f)
		{
			SIMUL_COMBINED_PROFILE_END(pContext)
			return;
		}
	}
	SIMUL_COMBINED_PROFILE_END(pContext)*/
	SIMUL_COMBINED_PROFILE_START(pContext,"PrecipitationRenderer")
	simul::math::Matrix4x4 v=deviceContext.viewStruct.view;
	v._41=v._42=v._43=0;
	camera::MakeCentredViewProjMatrix((float*)&wvp,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
	
	rainConstants.rainMapMatrix		=rainMapMatrix;
	rainConstants.lightColour	=(const float*)light_colour;
	rainConstants.lightDir		=(const float*)light_dir;
	rainConstants.phase			=Phase;
	rainConstants.flurry		=Waver;
	rainConstants.flurryRate	=1.0f;
	rainConstants.snowSize		=0.05f;

	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)deviceContext.viewStruct.proj);
	simul::math::Matrix4x4 p1=deviceContext.viewStruct.proj;
	if(ReverseDepth)
	{
		// Convert the proj matrix into a normal non-reversed matrix.
		p1=deviceContext.viewStruct.proj;//simul::dx11::ConvertReversedToRegularProjectionMatrix(proj);
		//simul::camera::ConvertReversedToRegularProjectionMatrix(p1);
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
	perViewConstants.worldView[1]		=deviceContext.viewStruct.view;
	perViewConstants.worldView[1].transpose();
	perViewConstants.worldViewProj[1]		=(const float *)&wvp;
	perViewConstants.worldViewProj[1].transpose();
	perViewConstants.offset[1]				=(const float*)cam_pos;
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

	static float near_rain_distance_metres			=250.0f;
	perViewConstants.nearRainDistance				=near_rain_distance_metres/max_fade_distance_metres;
	perViewConstants.splashDelta					=0.02f/max_fade_distance_metres;
	perViewConstants.depthToLinFadeDistParams		=camera::GetDepthToDistanceParameters(deviceContext.viewStruct,max_fade_distance_metres);
	
	perViewConstants.viewportToTexRegionScaleBias	=simul::sky::float4(viewportTextureRegionXYWH.z, viewportTextureRegionXYWH.w, viewportTextureRegionXYWH.x, viewportTextureRegionXYWH.y);

	perViewConstants.Apply(deviceContext);
	{
		rainConstants.Apply(deviceContext);
		UINT passes=1;
		for(unsigned i = 0 ; i < passes ; ++i )
		{
			//ApplyPass(pContext,m_hTechniqueRain->GetPassByIndex(i));
		//	simul::dx11::UtilityRenderer::DrawQuad(pContext);
//effect->Unapply(deviceContext);
		}
	}
	SIMUL_COMBINED_PROFILE_END(pContext)
	//if(RainToSnow>0)
	{
		RenderParticles(deviceContext);
	}
	simul::dx11::setTexture(effect->asD3DX11Effect(),"cubeTexture"		,NULL);
	simul::dx11::setTexture(effect->asD3DX11Effect(),"randomTexture3D"	,NULL);
	simul::dx11::setTexture(effect->asD3DX11Effect(),"depthTexture"		,NULL);
	dx11::setTextureArray(	effect->asD3DX11Effect(),"rainTextureArray"	,NULL);
}

void PrecipitationRenderer::RenderParticles(crossplatform::DeviceContext &deviceContext)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"PrecipitationRenderer Particles")
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	ID3D11InputLayout* previousInputLayout;
	pContext->IAGetInputLayout(&previousInputLayout);
	pContext->IASetInputLayout(m_pVtxDecl);
	D3D_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);
	rainConstants.intensity		=intensity;
	rainConstants.Apply(deviceContext);
	int numParticles			=(int)(intensity*125000.f);
	vertexBuffer.apply(pContext,0);
	effect->SetTexture(deviceContext,"rainMapTexture",rainMapTexture);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	if(RainToSnow>.5)
		effect->Apply(deviceContext,m_hTechniqueParticles,0);
	else
		effect->Apply(deviceContext,m_hTechniqueRainParticles,0);
	pContext->Draw(numParticles,0);
	effect->SetTexture(deviceContext,"rainMapTexture",NULL);
	effect->Unapply(deviceContext);
	{
		// Splashes!
		//splashBuffer.apply(pContext,0);
	//	effect->Apply(deviceContext,effect->GetTechniqueByName("splash"),0);
	//	pContext->Draw(10000,0);
	//	effect->Unapply(deviceContext);
	}
	pContext->IASetPrimitiveTopology(previousTopology);
	pContext->IASetInputLayout(previousInputLayout);
	SAFE_RELEASE(previousInputLayout);
	SIMUL_COMBINED_PROFILE_END(pContext)
}

void PrecipitationRenderer::RenderTextures(crossplatform::DeviceContext &deviceContext,int x0,int y0,int dx,int dy)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)deviceContext.asD3D11DeviceContext();
	HRESULT hr=S_OK;
	static int u=1;
	int w=(dx-8)/u;
	int h=w/8;
	//renderPlatform->DrawTexture(deviceContext,x0,y0+dy-(h+8),w,h,rain_texture);

	w=moisture_fb.Width;
	h=moisture_fb.Height;
	while(w>dx||h>dy)
	{
		w/=2;
		h/=2;
	}
//	deviceContext.renderPlatform->DrawTexture(deviceContext	,x0,y0	,w,h,moisture_fb.GetTexture(),1.f);
	//deviceContext.renderPlatform->Print(deviceContext	,x0,y0	,"Moisture");
	
	deviceContext.renderPlatform->DrawTexture(deviceContext	,x0				,y0+dy-w		,w,w		,rainMapTexture);
	deviceContext.renderPlatform->Print(deviceContext		,x0				,y0+dy-w		,"rain_map");
	
		math::Matrix4x4 mat=rainMapMatrix;
		math::Vector3 texc;
		math::Multiply4(texc,mat,(math::Vector3)last_cam_pos);
		deviceContext.renderPlatform->Print(deviceContext	,x0,y0	,simul::base::stringFormat("%4.4f %4.4f",texc.x,texc.y).c_str());
}
