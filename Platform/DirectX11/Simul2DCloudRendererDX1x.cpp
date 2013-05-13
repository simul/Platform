// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// Simul2DCloudRendererDX11.cpp A renderer for 2D cloud layers.

#include "Simul2DCloudRendererdx1x.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/FastCloudNode.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/FadeTableInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/Pi.h"
#include "Simul/LicenseKey.h"
#include "CreateEffectDX1x.h"


Simul2DCloudRendererDX11::Simul2DCloudRendererDX11(simul::clouds::CloudKeyframer *ck) :
	simul::clouds::Base2DCloudRenderer(ck)
	,m_pd3dDevice(NULL)
	,effect(NULL)
{
}

Simul2DCloudRendererDX11::~Simul2DCloudRendererDX11()
{
	InvalidateDeviceObjects();
}

void Simul2DCloudRendererDX11::RestoreDeviceObjects(void* dev)
{
	m_pd3dDevice=(ID3D11Device*)dev;
    RecompileShaders();
}

void Simul2DCloudRendererDX11::RecompileShaders()
{
	SAFE_RELEASE(effect);
	if(!m_pd3dDevice)
		return;
	CreateEffect(m_pd3dDevice,&effect,L"simul_clouds_2d.fx");
	tech=effect->GetTechniqueByName("simul_clouds_2d");
}

void Simul2DCloudRendererDX11::InvalidateDeviceObjects()
{
	SAFE_RELEASE(effect);
	m_pd3dDevice=NULL;
}

void Simul2DCloudRendererDX11::EnsureCorrectTextureSizes()
{
}

void Simul2DCloudRendererDX11::EnsureTexturesAreUpToDate(void*)
{
}

void Simul2DCloudRendererDX11::EnsureTextureCycle()
{
}

void Simul2DCloudRendererDX11::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}

bool Simul2DCloudRendererDX11::Render(void*,bool cubemap,void *depth_tex,bool default_fog,bool write_alpha)
{
	D3DXMATRIX wvp;
	D3DXMATRIX world;
	D3DXMatrixIdentity(&world);
	simul::dx11::MakeWorldViewProjMatrix(&wvp,world,view,proj);
	//worldViewProj->SetMatrix(&wvp._11);
	simul::dx11::setParameter(effect,"worldViewProj",&wvp._11);
	
	const std::vector<int> &quad_strip_vertices=helper->GetQuadStripIndices();
	size_t qs_vert=0;
	for(std::vector<simul::clouds::Cloud2DGeometryHelper::QuadStrip>::const_iterator j=helper->GetQuadStrips().begin();
		j!=helper->GetQuadStrips().end();j++)
	{
		for(size_t k=0;k<(j)->num_vertices;k++,qs_vert++)
		{
			const simul::clouds::Cloud2DGeometryHelper::Vertex &V=helper->GetVertices()[quad_strip_vertices[qs_vert]];
		/*	glMultiTexCoord2f(GL_TEXTURE0,V.cloud_tex_x,V.cloud_tex_y);
			glMultiTexCoord2f(GL_TEXTURE3,V.noise_tex_x,V.noise_tex_y);
			glMultiTexCoord2f(GL_TEXTURE4,(V.x+wind_offset.x)/image_scale,(V.y+wind_offset.y)/image_scale);
			
			glVertex3f(V.x,V.y,V.z);*/
		}
	}
return true;
}

void Simul2DCloudRendererDX11::RenderCrossSections(void*,int width,int height)
{
}

void Simul2DCloudRendererDX11::SetLossTexture(void *l)
{
}

void Simul2DCloudRendererDX11::SetInscatterTextures(void *i,void *s)
{
}

void Simul2DCloudRendererDX11::SetWindVelocity(float x,float y)
{
}
