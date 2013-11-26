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

using namespace simul;
using namespace dx11;

PrecipitationRenderer::PrecipitationRenderer() :
	m_pd3dDevice(NULL)
	,m_pVtxDecl(NULL)
	,m_pVertexBuffer(NULL)
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
	m_hTechniqueRain		=effect->GetTechniqueByName("simul_rain");
	m_hTechniqueParticles	=effect->GetTechniqueByName("simul_particles");
	rainTexture				=effect->GetVariableByName("rainTexture")->AsShaderResource();

	rainConstants.LinkToEffect(effect,"RainConstants");
	perViewConstants.LinkToEffect(effect,"RainPerViewConstants");
	
	ID3D11DeviceContext *m_pImmediateContext=NULL;
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);

	ID3DX11EffectTechnique *tech		=effect->GetTechniqueByName("create_rain_texture");
	ApplyPass(m_pImmediateContext,tech->GetPassByIndex(0));
	simul::dx11::Framebuffer make_rain_fb(512,64);
	make_rain_fb.RestoreDeviceObjects(m_pd3dDevice);
	make_rain_fb.Activate(m_pImmediateContext);
	make_rain_fb.DrawQuad(m_pImmediateContext);
	make_rain_fb.Deactivate(m_pImmediateContext);
	rain_texture=make_rain_fb.buffer_texture_SRV;
	// Make sure it isn't destroyed when the fb goes out of scope:
	rain_texture->AddRef();
	SAFE_RELEASE(m_pImmediateContext);
	view_initialized=false;
}

void PrecipitationRenderer::SetCubemapTexture(void *t)
{
	cubemap_SRV=(ID3D11ShaderResourceView*)t;
}

void PrecipitationRenderer::SetRandomTexture3D(void *t)
{
	randomTexture3D=(ID3D11ShaderResourceView*)t;
}

void PrecipitationRenderer::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(ID3D11Device*)dev;
	HRESULT hr=S_OK;
	cam_pos.x=cam_pos.y=cam_pos.z=0;
	view.Identity();
	proj.Identity();
	MakeMesh();
    RecompileShaders();
	D3D11_INPUT_ELEMENT_DESC decl[] =
	{
		{"POSITION"	,0	,DXGI_FORMAT_R32G32B32_FLOAT	,0	,0	,D3D11_INPUT_PER_VERTEX_DATA,0},
	};
	SAFE_RELEASE(m_pVtxDecl);
    D3D1x_PASS_DESC PassDesc;
	m_hTechniqueParticles->GetPassByIndex(0)->GetDesc(&PassDesc);
	hr=m_pd3dDevice->CreateInputLayout(decl,1,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
/*	D3D1x_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
	particles=new vec3[64000];
	simul::math::RandomNumberGenerator random;
	for(int i=0;i<64000;i++)
	{
		vec3 pos(2.f*random.FRand()-1.f,2.f*random.FRand()-1.f,2.f*random.FRand()-1.f);
		particles[i]=pos;
	}
    InitData.pSysMem		= particles;
    InitData.SysMemPitch	= sizeof(vec3);*/
	D3D11_BUFFER_DESC desc	=
	{
        64000*sizeof(vec3),
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_VERTEX_BUFFER|D3D11_BIND_UNORDERED_ACCESS
        ,0// CPU
		,0//D3D11_RESOURCE_MISC_BUFFER_STRUCTURED
		,sizeof(vec3)			//StructureByteStride
	};
	SAFE_RELEASE(m_pVertexBuffer);
	V_CHECK(m_pd3dDevice->CreateBuffer(&desc,NULL,&m_pVertexBuffer));
	
	// Use a compute shader to initialize the vertex buffer with random positions
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		ZeroMemory(&uav_desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
		uav_desc.Format					=DXGI_FORMAT_R32_FLOAT;
		uav_desc.ViewDimension			=D3D11_UAV_DIMENSION_BUFFER;
		uav_desc.Buffer.FirstElement	=0;
		uav_desc.Buffer.NumElements		=64000;
		uav_desc.Buffer.Flags			=0;
		ID3D11UnorderedAccessView* unorderedAccessView;
		V_CHECK(m_pd3dDevice->CreateUnorderedAccessView(m_pVertexBuffer, &uav_desc, &unorderedAccessView));
		ID3D11DeviceContext *m_pImmediateContext;
		m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
		// shader has been created:
		dx11::setUnorderedAccessView(effect,"targetVertexBuffer",unorderedAccessView);
		ApplyPass(m_pImmediateContext,effect->GetTechniqueByName("make_vertex_buffer")->GetPassByIndex(0));
		//40*40*40
		m_pImmediateContext->Dispatch(4,4,4);
		SAFE_RELEASE(unorderedAccessView);
		SAFE_RELEASE(m_pImmediateContext);
	}
	rainConstants.RestoreDeviceObjects(m_pd3dDevice);
	perViewConstants.RestoreDeviceObjects(m_pd3dDevice);
	//delete [] particles;
}


void PrecipitationRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	cubemap_SRV=NULL;
	SAFE_RELEASE(effect);
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(rain_texture);
	SAFE_RELEASE(m_pVertexBuffer);
	
	rainConstants.InvalidateDeviceObjects();
	perViewConstants.InvalidateDeviceObjects();
}

PrecipitationRenderer::~PrecipitationRenderer()
{
	InvalidateDeviceObjects();
}

/*
Here, we only consider the effect of exposure time on the transparency of the rain streak. It has been
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
void PrecipitationRenderer::Render(void *context,void *depth_tex,float max_fade_distance_metres,simul::sky::float4 viewportTextureRegionXYWH)
{
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext *)context;
	static float cc=0.006f;
	intensity*=1.f-cc;
	intensity+=cc*Intensity;
	if(intensity<=0.05)
		return;
	PIXBeginNamedEvent(0,"Render Precipitation");
	rainTexture->SetResource(rain_texture);
	simul::dx11::setTexture(effect,"cubeTexture",cubemap_SRV);
	simul::dx11::setTexture(effect,"randomTexture3D",randomTexture3D);
	simul::dx11::setTexture(effect,"depthTexture",(ID3D11ShaderResourceView*)depth_tex);
	m_pImmediateContext->IASetInputLayout( m_pVtxDecl );
	//set up matrices
	D3DXMATRIX world,wvp;
	D3DXMatrixIdentity(&world);
	vec3 viewPos=simul::dx11::GetCameraPosVector(view,false);
	view._41=view._42=view._43=0;
	simul::dx11::MakeWorldViewProjMatrix(&wvp,world,view,proj);
	
	rainConstants.lightColour	=(const float*)light_colour;
	rainConstants.lightDir		=(const float*)light_dir;
	rainConstants.phase			=Phase;
	rainConstants.flurry		=Waver;
	rainConstants.flurryRate	=1.0f;
	rainConstants.snowSize		=0.15f;

	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)proj);

	simul::math::Matrix4x4 p1=proj;
	if(ReverseDepth)
	{
		// Convert the proj matrix into a normal non-reversed matrix.
		p1=proj;//simul::dx11::ConvertReversedToRegularProjectionMatrix(proj);
		simul::camera::ConvertReversedToRegularProjectionMatrix(p1);
	}
	simul::math::Matrix4x4 vpt,viewproj,v((const float *)view),p((const float*)p1);

	simul::math::Multiply4x4(viewproj,v,p);
	viewproj.Transpose(vpt);
	simul::math::Matrix4x4 ivp;
	vpt.Inverse(ivp);
	if(view_initialized)
	{
		perViewConstants.invViewProj[0]		=perViewConstants.invViewProj[1];
		perViewConstants.worldViewProj[0]	=perViewConstants.worldViewProj[1];
		perViewConstants.offset[0]			=perViewConstants.offset[1];
		perViewConstants.viewPos[0]			=perViewConstants.viewPos[1];
	}

	perViewConstants.invViewProj[1]	=ivp;
	perViewConstants.invViewProj[1].transpose();
	perViewConstants.worldViewProj[1]	=wvp;
	perViewConstants.worldViewProj[1].transpose();
	perViewConstants.offset[1]		=offs;
	perViewConstants.tanHalfFov		=vec2(frustum.tanHalfHorizontalFov,frustum.tanHalfVerticalFov);
	perViewConstants.nearZ=0;//frustum.nearZ*0.001f/fade_distance_km;
	perViewConstants.farZ=0;//frustum.farZ*0.001f/fade_distance_km;
	perViewConstants.viewPos[1]		=viewPos;
	
	if(!view_initialized)
	{
		perViewConstants.invViewProj[0]		=perViewConstants.invViewProj[1];
		perViewConstants.worldViewProj[0]	=perViewConstants.worldViewProj[1];
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
	
	perViewConstants.Apply(m_pImmediateContext);

	if(RainToSnow<1.f)
	{
		rainConstants.intensity		=intensity*(1.f-RainToSnow);
		rainConstants.Apply(m_pImmediateContext);
		UINT passes=1;
		for(unsigned i = 0 ; i < passes ; ++i )
		{
			ApplyPass(m_pImmediateContext,m_hTechniqueRain->GetPassByIndex(i));
			simul::dx11::UtilityRenderer::DrawQuad(m_pImmediateContext);
		}
	}
	if(RainToSnow>0)
	{
		rainConstants.intensity		=intensity*(RainToSnow);
		rainConstants.Apply(m_pImmediateContext);
		RenderParticles(m_pImmediateContext);
	}
	simul::dx11::setTexture(effect,"cubeTexture",NULL);
	simul::dx11::setTexture(effect,"randomTexture3D",NULL);
	simul::dx11::setTexture(effect,"depthTexture",NULL);
	ApplyPass(m_pImmediateContext,m_hTechniqueRain->GetPassByIndex(0));
}

void PrecipitationRenderer::RenderParticles(void *context)
{
	ID3D11DeviceContext *m_pImmediateContext=(ID3D11DeviceContext *)context;
	ID3D11InputLayout* previousInputLayout;
	m_pImmediateContext->IAGetInputLayout( &previousInputLayout );

	m_pImmediateContext->IASetInputLayout( m_pVtxDecl );
	UINT stride = sizeof(vec3);
	UINT offset = 0;

	int numParticles=(int)(intensity*(RainToSnow)*64000.f);

	m_pImmediateContext->IASetVertexBuffers(	0,					// the first input slot for binding
												1,					// the number of buffers in the array
												&m_pVertexBuffer,	// the array of vertex buffers
												&stride,			// array of stride values, one for each buffer
												&offset );			// array of offset values, one for each buffer

	D3D10_PRIMITIVE_TOPOLOGY previousTopology;
	m_pImmediateContext->IAGetPrimitiveTopology(&previousTopology);
	m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	ApplyPass(m_pImmediateContext,m_hTechniqueParticles->GetPassByIndex(0));
	m_pImmediateContext->Draw(numParticles,0);

	m_pImmediateContext->IASetPrimitiveTopology(previousTopology);
	m_pImmediateContext->IASetInputLayout( previousInputLayout );
	SAFE_RELEASE(previousInputLayout);
}

void PrecipitationRenderer::SetMatrices(const simul::math::Matrix4x4 &v,const simul::math::Matrix4x4 &p)
{
	view=v;
	proj=p;
}

void PrecipitationRenderer::RenderTextures(void *context,int width,int height)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
	HRESULT hr=S_OK;
	static int u=1;
	int w=(width-8)/u;
	//if(w/8>height/3)
	//	w=8*height/3;
	int h=w/8;
	UtilityRenderer::SetScreenSize(width,height);
	simul::dx11::setTexture(effect,"showTexture",rain_texture);
	UtilityRenderer::DrawQuad2(pContext,width-(w+8),height-(h+8),w,h,effect,effect->GetTechniqueByName("show_texture"));
	//simul::dx11::setTexture(effect,"showTexture",random_SRV);
	//UtilityRenderer::DrawQuad2(pContext,width-(w+8),height-2*(h+8),h,h,effect,effect->GetTechniqueByName("show_texture"));
	simul::dx11::setTexture(effect,"showTexture",NULL);
}